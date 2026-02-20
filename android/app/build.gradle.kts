plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace   = "com.tschuster.esp32nav"
    compileSdk  = 34

    defaultConfig {
        applicationId  = "com.tschuster.esp32nav"
        minSdk         = 29          // Android 10 → WifiNetworkSpecifier
        targetSdk      = 34
        versionCode    = 1
        versionName    = "1.0"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    buildFeatures { compose = true }
    composeOptions { kotlinCompilerExtensionVersion = "1.5.14" }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime)
    implementation(libs.androidx.lifecycle.viewmodel)
    implementation(libs.androidx.activity.compose)

    val bom = platform(libs.compose.bom)
    implementation(bom)
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.tooling)
    implementation(libs.compose.material3)

    implementation(libs.play.services.location)
    implementation(libs.okhttp)
    implementation(libs.kotlinx.coroutines.android)

    // Mapa OSM interactivo
    implementation("org.osmdroid:osmdroid-android:6.1.18")
}
