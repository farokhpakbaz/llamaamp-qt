# LlamaAmp Qt roadmap

LlamaAmp should feel at home on a current Linux desktop while preserving its
experimental, clean-room XML-skin runtime. Work is prioritized around reliable
listening first, discoverability second, and compatibility third.

## Current modern-desktop milestone

- [x] MPRIS playback, metadata, seeking, volume, shuffle, and loop controls
- [x] media-key and lock-screen integration through the desktop MPRIS service
- [x] searchable `Ctrl+K` command palette
- [x] keyboard commands for transport, seeking, and page navigation
- [x] accessible names and descriptions for the primary player controls
- [x] headless GUI and session-bus regression tests

## Next: listening quality

- gapless playback and configurable crossfade where the Qt backend permits it
- ReplayGain scanning and per-track/album loudness normalization
- output-device hot-plug recovery without interrupting the queue
- waveform-based seeking and an optional compact mini-player

## Next: library experience

- background metadata scanning with visible progress and cancellation
- album/artist browsing, cover-art caching, ratings, and play counts
- smart playlists based on library fields and playback history
- duplicate detection and a reversible missing-file repair workflow

## Compatibility track

- expand layout and component coverage using independently documented XML
  behavior and contributor-owned test fixtures
- add docking, windowshade, and gamma-set primitives without importing
  proprietary source or assets
- keep native Qt controls available whenever a compatible skin is incomplete

## Engineering and distribution

- Flatpak/AppStream metadata and reproducible release artifacts
- PipeWire, PulseAudio-compatibility, Bluetooth, and HiDPI test matrix
- fuzz tests for playlists, plug-in metadata, and user-supplied skin XML
- translation infrastructure and accessibility testing with Orca

Please open an issue before beginning a large roadmap item. Small, independently
reviewable changes with tests are the fastest path into a release.
