# Contributing to LlamaAmp Qt

Thank you for helping build a modern, independent music player for Linux.
Contributions of every size are welcome: focused code changes, tests, bug
reports, accessibility work, packaging, documentation, and design feedback.

## Good places to contribute

- improve keyboard navigation, screen-reader support, HiDPI rendering, and localization;
- add visualization modes and optimize real-time audio processing;
- extend documented XML-skin compatibility using independently written code;
- test PipeWire/PulseAudio devices, Bluetooth routing, codecs, and video playback;
- improve packages for Debian, Ubuntu, Fedora, Arch, Flatpak, and other Linux systems;
- add regression tests for playlists, media-library data, skins, and plug-ins.

Open an issue before starting a large architectural change. Small, focused fixes
can go directly to a pull request.

## Build and test

Install CMake 3.22+, a C++20 compiler, and Qt 6.8+ with Core, Widgets, SQL,
Multimedia, MultimediaWidgets, and Test development packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
  -DLLAMAAMP_QT_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For GUI work, test the native interface and, when relevant, the XML-skin
compatibility surface using a skin you have the right to use. Include a
screenshot or short recording when the visual result matters.

## Pull-request checklist

- Keep changes focused and explain the user-visible result.
- Follow the existing C++20/Qt style and enable warnings-as-errors locally.
- Add or update tests for behavior changes.
- Run the complete test suite before submitting.
- Update documentation when commands, dependencies, features, or limitations change.
- Do not commit generated builds, credentials, proprietary media, or third-party skins.
- Only submit code and assets you created or have permission to contribute.

Please be patient and respectful during review. Technical disagreement is
welcome; harassment and personal attacks are not.

## License

Contributions are submitted under Apache-2.0 unless a file explicitly says
otherwise. By contributing, you confirm that you have the right to license your
work under those terms. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
