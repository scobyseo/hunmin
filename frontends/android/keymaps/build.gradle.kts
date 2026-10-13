// SPDX-License-Identifier: LGPL-2.1-or-later

import java.util.Properties

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// APK for hunmin keyboard layouts (P5.5)
// Minimal Kotlin code for the broadcast receiver, KCM files as raw resources.

android {
    namespace = "org.hunmin.keymaps"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.hunmin.keymaps"
        minSdk = 21
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0-p5.5"

        // No test runner needed - no code to test
    }

    // Release signing: same contract as the app module (issue #1). The
    // keystore is never named here -- it comes from
    // frontends/android/local.properties (gitignored), falling back to the
    // platform default debug keystore ($HOME/.android/debug.keystore) so a
    // fresh worktree still builds. See app/build.gradle.kts for the full
    // rule and its reasons.
    val localProps = Properties().apply {
        val f = rootProject.file("local.properties")
        if (f.isFile) f.inputStream().use { load(it) }
    }
    fun prop(name: String): String? =
        (project.findProperty(name) as String?)?.takeIf { it.isNotBlank() }
            ?: localProps.getProperty(name)?.takeIf { it.isNotBlank() }

    val signStoreFile = prop("storeFile")?.let { file(it) }
        ?: file("${System.getProperty("user.home")}/.android/debug.keystore")
    val signStorePassword = prop("storePassword")
    val signKeyPassword = prop("keyPassword") ?: signStorePassword
    val signKeyAlias = prop("keyAlias") ?: "androiddebugkey"

    if (!signStoreFile.isFile) {
        throw GradleException(
            "release signing: keystore not found at ${signStoreFile}. " +
                "Set storeFile/storePassword/keyPassword/keyAlias in " +
                "frontends/android/local.properties (gitignored)."
        )
    }
    if (signStorePassword == null) {
        throw GradleException(
            "release signing: keystore ${signStoreFile} has no storePassword. " +
                "Set storePassword (and keyPassword if the key differs) in " +
                "frontends/android/local.properties."
        )
    }

    signingConfigs {
        create("release") {
            storeFile = signStoreFile
            storePassword = signStorePassword
            keyPassword = signKeyPassword
            keyAlias = signKeyAlias
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isShrinkResources = false
            signingConfig = signingConfigs.getByName("release")
        }
        debug {
            // Minimal code, no JNI debugging needed
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        // KCM files are raw resources, keep as-is
        resources {
            excludes += listOf("**/*.kcm")
        }
    }
}

dependencies {
    // No dependencies - pure data APK
}