// SPDX-License-Identifier: LGPL-2.1-or-later

import org.gradle.api.tasks.testing.Test

import java.io.BufferedInputStream
import java.io.File
import java.util.Properties

import org.gradle.api.GradleException

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

    // release signing (P5.5): the keystore config lives in gitignored
    // local.properties (frontends/android/local.properties); nothing here
    // hardcodes a host path. Same pattern as app/build.gradle.kts.
    signingConfigs {
        create("release") {
            // rootProject.projectDir is frontends/android/ (settings.gradle.kts
            // lives there), so "local.properties" resolves to the shared file
            // regardless of the module working directory.
            val localPropertiesFile = rootProject.file("local.properties")
            val localProperties = Properties().apply {
                if (localPropertiesFile.exists()) {
                    localPropertiesFile.inputStream().use { load(it) }
                }
            }
            // Values read from local.properties (all default to empty).
            var configuredStoreFile = localProperties.getProperty("storeFile") ?: ""
            var configuredStorePassword = localProperties.getProperty("storePassword") ?: ""
            var configuredKeyPassword = localProperties.getProperty("keyPassword") ?: ""
            var configuredKeyAlias = localProperties.getProperty("keyAlias") ?: ""

            // CI/script override: a generated keystore is handed to Gradle via
            // the environment (tools/build-release.sh).
            val ciStore = System.getenv("HUNMIN_RELEASE_STORE")
            if (ciStore != null && File(ciStore).exists()) {
                storeFile = file(ciStore)
                storePassword = "android"
                keyPassword = "android"
                keyAlias = "hunmin"
            } else if (configuredStoreFile.isNotBlank() && File(configuredStoreFile).exists()) {
                storeFile = file(configuredStoreFile)
                storePassword = configuredStorePassword
                keyPassword = configuredKeyPassword
                keyAlias = configuredKeyAlias
            } else {
                // No keystore available: generate a throwaway release key so
                // assembleRelease runs end-to-end locally and in CI (build only,
                // not a releasable signature).
                val ks = rootProject.file("build/release-ci.keystore")
                val ksDir = ks.parentFile
                if (ksDir != null && !ksDir.exists()) {
                    ksDir.mkdirs()
                }
                // Both modules (app, keymaps) resolve to the SAME keystore path,
                // and both signingConfigs blocks are evaluated during
                // configuration -- before any task runs. Generating
                // unconditionally made the second module's keytool fail with
                // "alias <hunmin> already exists" (CI run 38416072671). A
                // keystore already on disk is reused: the passwords and alias
                // below are fixed, so reuse is safe.
                if (!ks.exists()) {
                    var generated = false
                    try {
                        val pb = ProcessBuilder(
                            "keytool", "-genkeypair",
                            "-keystore", ks.absolutePath,
                            "-storepass", "android", "-alias", "hunmin",
                            "-keypass", "android", "-keyalg", "RSA", "-keysize", "2048",
                            "-validity", "10000",
                            "-dname", "CN=hunmin-CI,O=hunmin,L=CI,C=KR"
                        ).redirectErrorStream(true)
                        val process = pb.start()
                        val output = BufferedInputStream(process.inputStream).reader().readText()
                        if (process.waitFor() != 0) {
                            throw GradleException("keytool failed: $output")
                        }
                        generated = true
                    } catch (e: Exception) {
                        throw GradleException("cannot generate throwaway keystore: ${e.message}", e)
                    }
                    if (!generated) {
                        throw GradleException("keytool unavailable; set HUNMIN_RELEASE_STORE or local.properties")
                    }
                }
                storeFile = ks
                storePassword = "android"
                keyPassword = "android"
                keyAlias = "hunmin"
            }
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
