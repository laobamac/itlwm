[中文](README.CN.md)

# itlwm

**An Intel Wi-Fi Adapter Kernel Extension for macOS, based on the OpenBSD Project.**

## Documentation

We highly recommend exploring our documentation before using this Kernel Extension:

- [Intro](https://OpenIntelWireless.github.io/itlwm)
- [Compatibility](https://openintelwireless.github.io/itlwm/Compat)
- [FAQ](https://openintelwireless.github.io/itlwm/FAQ)

## AirportItlwm on macOS Sequoia

The `AirportItlwm-Sequoia` target supports macOS Sequoia 15.2 and later on x86_64
through the native Wi-Fi interface. It has been tested on macOS 15.8 (24H23).
Sequoia and Tahoe use separate ABI headers and build products.

With the same MacKernelSDK checkout described below:

```sh
xcodebuild -project itlwm.xcodeproj -scheme AirportItlwm-Sequoia \
  -configuration Release ARCHS=x86_64 CODE_SIGNING_ALLOWED=NO build
```

Use this kext only on Darwin 24.2.0–24.99.99. Keep the Tahoe kext restricted to
Darwin 25.x when both systems share an OpenCore configuration. HE remains opt-in
with `itlwm_he=1`, and the Tahoe feature limitations below also apply to Sequoia.

## AirportItlwm on macOS 26

This fork supports macOS Tahoe 26.x (x86_64) through the native Wi-Fi interface,
including scanning, connecting, network switching, and private Wi-Fi addresses.

- **Wi-Fi 6 (802.11ax / HE) is disabled by default.**
  Add `itlwm_he=1` to enable it on supported adapters.
- WPA2-Personal and WPA2/WPA3 transition networks with optional PMF are supported.
  WPA3-only, mandatory PMF, AWDL / AirDrop, and MLO are not supported.
- Device support follows the upstream hardware table; Tahoe compatibility still
  requires validation on each adapter.

Download Sequoia or Tahoe **Release** and **Debug** builds from [this fork's Releases](https://github.com/laobamac/itlwm/releases).
CI builds both systems and updates the alpha release after all builds pass on `main`.

To build locally, install Xcode and Python 3, and check out MacKernelSDK commit
`3f750085caa17ec3a7880f11c11bf4f48cd6a164` in `MacKernelSDK/`:

```sh
xcodebuild -project itlwm.xcodeproj -scheme AirportItlwm-Tahoe \
  -configuration Release ARCHS=x86_64 CODE_SIGNING_ALLOWED=NO build
```

## Download

[![Download](https://img.shields.io/github/v/release/laobamac/itlwm?include_prereleases&label=Download)](https://github.com/laobamac/itlwm/releases)

## Questions and Issues

Check out our [FAQ Page](https://openintelwireless.github.io/itlwm/FAQ) for more info.

If you have other questions or feedback, feel free to [![Join the chat at https://gitter.im/OpenIntelWireless/itlwm](https://badges.gitter.im/OpenIntelWireless/itlwm.svg)](https://gitter.im/OpenIntelWireless/itlwm?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge).

We only accept bug reports in GitHub Issues.

## Credits

- [Acidanthera](https://github.com/acidanthera) for [MacKernelSDK](https://github.com/acidanthera/MacKernelSDK)
- [Apple](https://www.apple.com) for [macOS](https://www.apple.com/macos)
- [AppleIntelWiFi](https://github.com/AppleIntelWiFi) for [Black80211-Catalina](https://github.com/AppleIntelWiFi/Black80211-Catalina)
- [ErrorErrorError](https://github.com/ErrorErrorError) for UserClient bug fixes
- [Intel](https://www.intel.com) for [Wireless Adapter Firmwares](https://www.intel.com/content/www/us/en/support/articles/000005511/network-and-io/wireless.html) and [iwlwifi](https://wireless.wiki.kernel.org/en/users/drivers/iwlwifi)
- [Linux](https://www.kernel.org) for [iwlwifi](https://wireless.wiki.kernel.org/en/users/drivers/iwlwifi)
- [mercurysquad](https://github.com/mercurysquad) for [Voodoo80211](https://github.com/mercurysquad/Voodoo80211)
- [OpenBSD](https://openbsd.org) for [net80211, iwn, iwm, and iwx](https://github.com/openbsd/src)
- [pigworlds](https://github.com/OpenIntelWireless/itlwm/commits?author=pigworlds) for DVM devices support, MIRA bug fixes, and Tx aggregation for MVM Gen 1 devices
- [rpeshkov](https://github.com/rpeshkov) for [black80211](https://github.com/rpeshkov/black80211)
- [usr-sse2](https://github.com/usr-sse2) for implementing the usage of Apple RSN Supplicant and bug fixes
- [zxystd](https://github.com/zxystd) for developing [itlwm](https://github.com/OpenIntelWireless/itlwm)

## Acknowledgements

- [@penghubingzhou](https://github.com/startpenghubingzhou)
- [@Bat.bat](https://github.com/williambj1)
- [@iStarForever](https://github.com/XStar-Dev)
- [@stevezhengshiqi](https://github.com/stevezhengshiqi)
- [@DogAndPot](https://github.com/DogAndPot) for providing resources and help for system configuration
- [@Daliansky](https://github.com/Daliansky) for providing Wi-Fi cards
