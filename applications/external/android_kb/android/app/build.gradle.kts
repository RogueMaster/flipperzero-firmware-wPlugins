plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.flipperzero.androidkeyboard"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.flipperzero.androidkeyboard"
        minSdk = 26
        targetSdk = 34
        versionCode = 6
        versionName = "0.5.1"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            // Sideload-friendly: signed with the debug keystore.
            // For Play Store / production, replace with your own signingConfig.
            signingConfig = signingConfigs.getByName("debug")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        viewBinding = true
    }
}

val appVersionName: String = android.defaultConfig.versionName ?: "0.0.0"

// Meaningful APK names with version, e.g. FlipperZeroKbd-0.2.0.apk
android.applicationVariants.configureEach {
    outputs.configureEach {
        val outputImpl = this as com.android.build.gradle.internal.api.BaseVariantOutputImpl
        outputImpl.outputFileName = when (buildType.name) {
            "release" -> "FlipperZeroKbd-$appVersionName.apk"
            else -> "FlipperZeroKbd-$appVersionName-${buildType.name}.apk"
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.4")
}
