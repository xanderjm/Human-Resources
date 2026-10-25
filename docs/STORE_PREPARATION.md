# Store preparation and offline privacy

Local preparation for the current offline build, 25 October 2026. No console
declaration, public policy publication, distribution signing or submission is
recorded here. Recheck against the actual release artifact before submission.

## Current disclosure facts

Human Resources is an offline single-player game. Its local progression file
contains earned credits, Hangar ranks, records, run recovery and settings; it is
not an online account. No product server, advertising, analytics, cloud sync,
online sign-in or in-app purchase provider is installed. The merged Android
release manifest requests only vibration, with no INTERNET or advertising-ID
permission. Its backup/transfer rules exclude app data. iOS uses its private
Application Support directory; operating-system backups may include/restore it.
Do not promise that uninstalling erases independent backup copies.

Candidate declarations for this build:

| Store field | Prepared answer / evidence |
| --- | --- |
| Apple App Privacy | No data collected; no tracking. Local-only processing is not developer collection under Apple's definition. |
| Google Data safety | No user data collected or shared by this app. Do not claim transport encryption for a nonexistent network service. |
| Contains ads | No. Portable reward rules/test providers are not a live ad integration. |
| App access | No login or restricted online account. Features and earned progression are available through ordinary play. |
| Account deletion | No online account creation. Local saves/settings and independent device backups must be described accurately. |
| Intended audience | Teens and adults, 13+, owner confirmed. Google target groups 13–15, 16–17 and 18+ are candidates; some locales consider older teens children. |
| Content ratings | Complete the real questionnaires: frequent cartoon/fantasy combat and guns/rockets, human/animal abduction, no paid random goods; random run upgrades are earned through play. Do not copy Archero's rating or claim an assigned rating. |

Apple's current rating definitions place frequent cartoon violence/weapons in
13+, but questionnaire outcomes vary by region and older OS. This is an
inference for preparation, not an issued rating.

## Required-reason API contract

`ios/PrivacyInfo.xcprivacy` declares exactly:

- `NSPrivacyAccessedAPICategorySystemBootTime` / `35F9.1`: pinned SDL uses
  `mach_absolute_time` for in-app elapsed events/timers.
- `NSPrivacyAccessedAPICategoryFileTimestamp` / `C617.1`: own bundled asset and
  app-container metadata via `stat`/`fstat`, including SDL directory/file checks.

No tracking domains or collected-data types are declared. No UserDefaults or
disk-space reason was invented. The iOS NSUserDefaults source occurrence is
compiled out; SDL's separately compiled CFPreferences system-scroll preference
does not justify `CA92.1`, which forbids reading system-written defaults.

`scripts/verify-ios.sh` runs a narrow product audit checking exact declarations,
byte-exact source/bundle parity and the real executable's timing/file imports.
It is not a complete API scanner or a substitute for reviewing future services.
The previous empty manifest was rejected by this gate as expected; read-only
binary/source evidence is in `build/evidence/release-privacy-audit/`.

## Prepared local product materials

Settings exposes an accessible, measured, paged offline **Privacy policy**.
Its complete copy is owned by
`native/human_resources_product/src/privacy_policy.h`. It explains local storage,
no transmission/collection/sharing, absent services, device backups, retention,
deletion limits and the developer-contact path on the eventual store listing.
It does not invent a contact address or open an unconfigured website. Reading
it is transient UI; it cannot change saves or resume a paused flight.

Suggested listing copy, limited to implemented behavior:

- Name: **Human Resources**.
- Short description: **Fly a UFO, gather specimens and survive the resistance.**
- Description: **Drag to fly your modular saucer. Capture humans and animals,
  dodge military fire, and choose upgrades as your run grows. Earn Research
  Credits for permanent Hangar improvements, unlock new hardware, and discover
  powerful evolutions. Explore Farmland, Suburbia and Desert in portrait play.
  Progress and interrupted runs save on your device. Play offline.**

Do not advertise unmeasured frame rates, guaranteed run length, controller/device
coverage, live ads/revives/purchases or cloud recovery. Use genuine current-build
screenshots with representative gameplay, earned upgrades and Hangar; label any
concept art separately. Existing approved app icons are packaged, but final store
screenshots and Google artwork now have a local current-build export/selection
checkpoint. `build/evidence/store-capture-preparation/FINAL_INDEX.md` identifies
six Google 1080x1920 screenshots (including three actual active-gameplay frames)
and three each at Apple 1206x2622 and iPad 2064x2752. All thirteen source PNGs
were independently checked against opaque native GPU pixels; one crowded
alternate is retained but excluded. They are actual macOS native app output,
not physical phone execution or store acceptance. Earlier undersized sets stay
historical. A store preview video remains separate.

`tools/build_store_assets.py` reproducibly exports the existing approved UFO,
v3 wordmark and tagline into `build/store-listing/assets/`: an opaque RGBA
512x512 Google store icon, requested RGB counterpart, RGB 1024x500 feature
graphic and half-size review image. Root accepted the actual images; repeated
exports are byte-identical with bound source hashes and safe content bounds.
These are manual, nonshipping derivatives of existing assets. No new model,
generated artwork, dependency, runtime asset or public listing was added.

## Distribution gaps

Before submission, supply a real developer/store identity, privacy contact,
support endpoint and active publicly accessible privacy-policy URL. Publish the
reviewed current policy under that identity and add its real accessible in-app
link; the offline reader completes local access but does not invent the public
URL required by the stores. Populate questionnaires and disclosures in the
actual consoles against the final binary. Distribution credentials, pricing,
territories, signing, device qualification and store acceptance remain separate
release work. No placeholder URL/contact is a completed release obligation.

## Official sources checked 25 October 2026

- [Apple required-reason API policy](https://developer.apple.com/documentation/bundleresources/describing-use-of-required-reason-api)
  and [API categories/reasons](https://developer.apple.com/documentation/bundleresources/app-privacy-configuration/nsprivacyaccessedapitypes/nsprivacyaccessedapitype).
- [Apple App Privacy definitions](https://developer.apple.com/app-store/app-privacy-details/)
  and [Review Guidelines 5.1.1](https://developer.apple.com/app-store/review/guidelines/#privacy).
- [Google Data safety](https://support.google.com/googleplay/android-developer/answer/10787469?hl=en)
  and [User Data/privacy-policy requirements](https://support.google.com/googleplay/android-developer/answer/10144311?hl=en).
- [Apple rating values](https://developer.apple.com/help/app-store-connect/reference/app-information/age-ratings-values-and-definitions)
  and [Google target audience](https://support.google.com/googleplay/android-developer/answer/9867159?hl=en).
