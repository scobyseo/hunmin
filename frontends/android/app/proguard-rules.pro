# SPDX-License-Identifier: LGPL-2.1-or-later
#
# ProGuard / R8 rules for the Hunmin Android IME.
#
# JNI is reached from Kotlin *by name*: the symbols in libhunjni/libhunmin are
# Java_org_hunmin_ime_<Class>_<method>, so a shrinker that renames the declaring
# class or its native methods produces a library the JVM cannot link —
# java.lang.UnsatisfiedLinkError "No implementation found for long f0.p.c(...)"
# at HunminInputMethodService.onCreate, i.e. a keyboard that dies the moment a
# text field takes focus.
#
# MEASURED 2026-11-07 on a real device: this file kept org.hunmin.ime.HunminNative,
# HunminEngine, HunminKeys, HunminData and HunminTrace — class names that no
# longer exist in the tree. The real bridge is HunminNativeBridge, so nothing
# kept it, R8 renamed it, and the release APK crashed on first focus. The debug
# APK is unminified, which is why tier E1/E2 stayed green throughout.
#
# The generic rule below is the one that must not be removed: it keeps the name
# of ANY class that declares a native method, whatever that class is called next
# time. The explicit keep beside it is a belt, not the mechanism.

# ──────────────────────────────────────────────────────────────────────
# JNI: every class declaring native methods keeps its name, and theirs
# ──────────────────────────────────────────────────────────────────────
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# ──────────────────────────────────────────────────────────────────────
# The one bridge object: every engine call goes through it
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminNativeBridge {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Callbacks the JNI shim looks up BY NAME (hunmin-jni.c GetMethodID).
# These cannot be covered by a generic rule: the C side decides the names, so
# every class it reflectively reaches has to be listed here. Adding a lookup
# there without adding a keep here is a release-only crash — measured
# 2026-11-07: specialSupports/specialPerform were renamed and the shim died
# with NoSuchMethodError on the first text field.
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminSpecialHost { *; }

# ──────────────────────────────────────────────────────────────────────
# Components the platform instantiates by name. AGP already generates keeps
# for manifest-declared components; these restate the ones also launched by
# explicit intent, so a rename cannot quietly break the settings surface.
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminInputMethodService { *; }
-keep class org.hunmin.ime.SettingsActivity { *; }
-keep class org.hunmin.ime.PermissionRequestActivity { *; }
-keep class org.hunmin.ime.OverlayActivity { *; }

# ──────────────────────────────────────────────────────────────────────
# Voice path: reached by interface name from the service and the overlay
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.VoiceInput { *; }
-keep interface org.hunmin.ime.Recognizer { *; }
-keep interface org.hunmin.ime.PermissionGate { *; }

# ──────────────────────────────────────────────────────────────────────
# Attributes the platform reads reflectively
# ──────────────────────────────────────────────────────────────────────
-keepattributes *Annotation*
-keepattributes Signature
-keepattributes EnclosingMethod
-keepattributes InnerClasses

# ──────────────────────────────────────────────────────────────────────
# Optional platform APIs and test-only classes: absent at runtime is fine
# ──────────────────────────────────────────────────────────────────────
-dontwarn android.speech.SpeechRecognizer
-dontwarn android.speech.RecognitionListener
-dontwarn android.speech.RecognizerIntent
-dontwarn androidx.test.**
-dontwarn junit.**