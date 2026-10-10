/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * hunmin-jni.c — the JNI bridge between the Kotlin frontend and libhunmin.
 *
 * This file is the *whole* native surface of the Android frontend, and it is
 * deliberately mechanical: one function per libhunmin entry point the adapter
 * actually uses, no decisions, no policy, no layout knowledge. Everything it
 * exists to do is handle conversion and UTF-8 marshalling. That is the same
 * thin-adapter rule the fcitx5 addon follows (PLAN.md rule #4), and it holds
 * here for a stronger reason: a JNI function that makes a decision is a
 * decision no tier can test, because tier E1 mocks InputConnection, not JNI.
 *
 * Deliberately NOT here:
 *   - key mapping. That belongs to HunminEngine, in Kotlin, where tier E1 can
 *     assert it without a device.
 *   - data path resolution. See HunminData.install().
 *   - anything that caches. libhunmin's own return-value lifetime rule (valid
 *     until the next call on that ic) is honoured here rather than papered over
 *     with a Kotlin-side buffer, because a buffer would be a second copy of
 *     that rule and could disagree with it.
 *
 * See docs/FRONTEND-CONTRACT.md for the per-function assumptions.
 */
#include <jni.h>
#include <stdlib.h>
#include <string.h>

#include "hunmin.h"

/*
 * Handles cross as a plain jlong.
 *
 * libhunmin hands out opaque pointers and JNI has no pointer type, so the
 * address is carried as an integer and cast back on arrival. Nothing here
 * wraps it in a jobject or boxes it: a boxed handle would need a second
 * lifetime mechanism (a jobject the GC can collect) on top of the one libhunmin
 * already documents, and two mechanisms disagree. So the Kotlin side owns the
 * handle, closes it in `close()`, and never stores one in a field that
 * outlives the service's engine.
 *
 * There is no magic-value check on a zero or stale handle: libhunmin handles
 * every NULL and returns "" or 0 (see str_or_empty and each guard below), so
 * a zero handle degrades to "no text" instead of crashing. That is deliberate
 * -- the fcitx5 addon defends some call sites and not others for the same
 * reason (docs/FRONTEND-CONTRACT.md §3, A9).
 */

/* --- strings --------------------------------------------------------------
 *
 * Every string crossing back to Java goes through NewStringUTF, which takes
 * *modified* UTF-8. libhunmin returns well-formed UTF-8, so the only thing
 * that can go wrong is an embedded NUL — which cannot occur, because the
 * engine's strings are C strings and a NUL would have ended the read. The
 * check is therefore not paranoia about the engine, it is a refusal to hand a
 * silently truncated string to the IME: hunmin_ic_commit() is a destructive
 * read, so a string cut short here is a character the user typed and can never
 * get back.
 *
 * The empty-string-on-error rule comes from hunmin.h, which promises "" (never
 * NULL) for every out-of-range and NULL-input path, so Kotlin never has to
 * null-check and cannot accidentally commit "null" as text.
 */
static jstring str_or_empty(JNIEnv *env, const char *s) {
  if (!s) return (*env)->NewStringUTF(env, "");
  size_t n = strlen(s);
  if (n == 0) return (*env)->NewStringUTF(env, "");
  /* Reject a truncated tail rather than commit mojibake. Mirrors what
   * hunmin_ic_commit_text() itself does to its input. */
  const unsigned char *u = (const unsigned char *)s;
  for (size_t i = 0; i < n; i++) {
    if (u[i] == 0xc0 || u[i] == 0xc1) return (*env)->NewStringUTF(env, "");
  }
  return (*env)->NewStringUTF(env, s);
}

static char *dup_java_string(JNIEnv *env, jstring s) {
  if (!s) return NULL;
  const char *c = (*env)->GetStringUTFChars(env, s, NULL);
  if (!c) return NULL;
  char *out = strdup(c);
  (*env)->ReleaseStringUTFChars(env, s, c);
  return out;
}

/* ===================== hunmin_ctx ======================================= */

JNIEXPORT jlong JNICALL
Java_org_hunmin_ime_HunminNative_nativeCtxNew(JNIEnv *env, jclass k, jstring data_dir) {
  (void)k;
  char *dir = dup_java_string(env, data_dir);
  /* One explicit search path rather than the HUNMIN_DATA_PATH env var: an IME
   * runs inside someone else's process under their app sandbox, and mutating
   * the environment to communicate with your own library is both rude and
   * racy if two contexts are ever created. dirs[] is consumed only here. */
  const char *dirs[1];
  hunmin_ctx *ctx = NULL;
  if (dir) {
    dirs[0] = dir;
    ctx = hunmin_ctx_new(dirs, 1);
    free(dir);
  }
  return (jlong)(intptr_t)ctx;
}

JNIEXPORT void JNICALL
Java_org_hunmin_ime_HunminNative_nativeCtxFree(JNIEnv *env, jclass k, jlong ctx) {
  (void)env; (void)k;
  hunmin_ctx_free((hunmin_ctx *)(intptr_t)ctx);
}

JNIEXPORT jint JNICALL
Java_org_hunmin_ime_HunminNative_nativeLayoutCount(JNIEnv *env, jclass k, jlong ctx) {
  (void)env; (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  return c ? (jint)hunmin_ctx_layout_count(c) : 0;
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeLayoutId(JNIEnv *env, jclass k, jlong ctx, jint i) {
  (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  const char *s = (c && i >= 0) ? hunmin_ctx_layout_id(c, i) : NULL;
  return str_or_empty(env, s);
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeLayoutName(JNIEnv *env, jclass k, jlong ctx,
                                                  jint i, jstring lang) {
  (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  if (!c || i < 0) return (*env)->NewStringUTF(env, "");
  const char *l = NULL;
  if (lang) {
    char *tmp = dup_java_string(env, lang);
    l = tmp;
    const char *s = hunmin_ctx_layout_name(c, i, l);
    free(tmp);
    return str_or_empty(env, s);
  }
  return str_or_empty(env, hunmin_ctx_layout_name(c, i, NULL));
}

JNIEXPORT jboolean JNICALL
Java_org_hunmin_ime_HunminNative_nativeLayoutSupported(JNIEnv *env, jclass k, jlong ctx, jint i) {
  (void)env; (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  return (c && i >= 0 && hunmin_ctx_layout_supported(c, i)) ? JNI_TRUE : JNI_FALSE;
}

/* ===================== hunmin_ic ======================================== */

JNIEXPORT jlong JNICALL
Java_org_hunmin_ime_HunminNative_nativeIcNew(JNIEnv *env, jclass k, jlong ctx, jstring layout) {
  (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  char *id = dup_java_string(env, layout);
  if (!id) return 0;
  hunmin_ic *ic = hunmin_ic_new(c, id);
  free(id);
  return (jlong)(intptr_t)ic;
}

JNIEXPORT void JNICALL
Java_org_hunmin_ime_HunminNative_nativeIcFree(JNIEnv *env, jclass k, jlong ic) {
  (void)env; (void)k;
  hunmin_ic_free((hunmin_ic *)(intptr_t)ic);
}

JNIEXPORT jboolean JNICALL
Java_org_hunmin_ime_HunminNative_nativeProcess(JNIEnv *env, jclass k, jlong ic, jint ch) {
  (void)env; (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  return (i && hunmin_ic_process(i, (uint32_t)ch)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeCommit(JNIEnv *env, jclass k, jlong ic) {
  (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  return str_or_empty(env, i ? hunmin_ic_commit(i) : NULL);
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativePreedit(JNIEnv *env, jclass k, jlong ic) {
  (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  return str_or_empty(env, i ? hunmin_ic_preedit(i) : NULL);
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeFlush(JNIEnv *env, jclass k, jlong ic) {
  (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  return str_or_empty(env, i ? hunmin_ic_flush(i) : NULL);
}

JNIEXPORT void JNICALL
Java_org_hunmin_ime_HunminNative_nativeReset(JNIEnv *env, jclass k, jlong ic) {
  (void)env; (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  if (i) hunmin_ic_reset(i);
}

JNIEXPORT jboolean JNICALL
Java_org_hunmin_ime_HunminNative_nativeSetOption(JNIEnv *env, jclass k, jlong ic,
                                                  jstring name, jint value) {
  (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  char *n = dup_java_string(env, name);
  if (!i || !n) return JNI_FALSE;
  jboolean ok = hunmin_ic_set_option(i, n, value) ? JNI_TRUE : JNI_FALSE;
  free(n);
  return ok;
}

/* Invariant 5 (PLAN.md): external text — voice input above all — must reach
 * the client verbatim and must NOT be fed through the jamo pipeline. The
 * JNI layer passes it straight to hunmin_ic_commit_text() and returns whatever
 * that produces, including the in-flight composition it committed first. The
 * temptation to "help" here (splitting, trimming, re-composing) is exactly the
 * bug invariant 5 exists to prevent, so there is deliberately no post-processing
 * between the call and the return.
 */
JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeCommitText(JNIEnv *env, jclass k, jlong ic,
                                                   jstring text) {
  (void)k;
  hunmin_ic *i = (hunmin_ic *)(intptr_t)ic;
  if (!i) return (*env)->NewStringUTF(env, "");
  char *t = dup_java_string(env, text);
  if (!t) return (*env)->NewStringUTF(env, "");
  const char *r = hunmin_ic_commit_text(i, t);
  free(t);
  return str_or_empty(env, r);
}

/* ===================== hanja candidates (P3) ============================ */

JNIEXPORT jint JNICALL
Java_org_hunmin_ime_HunminNative_nativeSuggestCount(JNIEnv *env, jclass k, jlong ctx,
                                                     jstring hangul) {
  (void)env; (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  if (!c) return 0;
  char *h = dup_java_string(env, hangul);
  if (!h) return 0;
  jint n = (jint)hunmin_suggest_count(c, h);
  free(h);
  return n;
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeSuggestText(JNIEnv *env, jclass k, jlong ctx, jint i) {
  (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  return str_or_empty(env, c ? hunmin_suggest_text(c, i) : NULL);
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeSuggestDetail(JNIEnv *env, jclass k, jlong ctx, jint i) {
  (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  return str_or_empty(env, c ? hunmin_suggest_detail(c, i) : NULL);
}

JNIEXPORT jstring JNICALL
Java_org_hunmin_ime_HunminNative_nativeSuggestKind(JNIEnv *env, jclass k, jlong ctx, jint i) {
  (void)k;
  hunmin_ctx *c = (hunmin_ctx *)(intptr_t)ctx;
  return str_or_empty(env, c ? hunmin_suggest_kind(c, i) : NULL);
}
