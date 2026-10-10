# SPDX-License-Identifier: LGPL-2.1-or-later
#
# ProGuard / R8 rules for the Hunmin Android IME.
#
# JNI is reached from Kotlin *by name*, so the shim's exported symbols and
# HunminNative members are exactly what a shrinker removes. This file keeps
# everything the native bridge and the IME lifecycle depend on.

# ──────────────────────────────────────────────────────────────────────
# JNI bridge: HunminNative (all native methods, fields, the object itself)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminNative {
    *;
}

# The native methods are called by name from the JVM via JNI. The C symbols
# (Java_org_hunmin_ime_HunminNative_*) are in the native .so and are not
# affected by R8. However, the Kotlin declarations must be kept so the JVM
# can link them.

# ──────────────────────────────────────────────────────────────────────
# Engine adapter: HunminEngine (JNI bridge class, instantiated from service)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminEngine {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# IME entry point: HunminInputMethodService (instantiated by the platform)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminInputMethodService {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Voice path: VoiceInput.kt contains VoiceSession, VoiceLauncher, VoiceResult,
# VoicePath, VoicePermissionAction, Recognizer, SpeechRecognizerRecognizer,
# VoiceSession, PermissionGate, AndroidPermissionGate
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.VoiceInput {
    *;
}
-keep class org.hunmin.ime.VoiceSession {
    *;
}
-keep class org.hunmin.ime.VoiceLauncher {
    *;
}
-keep class org.hunmin.ime.VoiceResult {
    *;
}
-keep class org.hunmin.ime.VoicePath {
    *;
}
-keep class org.hunmin.ime.VoicePermissionAction {
    *;
}
-keep class org.hunmin.ime.Recognizer {
    *;
}
-keep class org.hunmin.ime.SpeechRecognizerRecognizer {
    *;
}
-keep class org.hunmin.ime.PermissionGate {
    *;
}
-keep class org.hunmin.ime.AndroidPermissionGate {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Key mapping: HunminKeys (engineChar, hasLayoutKey, all)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminKeys {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Data installation: HunminData (install)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminData {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Trace sink: HunminTrace (enable, disable, event, close, isEnabled)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminTrace {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Layout picker UI: LayoutPicker (bind, select, onMicClicked)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.LayoutPicker {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Permission activity: PermissionRequestActivity (raised from service)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.PermissionRequestActivity {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Layout entry data class (used in picker and service)
# ──────────────────────────────────────────────────────────────────────
-keep class org.hunmin.ime.HunminInputMethodService$LayoutEntry {
    *;
}

# ──────────────────────────────────────────────────────────────────────
# Keep annotations and enums used by the above classes
# ──────────────────────────────────────────────────────────────────────
-keepattributes *Annotation*
-keepattributes Signature
-keepattributes EnclosingMethod
-keepattributes InnerClasses

# ──────────────────────────────────────────────────────────────────────
# Native method names must be preserved exactly for JNI linkage
# ──────────────────────────────────────────────────────────────────────
-keepclassmembers class org.hunmin.ime.HunminNative {
    native <methods>;
}

# ──────────────────────────────────────────────────────────────────────
# Don't warn about missing classes from optional platform APIs
# ──────────────────────────────────────────────────────────────────────
-dontwarn android.speech.SpeechRecognizer
-dontwarn android.speech.RecognitionListener
-dontwarn android.speech.RecognizerIntent
-dontwarn androidx.test.**
-dontwarn junit.**

# ──────────────────────────────────────────────────────────────────────
# Keep the JNI library load call (static initializer is kept with the class)
# ──────────────────────────────────────────────────────────────────────