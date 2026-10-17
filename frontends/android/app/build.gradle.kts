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

// The engine's tables ship as APK assets, and they ship from ONE place: the
// repo's libhunmin/data. Copying them into frontends/android/ would be a second
// copy of the normative layout definitions -- the exact thing AGENTS.md's
// "layouts are data, never code" rule exists to prevent, because a stale copy
// under frontends/ would type the wrong Korean while every host test stayed
// green. So assets are synced from the source tree at build time and nothing
// under src/main/assets/data is ever committed.
val syncHunminDataInstaller by tasks.registering(Sync::class) {
    from(rootProject.file("../../libhunmin/data")) {
        exclude("krdict/**")
    }
    into(layout.projectDirectory.dir("src/main/assets/data"))
}

// The sync has to feed the task that PACKAGES assets, and that is
// mergeDebugAssets -- not preBuild. Wiring it to preBuild alone (as a first
// revision did) left the APK with zero assets while every build reported
// success: the Sync task ran, wrote into src/main/assets/, and nothing had
// declared that directory as an input, so the files were never packaged. That
// produced a 950 KB APK that installs, starts, and then reports "no layout
// tables extracted" on a device.
// Depends on both because the same source set is read by the debug and
// release variants, and only one of them is built per invocation.
listOf("mergeDebugAssets", "mergeReleaseAssets").forEach { name ->
    tasks.matching { it.name == name }.configureEach {
        dependsOn(syncHunminDataInstaller)
    }
}
tasks.named("preBuild") { dependsOn(syncHunminDataInstaller) }

android {
    namespace = "org.hunmin.ime"
    compileSdk = 35

    // THE NDK VERSION, PINNED AT THE `android` LEVEL, NOT INSIDE defaultConfig.
    //
    // AGP with no ndkVersion scans $ANDROID_HOME/ndk/* and takes what it finds.
    // On a GitHub ubuntu-24.04 runner that directory holds BOTH
    // 27.3.13750724 (what this repo builds against -- tools/build-android.sh,
    // docs/ENVIRONMENT.md) and 26.1.10909125. Which one AGP hands the NDK
    // toolchain file is directory order, not configuration.
    //
    // Measured 2026-10-14, issue #59's CI run: the linux job's tier-C Android
    // conformance tests failed with
    //   [CXX1101] NDK at /usr/local/lib/android/sdk/ndk/26.1.10909125
    //   did not have a source.properties file
    // -- 11 tests, 456 s of test time, all engine-android-conformance-*. They
    // had passed in the run before it, and what changed is that they now run on
    // a job where engine-android-e1 (a full Gradle configure) does not populate
    // ~/.gradle first.
    //
    // The placement is load-bearing, and getting it wrong is silent. A first
    // attempt put `version = "27.3.13750724"` inside the `ndk { }` block under
    // defaultConfig. That reads as correct and is not: measured on this host,
    // `./gradlew :app:properties` reports `version: 27.3.13750724`, so the DSL
    // property is accepted, while the CXX build model
    // (build/intermediates/cxx/*/logs/*/build_model.json) reports
    // `"ndkVersion": null` and `--info` shows
    // `-DCMAKE_TOOLCHAIN_FILE=…/ndk/26.1.10909125/build/cmake/android.toolchain.cmake`
    // regardless of whether ANDROID_NDK is exported. `android.ndkVersion` is
    // the property externalNativeBuild actually resolves.
    //
    // Deliberately NOT left to the ANDROID_NDK environment variable, which
    // tools/build-android.sh and the android-release job export: a build that
    // depends on an exported variable is green in the job that exports it and
    // broken in the one that does not, which is exactly the failure above.
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "org.hunmin.ime"
        // minSdk 24, not 21: tools/build-android.sh has cross-compiled the core
        // at API 24 since 2026-10-03 and that is the level the shipped .so was
        // actually built and exercised against on the emulator (tier E0). Going
        // lower would mean a .so nobody has ever run.
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0-p5"

        externalNativeBuild {
            cmake {
                // The JNI shim is tiny and links against nothing but libhunmin
                // and libc, so the build-type flags that matter are the NDK
                // defaults; nothing here needs -O3 tuning or LTO.
                arguments += listOf("-DANDROID_STL=none")
            }
        }

        ndk {
            // Deliberately NOT the default all-four set. x86_64 is the
            // emulator (tier E2) and arm64-v8a is real devices; see
            // gradle.properties.
            abiFilters += setOf("x86_64", "arm64-v8a")
        }

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    // release signing (P6): the keystore config lives in gitignored
    // local.properties (frontends/android/local.properties); nothing in this
    // file hardcodes a host path. When local.properties is absent (e.g. a
    // fresh clone) a throwaway release keystore is generated at build time,
    // the same route tools/build-release.sh and the CI android-release job
    // take. B8 (issue #1) is resolved: no host path in this file.
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
            // the environment (ci.yml android-release, tools/build-release.sh).
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
            isMinifyEnabled = true
            proguardFiles("proguard-rules.pro")
            // Apply the release signing configuration so release APKs can be
            // installed on emulators/physical devices that do not have a
            // custom debug keystore mapped.
            signingConfig = signingConfigs.getByName("release")
        }
        debug {
            // debuggable so that tier E can drive the engine via logcat/IM
            isJniDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    sourceSets {
        getByName("main") {
            // The engine's data ships inside the APK as assets. libhunmin's
            // loader is a filesystem layout scanner -- it opens <dir>/compose/
            // and <dir>/layouts/*.json with opendir/fopen -- so the service
            // must extract them to filesDir before the first hunmin_ctx_new().
            // See HunminDataInstaller.install().
            assets.srcDirs("src/main/assets")
            // One source dir per ABI build dir, because
            // tools/build-android.sh writes to build-android/<abi>/jniLibs/<abi>/libhunmin.so
            // and AGP wants <a jniLibs root>/<abi>/libhunmin.so.
            listOf("x86_64", "arm64-v8a").forEach { abi ->
                jniLibs.srcDir(rootProject.file("../../build-android/$abi/jniLibs"))
            }
        }
        getByName("test") {
            java.srcDirs("src/test/java")
        }
        getByName("androidTest") {
            java.srcDirs("src/androidTest/java")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    packaging {
        jniLibs {
            // libhunmin.so is built by the CMake path (tools/build-android.sh),
            // not by AGP. Keep it uncompressed: a compressed .so cannot be
            // mmap'd straight out of the APK on pre-Android-11
            // extraction-free installs, and extractNativeLibs=false means the
            // loader maps it in place.
            useLegacyPackaging = false
        }
    }

    testOptions {
        unitTests {
            // Android framework methods return defaults instead of throwing
            // "Stub!" -- needed because InputMethodService and View are only
            // ever touched by androidTest, and a unit test constructing one
            // would otherwise die on the first call.
            isReturnDefaultValues = true

            // The test JVM does NOT inherit the shell environment, so
            // HUNMIN_JNI_LIB (which points at the HOST build of the JNI shim --
            // a JVM cannot dlopen an Android .so) has to be handed over
            // explicitly. Without this every test's assumeTrue fails, Gradle
            // reports BUILD SUCCESSFUL, and the run measures nothing; that is
            // what tools/android-unit.sh exists to catch.
            all {
                it.environment("HUNMIN_JNI_LIB", System.getenv("HUNMIN_JNI_LIB") ?: "")
                // rootProject.projectDir IS frontends/android, so the repo root
                // is two levels up, not one. Getting this wrong by a level made
                // every test skip on a missing data/ dir, invisibly.
                it.environment("HUNMIN_REPO_ROOT", rootProject.projectDir.parentFile.parentFile.absolutePath)
                // THE critical line, and the reason the suite reported 17/17
                // SKIPPED while looking perfectly green. Unit tests compile
                // against the module's own classes, and without this the main
                // source set -- where HunminNativeBridge, HunminAdapter and HunminKeyRegistry
                // live -- is not on the test classpath. So every @Before died
                // with NoClassDefFoundError, and JUnit 4 records an
                // initialisation error in @Before as a SKIPPED test, not a
                // failure. tools/android-unit.sh catches the skips; this line
                // is what makes them not happen.
                it.dependsOn(
                    tasks.named("compileDebugKotlin"),
                    tasks.named("mergeDebugJavaResource"),
                )
                // Put the main classes on the test classpath so the real
                // HunminAdapter/HunminKeyRegistry/HunminNativeBridge are under test rather
                // than missing. Kotlin's output dir is fixed for AGP builds, so
                // name it directly instead of asking the task for its
                // destinationDirectory (which a KotlinCompile task does not
                // expose in the version AGP 8.6 pins).
                it.classpath += files(
                    layout.buildDirectory.dir("tmp/kotlin-classes/debug"),
                    layout.buildDirectory.dir("intermediates/javac/debug/classes"),
                    layout.buildDirectory.dir("intermediates/compile_and_runtime_not_namespaced_r_class_jar/debug/processDebugResources/R.jar"),
                )
            }
        }
    }
}

dependencies {
    // Deliberately tiny. An IME needs the platform's InputConnection and
    // nothing else; anything in here is either a keymap we already proved we
    // do not need (a keymap would bypass libhunmin entirely, which is the one
    // thing P5 must not do) or a compileSdk convenience that costs method count
    // for nothing.
    implementation("androidx.core:core:1.13.1")
    implementation("androidx.appcompat:appcompat:1.6.1")

    testImplementation("junit:junit:4.13.2")

    androidTestImplementation("androidx.test.ext:junit:1.2.1")
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test:rules:1.6.1")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.5.1")
    androidTestImplementation("androidx.test.espresso:espresso-contrib:3.5.1")
    androidTestImplementation("androidx.test.uiautomator:uiautomator:2.3.0")
    androidTestImplementation("androidx.test.espresso:espresso-intents:3.5.1")
}

// Tier C: trace emitter for Android (host JVM, uses host JNI shim).
// Runs via tools/android-trace-emit.sh, registered as ctest engine-android-conformance.
//
// The classpath is factored out because TWO things need it and they must not
// drift apart: runTraceEmit, which runs the emitter, and printTraceEmitClasspath,
// which hands the same list to tools/android-trace-emit.sh so the shell can
// invoke `java` directly instead of paying a Gradle invocation per trace.
@Suppress("unused")
fun traceEmitClasspath() = files(
    // The emitter (org.hunmin.ime.TraceEmit) lives in the unit-test source
    // set, but it calls the JNI shim HunminNativeBridge, which is MAIN source.
    // A unit-test runtime classpath carries the main classes, but they land
    // in the variant's own kotlin-classes dir (…/debug, not
    // …/debugUnitTest) and that directory is NOT part of
    // debugUnitTestRuntimeClasspath. Measured: with only the unit-test dir
    // on the path the run died with
    //   NoClassDefFoundError: org/hunmin/ime/HunminNativeBridge
    // so both directories are listed explicitly.
    layout.buildDirectory.dir("tmp/kotlin-classes/debugUnitTest"),
    layout.buildDirectory.dir("tmp/kotlin-classes/debug"),
    layout.buildDirectory.dir("intermediates/javac/debugUnitTest/classes"),
    layout.buildDirectory.dir("intermediates/javac/debug/classes"),
    layout.buildDirectory.dir("intermediates/compile_and_runtime_not_namespaced_r_class_jar/debugUnitTest/processDebugUnitTestResources/R.jar"),
    configurations.getByName("debugUnitTestRuntimeClasspath")
)

tasks.register("runTraceEmit", JavaExec::class) {
    group = "hunmin"
    description = "Run the Android trace emitter (tier C)"
    dependsOn("compileDebugUnitTestKotlin", "compileDebugUnitTestJavaWithJavac")
    mainClass.set("org.hunmin.ime.TraceEmit")
    doFirst {
        classpath = traceEmitClasspath()
        // Arguments: trace file path from project property traceIn
        val traceIn = project.findProperty("traceIn") as? String ?: ""
        args(traceIn)
        // JNI library and repo root from gradle properties or env
        environment("HUNMIN_JNI_LIB", project.findProperty("HunminJniLib") as? String ?: System.getenv("HUNMIN_JNI_LIB") ?: "")
        environment("HUNMIN_REPO_ROOT", project.findProperty("HunminRepoRoot") as? String ?: System.getenv("HUNMIN_REPO_ROOT") ?: "")
        // Fail fast on error
        isIgnoreExitValue = false
    }
}

// Prints the classpath, one absolute path per line, after making sure the
// classes it names exist and are current. tools/android-trace-emit.sh caches
// this and then runs `java -cp ...` per trace, which is what lets the fourteen
// engine-android-conformance tests run concurrently: they only read the build
// tree, where a per-trace `gradlew --rerun-tasks` wrote to the same
// intermediates/ paths and made nine of them fail. Measured at 2451fdc.
tasks.register("printTraceEmitClasspath") {
    group = "hunmin"
    description = "Print the tier C trace-emitter classpath (one path per line)"
    dependsOn("compileDebugUnitTestKotlin", "compileDebugUnitTestJavaWithJavac")
    doLast {
        traceEmitClasspath().files.forEach { println(it.absolutePath) }
    }
}
