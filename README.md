# LlamaAmp Qt

LlamaAmp Qt is an independent, community-driven music player for Linux built
with Qt 6 and C++20. It combines a clean native interface with an experimental
XML-skin compatibility layer, without bundling proprietary player source,
skins, media, logos, or trademarks.

The project is open source under Apache-2.0 and welcomes contributors. See
[CONTRIBUTING.md](CONTRIBUTING.md) for development priorities and the pull
request checklist.

## Features

- local and remote audio playback and embedded video through Qt Multimedia;
- file, recursive-folder, command-line, URL, drag-and-drop, and M3U/M3U8 import;
- play/pause, stop, next/previous, seeking, volume, shuffle, and repeat modes;
- Linux MPRIS integration for media keys, lock screens, and desktop controls;
- a searchable command palette (`Ctrl+K`) plus keyboard-first queue navigation;
- selectable audio outputs with remembered settings;
- metadata, embedded artwork, session restoration, and searchable queues;
- a persistent SQLite media library;
- a 10-band PCM equalizer with presets;
- a live waveform and 48-band spectrum visualization;
- a versioned native Linux `.so` plug-in ABI and example soft-clipping DSP;
- native color themes and experimental user-supplied XML-skin support;
- desktop integration and automated Qt tests.

## Requirements

Install a C++20 compiler, CMake 3.22 or newer, and Qt 6.8 or newer with Core,
Widgets, DBus, SQL, Multimedia, MultimediaWidgets, and Test development packages.
On Debian and Ubuntu:

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-multimedia-dev
```

Codec availability depends on the Qt Multimedia backend installed by the host
distribution.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/llamaamp
```

Files, folders, URLs, and playlists can be passed directly:

```sh
./build/llamaamp ~/Music playlist.m3u8
```

Useful options include:

```sh
./build/llamaamp --list-audio-devices
./build/llamaamp --audio-output "Headphones" song.flac
./build/llamaamp --dsp --equalizer "Bass Boost" song.mp3
```

On Linux, LlamaAmp defaults to Qt's PulseAudio compatibility backend, which is
normally backed by PipeWire on modern desktops. Set
`QT_AUDIO_BACKEND=pipewire` to request Qt's direct PipeWire backend.

## User-supplied XML skins

No third-party skins are included. LlamaAmp can catalogue user-supplied legacy
XML skins and provides a partial compatibility surface for Bento-family layouts.
Only use skins you are legally permitted to use.

Place each unpacked skin directory, including its `skin.xml`, under one of:

- `~/.local/share/llamaamp/skins/`;
- the application's local data `skins/` directory;
- `skins/` beside the executable; or
- any directory listed in `LLAMAAMP_SKIN_PATH` (colon-separated on Linux).

Then select it from **Tools → XML skin browser** or with `--skin NAME`.
The runtime parses XML fragments, recursively resolves bounded includes, and
indexes bitmap, group, layout, script, accelerator, and action definitions.
MAKI execution, generic geometry instantiation, docking, windowshade, gamma
sets, and embedded browser services are not implemented.

Compatibility names refer to file formats and are not endorsements. This
project is not affiliated with the owners of any compatible application or
skin format.

## Plug-ins

The public ABI is declared in `Src/qt6/LlamaAmpPlugin.h`. Native plug-ins are
loaded from `plugins/` beside the executable, the installed
`lib/llamaamp/plugins/` directory, or the application data directory. The
included Soft Clipper is a small example DSP module.

## Test and install

```sh
ctest --test-dir build --output-on-failure
cmake --install build --prefix ~/.local
```

## Keyboard and desktop controls

Press `Ctrl+K` to search every menu command. `Space` toggles playback,
`Ctrl+Left`/`Ctrl+Right` change tracks, `Shift+Left`/`Shift+Right` seek by ten
seconds, and `Ctrl+1` through `Ctrl+5` change pages. On Linux desktops,
LlamaAmp also publishes playback state, track metadata, volume, shuffle, repeat,
seeking, and URI opening through MPRIS.

The current product direction and contributor-sized follow-up work are tracked
in [ROADMAP.md](ROADMAP.md).

## Contributing

New contributors are welcome. A reproducible bug report or documentation fix is
as valuable as a large feature. Good starting areas include accessibility,
visualizations, device testing, packaging, media-library improvements, and
clean-room XML compatibility. Read [CONTRIBUTING.md](CONTRIBUTING.md) before
submitting a pull request.

## License and independence

LlamaAmp Qt is licensed under the [Apache License 2.0](LICENSE). See
[NOTICE](NOTICE) for attribution and the independence statement.

The repository intentionally contains no third-party player source, bundled
skins, demo media, or trademarked logos. Contributors must not add assets or
code unless their redistribution terms are documented and compatible.
