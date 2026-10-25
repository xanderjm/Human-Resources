# Android Gradle toolchain upgrade — 25 October 2026

The Android platform owner deliberately upgrades the existing build-only Gradle wrapper from 9.6.1 to 9.8.2. Android release lint rejected the older wrapper through `AndroidGradlePluginVersion`. Gradle also recommends 9.8.2 and documents security fixes included from 9.8.1. Preserve the lint gate and qualify the maintained toolchain instead of suppressing the failing checker. [Official release notes](https://docs.gradle.org/9.8.2/release-notes.html).

## Capability and boundary

Gradle already owns Android Gradle Plugin orchestration: resource/manifest processing, Java compilation, R8, native CMake integration, APK assembly and Android lint. Replacing these with product-owned Python/CMake packaging is not a simple or equivalent alternative. This is one host build-tool upgrade; Android Gradle Plugin 9.3.0, JDK 17, API 36, NDK 28.2.13676358, SDL and shader tooling stay pinned. Gradle uses Apache License 2.0, with its distribution notices retained inside the downloaded host tool.

The wrapper works on the selected Android build hosts through the checked-in POSIX/Windows launchers and existing JDK discovery. The native iOS, macOS, Windows and Linux game paths do not acquire a Gradle runtime dependency. No JVM, Gradle code, wrapper JAR or downloaded distribution ships in the game: additional application binary, package and startup cost is zero. The ignored host distribution is approximately 152 MB compressed; extracted host caches are separate from packages. Gradle is outside gameplay, simulation timing and runtime threads.

## Exact integrity and upgrade mechanism

| Artifact | Pin |
| --- | --- |
| Binary distribution | `https://services.gradle.org/distributions/gradle-9.8.2-bin.zip` |
| Binary ZIP SHA-256 | `21bd5da2de49b642904077f38c0efdc7778e92b3ed0889392dca61b97e389607` |
| Wrapper JAR SHA-256 | `3b8a25775a69158b5ad2b1d17a80a88dc7b40a352a73eb27d06c612a7ce68e98` |

The ZIP was downloaded through HTTPS and independently hashed before execution. Both expected digests were retrieved from the official distribution checksum endpoints and compared with the [official checksum reference](https://gradle.org/release-checksums/). The vendor's documented two-stage wrapper procedure updates the distribution selector first and regenerates the launchers/JAR with the selected version on the second invocation. The generated JAR must match the official digest before qualification continues. Source properties retain distribution SHA-256 verification and HTTPS distribution URL validation. `verify_foundation.py` rejects distribution-version/checksum or wrapper-JAR drift; focused tests corrupt each independently.

Download verification and wrapper task logs live under ignored `build/evidence/mobile-release/`. The Android platform integration owner maintains this pin, follows upstream security advisories, and upgrades one toolchain component at a time. No hosted workflow was triggered.

## Replacement checks

Require the wrapper to report 9.8.2; pass focused pin rejection tests and aggregate static verification; assemble debug and unsigned release packages for arm64-v8a/x86_64; pass release lint without a new suppression; pass exact source-bound cooked shader/runtime package audits, packaged portrait checks and 16 KiB ZIP/ELF alignment probes. Repeat the native emulator smoke after the final product source freeze. A passing build remains separate from physical-device gameplay, thermal, signing distribution and store evidence.

Observed locally on 25 October 2026: generated wrapper JAR matches the official digest, the wrapper reports 9.8.2 on JDK 17, all seven tooling regression tests and static verification pass, both APK variants/ABIs build, strict `lintDebug`/`lintRelease` pass, exact shader/runtime/orientation package audits pass, and the Android API 36 software-Vulkan native flow passes all 280 frames with the required screen mask. Later product-only changes require their own final artifact refresh; no Gradle checker was suppressed.
