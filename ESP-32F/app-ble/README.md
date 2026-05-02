# BLE 控制 LED


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
