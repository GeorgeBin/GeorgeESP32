# Demo 模板工程

Demo 模板工程，方便写 Demo。



## 必改项

### 1. 项目别名

文件：settings.gradle.kts

```kotlin
rootProject.name = "InfraD_Template"
```

> 该值会影响 Android Studio 显示的工程名称。



### 2. 包名

文件：app/build.gradle.kts

```kotlin
applicationId = "com.demo.infra.d.template"
// 修改为你的正式包名，例如：
applicationId = "com.demo.xxx"
```



### 3. 应用名

文件：app/src/main/res/values/strings.xml

```xml
<string name="app_name">Demo</string>
```



### 4. 应用图标

直接用 Android Studio 的 `Image Asset` 重新生成 `ic_launcher`

1. Icon type：Launcher Icons(Adaptive and Legacy)
2. Name: ic_launcher
3. Foreground Layer
   * Asset type: Text  
   * Text: 输入应用名
   * Color: FFFFFF
   * Resize: 调整大小
4. Background Layer
   * Asset type: Color
   * Color: 2F318B
5. Options
   * Round Icon: No
   * Google Play Store Icon: No



## 版本更新

### 版本号

文件：app/build.gradle.kts

```kotlin
versionCode = 1_01
versionName = "1.01"
```

> 同时记录更新日志



## 打包

`exportApk` 说明：

- 默认导出 `release` 包
- 默认输出目录为 `app/build/apk`
- 默认文件名格式为 `${appName}-v${versionName}-${buildType}.apk`

可通过 Gradle 参数覆盖：

```bash
./gradlew exportApk -PapkExportBuildType=debug
./gradlew exportApk -PapkExportDir=/your/output/path
./gradlew exportApk -PapkFileNameFormat='${appName}-${versionName}-${buildTime}.apk'
```



## 项目更新时机

* Android Studio 更新，则统一调整
* 新的必备功能
