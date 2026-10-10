// SPDX-License-Identifier: LGPL-2.1-or-later

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

    signingConfigs {
        create("release") {
            storeFile = file("/home/scoby/.android/debug.keystore")
            storePassword = "android"
            keyPassword = "android"
            keyAlias = "androiddebugkey"
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