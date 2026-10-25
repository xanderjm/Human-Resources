# Human Resources product audio

`human_resources::feedback::Mixer` owns the original procedural cue palette and
stereo PCM synthesis. It has no SDL, gameplay, asset or storage dependency. No
sound recordings, middleware, generated media or downloaded assets are required.
`platform::ProductAudio` owns the optional SDL3 playback stream and its lifetime.
The client owns semantic triggers, the saved sound setting and foreground policy.

The palette uses a quiet layered tractor hum, rising capture resolution, short
laser sweeps, muted impact/noise, coral-warning pulses and brief harmonic rewards.
It intentionally leaves space between events: capture/fire are rate limited,
warning repeats at most every 1.2 seconds, and important feedback ducks the hum.
This is a restrained first sound-design implementation; contract tests establish
signal safety and behavior, not creative acceptance or mobile speaker quality.
No music or physical haptic implementation is included.

The mixer generates 48 kHz float stereo into caller-owned memory. Sixteen fixed
voices include four slots reserved from ordinary cues for damage/death/upgrade
feedback. Full pools drop incoming cues. Voice envelopes have smooth attack and
release; panning is bounded and equal power, gain is finite and bounded, and the
final output uses smooth saturation. The low hum ramps over approximately 75 ms.
Nothing in audio mutates game state or consumes gameplay RNG.

The SDL adapter runs on the main thread and queues at most six 256-frame blocks
per update (32 ms of application-side PCM). It uses no application audio callback,
shared mutable mixer state, realtime locks or per-frame heap allocations. SDL owns
its internal conversion/device processing. Output device and OS buffers can add
latency beyond the application queue. Stalled frames cannot queue stale seconds
of sound. Pausing, backgrounding or disabling sound pauses the device and clears
queued PCM, voices and tractor state; resuming waits for fresh semantic requests.
The client must destroy this owner before `SDL_Quit`.

`initialize()` returns false when the optional subsystem/device cannot open.
Playback failures close the device; gameplay remains operable. SDL's default
playback device handles normal default-device routing; native interruptions and
physical iOS/Android speaker/headset behavior still require device evidence.
The pause API means application suspension: menu/upgrade screens should stop the
tractor but keep audio unpaused if they need choice/confirmation sounds.

Focused verification:

- `audio_feedback_contracts`: every cue emits finite bounded PCM, starts at zero,
  ends in silence and stays within a waveform-step bound; dense cue stress,
  voice limits, cooldowns, panning, invalid values, mute and tractor fades.
- The test binary accepts an optional WAV output path for listening evidence.
  Use an ignored location such as `build/audio-feedback/cue-review.wav`.
- Development evidence includes an SDL dummy-driver device/pause/mute/destructor
  smoke and an unavailable-driver fallback smoke. These prove adapter mechanics,
  not real sound-device playback or creative acceptance.

The API follows the pinned SDL headers and official SDL3 documentation:
[device stream](https://wiki.libsdl.org/SDL3/SDL_OpenAudioDeviceStream),
[queued input bytes](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamQueued), and
[pause](https://wiki.libsdl.org/SDL3/SDL_PauseAudioStreamDevice).
