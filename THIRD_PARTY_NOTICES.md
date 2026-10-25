# Third-Party Notices

The repository bootstraps third-party sources under `external/`; their own license files are authoritative and must be included as required in distributed source or binary packages.

- SDL: zlib licence.
- SDL_shadercross: zlib licence; recursive compiler dependencies carry separate notices.
- Gradle wrapper 9.8.2: Apache License 2.0, build tooling only. Vendor `LICENSE` and `NOTICE` are retained under `android/gradle/wrapper/`; the wrapper and distribution are excluded from game packages.
`python3 tools/project.py package --platform <host>` copies the exact checked-out runtime dependency licences into the desktop package. Product assets may introduce additional licence obligations; record those before distribution.
