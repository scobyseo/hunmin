// SPDX-License-Identifier: LGPL-2.1-or-later
pluginManagement {
    repositories {
        // Google's Maven first: AGP and the platform tools are only there.
        google()
        gradlePluginPortal()
        mavenCentral()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "hunmin-ime"
include(":app")
include(":keymaps")
