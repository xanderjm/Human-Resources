# Optional mobile haptics

`platform::ProductHaptics` owns optional native feedback. The portable
`feedback::HapticGate` owns rate limits; neither owns gameplay, random numbers,
saved state or event generation. The client consumes fresh bounded presentation
events and requests the highest-priority capture/shield/hull cue in each frame.
Upgrade selection and a real death transition request separate intentional cues.

The existing schema-1 `Settings.haptics` preference is saved through AccountSession.
Supported devices expose an On/Off settings toggle. Unsupported devices show a
disabled Unavailable control with matching accessibility semantics. Disabling the
preference or leaving an active foreground run/upgrade/death screen cancels pending
native work. Pause, settings, protected-save errors and background do not vibrate.
Ordinary gameplay remains operable without this capability.

Capture feedback is capped at one pulse per 300 ms; shield/hull feedback at one
per 180 ms; upgrade feedback at one per 300 ms; death feedback at one per second.
A global 100 ms separation suppresses weaker incidental events, while urgent
damage/death can preempt a capture. Paused/disabled requests are discarded and
never replayed after return. There is no laser pulse, continuous rumble, or
per-target pending queue.

The iOS implementation uses UIKit impact generators and checks the system's
CoreHaptics hardware capability once during initialization. It runs only on the
main thread while UIApplication is active. No custom haptic engine/pattern assets
or service SDK are used. Apple's hardware query and impact API are documented in
[Preparing your app to play haptics](https://developer.apple.com/documentation/corehaptics/preparing-your-app-to-play-haptics)
and [UIImpactFeedbackGenerator](https://developer.apple.com/documentation/uikit/uiimpactfeedbackgenerator).

Android uses the existing SDL JNI activity boundary and a small Java UI-thread
adapter. It checks for a vibrator, then requests semantic system feedback using
`View.performHapticFeedback`, with API-floor fallbacks for the project's API 26
minimum. It uses no ignore-setting flags and needs no new VIBRATE permission.
There is at most one queued UI task; more urgent cues replace its pending value.
Lifecycle epochs, real Activity foreground state, window focus and a 150 ms
expiry reject stale requests, including native/UI pause races. The OS decides
whether its own touch-feedback settings allow playback. See
[Android haptic feedback guidance](https://developer.android.com/develop/ui/views/haptics/haptic-feedback)
and the [SDL JNI activity contract](https://wiki.libsdl.org/SDL3/SDL_GetAndroidActivity).
Desktop profiles deliberately report unavailable.

Verified on 25 October 2026: optimized host platform compilation; portable haptic
rate/priority/disable/pause/rollback/dense-stream tests; supported/unavailable
settings semantics and existing phone safe-area tests; static repository audit;
iPhoneOS arm64/iOS 17 Objective-C++ syntax compilation; Android NDK API 26 arm64
strict native syntax compilation; Java 17 compilation against Android 36 with
all lint warnings treated as errors. These do not establish mobile linking,
Android Lint, real motor feel, system-setting behavior or physical lifecycle
quality. The combined platform qualification pass owns that evidence.
