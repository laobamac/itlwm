# itlwm

**基于OpenBSD项目的macOS Intel Wi-Fi网卡内核扩展。**

## 文档

强烈建议在使用此内核扩展之前阅读我们的文档：

- [项目介绍](https://OpenIntelWireless.github.io/itlwm)
- [兼容性](https://openintelwireless.github.io/itlwm/Compat)
- [常见问题](https://openintelwireless.github.io/itlwm/FAQ)

## macOS Sequoia上的AirportItlwm

`AirportItlwm-Sequoia` 构建目标支持在x86_64架构的macOS Sequoia 15.2及更高版本使用系统原生Wi-Fi界面。已在macOS 15.8（24H23）进行测试。Sequoia和Tahoe使用各自独立的ABI头文件和构建产物。

使用下文所述相同版本的MacKernelSDK源码进行构建：

```sh
xcodebuild -project itlwm.xcodeproj -scheme AirportItlwm-Sequoia \
  -configuration Release ARCHS=x86_64 CODE_SIGNING_ALLOWED=NO build
```

此kext仅适用于Darwin 24.2.0–24.99.99

当两个系统共用一份OpenCore配置时，请将Tahoe kext的适用范围限制为Darwin 25.x

HE仍需通过`itlwm_he=1`手动启用，下文所列的Tahoe功能限制同样适用于Sequoia

## macOS 26上的AirportItlwm

此分支支持在macOS Tahoe 26.x（x86_64）上使用系统原生Wi-Fi界面，
包括扫描、连接、切换网络和私有Wi-Fi地址等功能。

- **Wi-Fi 6（802.11ax / HE）默认禁用。**
  添加 `itlwm_he=1` 即可在支持的网卡上启用此功能。
- 支持WPA2-Personal，以及PMF为可选项的WPA2/WPA3过渡模式网络。
  不支持仅使用WPA3的网络、强制要求PMF的网络、AWDL/AirDrop和MLO。
- 支持的设备以上游硬件兼容性表为准；每款网卡在Tahoe上的兼容性仍需单独验证。

可从[此分支的Releases页面](https://github.com/laobamac/itlwm/releases)下载Sequoia或Tahoe的**Release**和**Debug**版本。
CI会为两个系统进行构建，并在`main`分支的所有构建均通过后更新alpha预发布版本。

如需在本地构建，请安装Xcode和Python 3，并在`MacKernelSDK/`目录中
将MacKernelSDK源码检出到提交 `3f750085caa17ec3a7880f11c11bf4f48cd6a164`：

```sh
xcodebuild -project itlwm.xcodeproj -scheme AirportItlwm-Tahoe \
  -configuration Release ARCHS=x86_64 CODE_SIGNING_ALLOWED=NO build
```

## 下载

[![Download](https://img.shields.io/github/v/release/laobamac/itlwm?include_prereleases&label=Download)](https://github.com/laobamac/itlwm/releases)

## 问题与反馈

请查阅我们的[常见问题页面](https://openintelwireless.github.io/itlwm/FAQ)，了解更多信息。

我们仅在GitHub Issues中接受错误报告。

## 贡献鸣谢

- 感谢 [Acidanthera](https://github.com/acidanthera) 提供 [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)
- 感谢 [Apple](https://www.apple.com) 提供 [macOS](https://www.apple.com/macos)
- 感谢 [AppleIntelWiFi](https://github.com/AppleIntelWiFi) 提供 [Black80211-Catalina](https://github.com/AppleIntelWiFi/Black80211-Catalina)
- 感谢 [ErrorErrorError](https://github.com/ErrorErrorError) 修复 UserClient 错误
- 感谢 [Intel](https://www.intel.com) 提供[无线网卡固件](https://www.intel.com/content/www/us/en/support/articles/000005511/network-and-io/wireless.html)和 [iwlwifi](https://wireless.wiki.kernel.org/en/users/drivers/iwlwifi)
- 感谢 [Linux](https://www.kernel.org) 提供 [iwlwifi](https://wireless.wiki.kernel.org/en/users/drivers/iwlwifi)
- 感谢 [mercurysquad](https://github.com/mercurysquad) 提供 [Voodoo80211](https://github.com/mercurysquad/Voodoo80211)
- 感谢 [OpenBSD](https://openbsd.org) 提供 [net80211、iwn、iwm 和 iwx](https://github.com/openbsd/src)
- 感谢 [pigworlds](https://github.com/OpenIntelWireless/itlwm/commits?author=pigworlds) 实现DVM设备支持、修复MIRA错误，并为MVM第一代设备实现发送（Tx）聚合
- 感谢 [rpeshkov](https://github.com/rpeshkov) 提供 [black80211](https://github.com/rpeshkov/black80211)
- 感谢 [usr-sse2](https://github.com/usr-sse2) 实现Apple RSN Supplicant的使用并修复错误
- 感谢 [zxystd](https://github.com/zxystd) 开发 [itlwm](https://github.com/OpenIntelWireless/itlwm)

## 致谢

- [@penghubingzhou](https://github.com/startpenghubingzhou)
- [@Bat.bat](https://github.com/williambj1)
- [@iStarForever](https://github.com/XStar-Dev)
- [@stevezhengshiqi](https://github.com/stevezhengshiqi)
- 感谢 [@DogAndPot](https://github.com/DogAndPot) 提供资源和系统配置帮助
- 感谢 [@Daliansky](https://github.com/Daliansky) 提供 Wi-Fi 网卡
