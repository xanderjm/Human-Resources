# Launcher icon handoff

Status: **approved authoring candidate**, 25 October 2026. The root agent inspected the actual small-size sheet and relayed coordinator approval. This icon derivation is separate from the 92 level/game assets. It changes no game model, paint, rig, gameplay, CMake file, mobile manifest, generated app project or production inventory.

## Source and appearance

The source is the existing approved `assets/models/ufo-modular/ufo-modular.glb`, in its rest configuration. The authoring tool decodes its actual triangles, node transforms, embedded painted textures and base-colour material factors. The existing CPU triangle reviewer supplies simple diffuse shading. No new model, ImageGen request, external illustration, text, beam, glow or decorative artwork is involved.

The cobalt hull, mint lenses and painted dome are the identity. A plain full-bleed ivory `#F2EDDE` background follows the approved style-guide presentation. Camera direction `[1,3,2]` makes the deck readable at launcher scale. The Android monochrome candidate derives its silhouette and detail cutouts from actual rim-lens/dome-bearing geometry; it preserves the antenna and matches the colour layer placement. The small-size sheet includes actual 20, 29, 40, 60, 80 and 120px reductions plus 48px Android mask checks.

## Rebuild and validate

Run from the repository root with the existing bundled NumPy/Pillow interpreter:

```sh
/Users/coral/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 tools/build_app_icon.py
/Users/coral/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 tools/build_app_icon.py --verify-only
```

`assets/graphics/app-icon/recipe.json` is the reproducible source recipe. Its pinned model hash rejects a silently changed UFO. Rebuild requires no network access. `review.json` binds the reported root/coordinator approval to the exact recipe and contact-sheet hashes; changed imagery returns the validation to pending review. The tool is authoring-only and adds no runtime dependency.

## Exact outputs

Source-side derivation masters:

- `assets/graphics/app-icon/app-icon-1024.png` — opaque RGB flattened iOS candidate.
- `assets/graphics/app-icon/ufo-foreground-1024.png` — transparent painted UFO layer, suitable for later Icon Composer import.
- `assets/graphics/app-icon/background-1024.png` — opaque ivory background layer.
- `assets/graphics/app-icon/adaptive-foreground-432.png` — transparent 108dp Android layer at xxxhdpi.
- `assets/graphics/app-icon/adaptive-monochrome-432.png` — aligned white/alpha themed layer.
- `assets/graphics/app-icon/small-size-review.png` — approved review evidence, never a launcher resource.
- `assets/graphics/app-icon/platform-sources.json` — checked official documentation URLs and fetched-document hashes.

Generated iOS catalogue:

- `build/app-icon/ios/Assets.xcassets/Contents.json`
- `build/app-icon/ios/Assets.xcassets/AppIcon.appiconset/Contents.json`
- `build/app-icon/ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png`

Generated Android resource candidates under `build/app-icon/android/res/`:

| Directory | Files | Dimensions |
| --- | --- | --- |
| `drawable-mdpi` | `ic_launcher_foreground.png`, `ic_launcher_monochrome.png` | 108×108 |
| `drawable-hdpi` | same | 162×162 |
| `drawable-xhdpi` | same | 216×216 |
| `drawable-xxhdpi` | same | 324×324 |
| `drawable-xxxhdpi` | same | 432×432 |
| `drawable` | `ic_launcher_background.xml` | opaque colour shape |
| `mipmap-anydpi-v26` | `ic_launcher.xml` | foreground/background adaptive resource |
| `mipmap-anydpi-v33` | `ic_launcher.xml` | adds monochrome for themed icons |
| `mipmap-mdpi/hdpi/xhdpi/xxhdpi/xxxhdpi` | `ic_launcher.png` | flattened fallback 48/72/96/144/192px |

All generated project-ready candidates stay under ignored `build/`; they have not been copied into any mobile project. Rebuild them rather than treating disposable generated files as authorities.

## Platform rules checked against official documentation

Checked live on 25 October 2026:

- Apple permits a flattened icon and prefers layered Icon Composer designs. Use a **1024×1024 square**; leave masking to the system. A full-bleed background is opaque. The delivered flattened PNG is RGB, with no transparency or baked rounded corners. [Apple app-icon guidance](https://developer.apple.com/design/human-interface-guidelines/app-icons)
- Xcode supports generating iOS variations from a single 1024px image. The delivered catalogue uses that single-size universal iOS form. Layered `.icon` creation and appearance/system-effect tuning are separate later work; no fabricated Icon Composer package is supplied. [Xcode asset-catalogue configuration](https://developer.apple.com/documentation/xcode/configuring-your-app-icon)
- Android adaptive layers use **108×108dp**, with important logo content between **48 and 66dp** and kept in the safe central area. Colour foreground/background and a monochrome layer support launcher masking and theming. This candidate conservatively keeps every visible foreground pixel within a **33dp radius**, uses no pre-applied mask or outline shadow, and supplies transparent foreground/monochrome with an opaque background. [Android adaptive-icon specification](https://developer.android.com/develop/ui/views/launch/icon_design_adaptive)

Masks in the contact sheet are illustrative previews. OEM masks and platform lighting differ; those preview masks are never baked into the exported layers. Do not crop the 108dp foreground to its alpha bounds or stretch it to fill the canvas. That would invalidate safe-area placement.

## Actual checks and evidence

- Offline validation passes: 1024px opaque RGB/sRGB iOS master; unmasked background corners; RGBA adaptive alpha and clear layer edges; expected density dimensions; every catalogue image exists; Android XML parses.
- Android actual foreground extent is **63.75×53.25dp**; outermost visible-pixel radius is **32.65348dp**, below33dp.
- A second full render/export reproduced all **28 canonical recipe/image/catalogue/resource files byte-for-byte**. `build/app-icon/reproducibility.json` records hashes and verifies that a wrong source-model hash is rejected.
- `build/app-icon/validation.json` records PNG/XML checks, approved-candidate review and output hashes. `build/app-icon/rebuild.log` captures the reproducible export run.
- Standalone Apple `actool` catalogue compilation and Android AAPT2 resource compilation were exercised, and the root reports independent coordinator approval of current-candidate asset generation. These are asset-tool checks, not signed app/device/store qualification.
- The shared aggregate repository gate was not rerun for this isolated authoring pass; no native product source or build integration changed. The coordinator owns the next integrated build gate.

Standalone verification commands, after rebuilding:

```sh
mkdir -p build/app-icon/ios/compiled
xcrun actool --compile build/app-icon/ios/compiled \
  --platform iphoneos --minimum-deployment-target 17.0 \
  --app-icon AppIcon --target-device iphone --target-device ipad \
  --output-partial-info-plist build/app-icon/ios/assetcatalog_generated_info.plist \
  --warnings --notices --output-format human-readable-text \
  build/app-icon/ios/Assets.xcassets
/opt/homebrew/share/android-commandlinetools/build-tools/36.0.0/aapt2 compile \
  --dir build/app-icon/android/res \
  -o build/app-icon/android/compiled-res.zip
```

`actool` may need normal access to Xcode/CoreSimulator services outside a restricted filesystem sandbox. No app launch or store submission is performed by these commands. AAPT2 compilation alone does not establish manifest references, resource linking or APK/AAB installation.

## Mobile integration remaining

The mobile qualification owner should add a durable generator/build step and allowlist only consumed launcher resources. For iOS, stage the catalogue, select `AppIcon` in owned build configuration and verify it in the resulting installed app; optionally import the PNG layers into Icon Composer for modern layered/default/dark/clear/tinted treatment while retaining deployment compatibility. For Android, stage the resource tree, bind the owned manifest/build references to `@mipmap/ic_launcher` (and round-icon handling as appropriate), then link/package and inspect adaptive/themed rendering on launchers.

No product project or manifest was edited here. Signed installation, physical-device Home Screen/Settings/notification appearance, dark/tinted/themed behavior, OEM masking, platform store assets and submission remain separate qualification work. Review preserves the painterly UFO; it does not qualify gameplay, performance or the rest of the game art.

## Source and master hashes

| File | SHA-256 |
| --- | --- |
| `assets/models/ufo-modular/ufo-modular.glb` | `5026c93d9ca212052f0eeaf5ba9e69e7a9eb63528f65c52f95be5664fae46e69` |
| `assets/graphics/app-icon/recipe.json` | `bde67280ff1bccd5b3a91d3fa60f573ff09599066cf3aa0c52e370ffe03659f8` |
| `assets/graphics/app-icon/app-icon-1024.png` | `06850486191066507b20350ff1f2c450c3ff8c2874e3291cb5dd63388dc49396` |
| `assets/graphics/app-icon/ufo-foreground-1024.png` | `1e99a6bdcf8087bc91f71b2585991e2e4c2edf091e2701d2b73ff13a2edf8aaa` |
| `assets/graphics/app-icon/background-1024.png` | `422f410d35914cbf3634f56379f402427c70d436a44d4365a5d7a5078cbe2670` |
| `assets/graphics/app-icon/adaptive-foreground-432.png` | `78a30dfcf8fac7bc6fa8b83713017828b521cfbc0bb39eb421486e72083505e9` |
| `assets/graphics/app-icon/adaptive-monochrome-432.png` | `981d30e80a8cde8338bf26b36815998ecb9d8d770e3dc225f056b2ac0271aa16` |
| `assets/graphics/app-icon/small-size-review.png` | `54c564076c504ab27bc4bf229de73c952fa2774a97aefa855120ac9652d8dae1` |
