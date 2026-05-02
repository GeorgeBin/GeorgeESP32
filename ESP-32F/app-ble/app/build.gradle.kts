import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

// apk 打包：类型 + 文件命名 + 存储路径
val apkExportBuildType = providers.gradleProperty("apkExportBuildType").orElse("release")
val apkExportDir = providers.gradleProperty("apkExportDir").orElse("${projectDir}/build/apk")
val apkFileNameFormat = providers.gradleProperty("apkFileNameFormat")
    .orElse($$"${appName}-v${versionName}-${buildType}.apk")

// apk 打包：签名
val signingStoreFilePath = providers.gradleProperty("signing.storeFile")
    .orElse("${rootDir}/signkey/systemkey.jks")
val signingStoreFile = file(signingStoreFilePath.get())
val hasReleaseSigning = signingStoreFile.exists()
val signingKeyAlias = providers.gradleProperty("signing.keyAlias").orElse("zyzl")
val signingKeyPassword = providers.gradleProperty("signing.keyPassword").orElse("androidsystem")
val signingStorePassword = providers.gradleProperty("signing.storePassword").orElse("androidsystem")

android {
    namespace = "com.george.esp32k.led"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        minSdk = 23
        targetSdk = 36
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            // noinspection ChromeOsAbiSupport
            abiFilters += setOf("armeabi-v7a","arm64-v8a")
        }

        applicationId = "com.george.esp32k.led"

        versionCode = 1_01
        versionName = "1.01"

        // 更新记录
    }

    signingConfigs {
            create("release") {
                keyAlias = signingKeyAlias.get()
                keyPassword = signingKeyPassword.get()
                storeFile = signingStoreFile
                storePassword = signingStorePassword.get()
            }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
                signingConfig = signingConfigs.getByName("release")
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
        debug {
                signingConfig = signingConfigs.getByName("release")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    buildFeatures {
        compose = true
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.datastore.preferences)
    implementation(libs.process.phoenix)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.nordic.ble)
    implementation(libs.nordic.ble.ktx)
    testImplementation(libs.junit)
    testImplementation(libs.json)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.compose.ui.test.junit4)
    debugImplementation(libs.androidx.compose.ui.tooling)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
}

tasks.register<Copy>("exportApk") {
    group = "george"
    description = "Builds APK and copies it to configurable directory (apkExportDir)."

    val buildType = apkExportBuildType.get().trim().lowercase()
    require(buildType == "release" || buildType == "debug") {
        "apkExportBuildType must be 'release' or 'debug', but was '$buildType'"
    }
    val assembleTask = "assemble" + buildType.replaceFirstChar { it.uppercase() }
    dependsOn(assembleTask)

    val appName = "George_LED"
    val appId = android.defaultConfig.applicationId ?: "unknown.package"
    val versionName = android.defaultConfig.versionName ?: "0.0.0"
    val versionCode = android.defaultConfig.versionCode ?: 0
    val buildTime = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss"))
    val resolvedApkName = apkFileNameFormat.get()
        .replace($$"${appName}", appName)
        .replace($$"${packageName}", appId)
        .replace($$"${buildType}", buildType)
        .replace($$"${versionName}", versionName)
        .replace($$"${versionCode}", versionCode.toString())
        .replace($$"${buildTime}", buildTime)

    from(layout.buildDirectory.dir("outputs/apk/$buildType")) {
        include("*.apk")
    }
    into(file(apkExportDir.get()))
    rename { resolvedApkName }

    doFirst {
        if (buildType == "release" && !hasReleaseSigning) {
            logger.error(
                "Release signing keystore not found: ${signingStoreFile.absolutePath}. " +
                        "Release APK will use the default signing behavior."
            )
        }
        logger.lifecycle("exportApk: buildType=$buildType")
        logger.lifecycle("exportApk: outputDir=${file(apkExportDir.get()).absolutePath}")
        logger.lifecycle("exportApk: fileName=$resolvedApkName")
    }
}
