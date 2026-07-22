plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.finlink.android"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.finlink.android"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "0.1"
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}

// Deliberately no dependencies beyond the Kotlin/Android defaults: this is a
// bare-bones demo, and everything that matters (transport, protocol, codec)
// lives in the native finlink_core library instead of a networking/JSON/etc.
// library on the JVM side.
