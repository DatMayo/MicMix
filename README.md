# MicMix

## Important Disclaimer

**This project was developed with help from Codex, an AI coding agent by OpenAI.**
AI-assisted development can introduce mistakes, edge-case bugs, or unintended behavior. Use MicMix with caution, especially in production-like or critical setups.

The full source code is public. **Please review the code**, verify behavior, and report bugs, regressions, or security concerns.

![MicMix Logo](assets/branding/MicMix%20Logo.png)

MicMix is a Windows plugin for TeamSpeak 3 that mixes a second audio source (music, browser audio, media player audio, etc.) into your microphone stream.

It is built for people who want to speak and play audio in the same voice channel without setting up complex virtual cable chains.

## Screenshot

<img width="682" height="812" alt="Screenshot 2026-03-16 110017" src="https://github.com/user-attachments/assets/a6bb4c2d-27be-468c-b5be-9857590867da" />

## What MicMix Can Do

- Mix your mic + one selected audio source into TeamSpeak voice transmission.
- Capture audio either from `Loopback` (system playback device) or `App Session` (a specific running app process, for example Spotify, browser, VLC).
- Control music level with a gain slider.
- Mute/unmute music quickly with a hotkey.
- Mute/unmute mic input separately (button + hotkey).
- Keep sending music even while you are not talking (`Force TX`).
- Show live level meters for music and microphone.
- Show clip strips and clip event counters for music and mic paths.
- Monitor the mixed output locally (`Monitor Mix`) while connected.
- Auto-start source capture on plugin startup (optional).
- Reacquire the selected source automatically after runtime errors.
- Save settings and restore them on next launch.
- Publish runtime activity tag `micmix_active=<0|1>` into TeamSpeak client metadata.

## Typical Use Cases

- DJ in TeamSpeak: play music for your channel while still talking normally.
- Roleplay or gaming immersion: use it as an in-car radio replacement in RP servers.
- Community events: stream intro music, ambience, or short jingles.
- Watch parties: share browser or media app audio while coordinating by voice.

## How It Works (Simple + Technical)

MicMix hooks into TeamSpeak's captured-voice callback and edits outgoing voice frames before they are sent.

Technical basics:

- TeamSpeak plugin API `26`.
- Windows audio capture via WASAPI.
- Internal mix target at `48 kHz` mono.
- Ring buffer between capture thread and voice callback.
- Resampling done with `speexdsp` when source sample rate differs.
- Source state handling includes start, stop, running, and automatic reacquire after errors.

## Server Operator Tag (`micmix_active`)

MicMix writes a runtime marker into TeamSpeak client metadata:

- `micmix_active=1`: MicMix currently detects active music signal being sent.
- `micmix_active=0`: no current active music signal.

Implementation details:

- The value is written to `CLIENT_META_DATA` and merged into existing metadata as a `;`-separated key-value pair.
- Existing metadata tokens are kept, only the `micmix_active` token is updated/replaced.
- Updates are throttled to avoid metadata spam, so there can be short delay on state transitions.

How server operators can use it:

1. Read `client_meta_data` via ServerQuery or a bot.
2. Parse for token `micmix_active=1`.
3. Use it as a signal for UI/badges, logging, moderation hints, or music-bot/channel automation.

Example logic (pseudo):

```text
if "micmix_active=1" in client_meta_data:
    mark_client_as_music_source()
else:
    clear_music_source_marker()
```

Important: this tag is client-published metadata and should be treated as a helpful signal, not a hard security control.

## Requirements

For users:

- Windows 10 or Windows 11 (64-bit recommended).
- TeamSpeak 3 Client `3.6.2`.
- TeamSpeak plugin support (API `26` compatible).

Recommended setup:

- Use headphones to avoid feedback/echo.
- Choose the correct source device or app process in MicMix settings.
- Keep source volume moderate and fine-tune with MicMix gain.

## Quick Start (User)

1. Install the `.ts3_plugin` package (double-click it, then confirm in TeamSpeak).
2. Open TeamSpeak and enable `MicMix` in Plugin settings.
3. Open `MicMix Settings...` from the plugin menu.
4. Select exactly one audio source: output device (`Loopback`) or running app (`App Session`).
5. Click `Enable MicMix`.
6. Set music gain and test levels in a channel with a friend.
7. Optional: enable `Force TX` if you want music to continue while you are silent.

## Build From Source (Developer)

Prerequisites:

- Windows + Visual Studio 2022 or newer, or the matching Build Tools (workload "Desktop development with C++"). The script picks the matching CMake generator automatically.
- CMake `>= 3.21` (the copy bundled with Visual Studio is detected automatically).
- Git.

The TeamSpeak 3 Plugin SDK and SpeexDSP are fetched automatically into `third_party/` at pinned commits.

### One-click build

Double-click `build.cmd`, or run from PowerShell:

```powershell
.\scripts\build.ps1
```

This fetches dependencies, configures CMake, builds Release, runs the unit tests and creates
`dist/MicMix-<version>-win64.ts3_plugin`.

Useful options:

| Option | Effect |
| --- | --- |
| `-Configuration Debug` | Debug build (no package) |
| `-Clean` | Delete `build/` before configuring |
| `-SkipTests` | Skip building and running the unit tests |
| `-NoPackage` | Do not create the `.ts3_plugin` |
| `-Version 1.3.0` | Override the version from `VERSION` |
| `-Install` | Copy the plugin into `%APPDATA%\TS3Client\plugins` (close TeamSpeak first) |

CI and the release workflow use the same script.

### Manual build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\package_ts3_plugin.ps1 -Configuration Release
```

## Releasing

Releases are cut from the commits since the last `vX.Y.Z` tag. The next version follows
[`VERSIONING.md`](./VERSIONING.md) and Conventional Commits: a breaking change bumps MAJOR, any `feat` bumps
MINOR, everything else bumps PATCH.

**From GitHub (recommended):** Actions → `Release` → `Run workflow`, choose `auto` (or `patch` / `minor` / `major`).
The workflow updates `VERSION`, `CMakeLists.txt` and `CHANGELOG.md`, commits `chore(release): vX.Y.Z`, tags it,
builds and tests the plugin and publishes a GitHub release with the `.ts3_plugin` and its SHA256.

**Locally:**

```powershell
.\scripts\release.ps1 -DryRun   # show next version and release notes
.\scripts\release.ps1 -Push     # build, commit, tag and push; GitHub publishes the release
```

Notes written under `## [Unreleased]` in `CHANGELOG.md` become the release notes. Without them, the notes are
generated from the commit subjects (Added / Fixed / Changed).

## Settings Window

The settings window uses a sidebar with five pages: `Source`, `Mix`, `Hotkeys`, `Appearance` and `Diagnostics`.
The status card at the top shows what MicMix is doing and holds the master On/Off switch.

`Mini mode` (bottom of the sidebar) switches to a compact window with a power button, the current source,
music volume, level meters and mute buttons. Pin it to keep it above other windows; the expand button returns
to the full window.

## Appearance (Dark Mode)

The `Appearance` page has a theme selector:

- `System` (default) follows the Windows setting *Settings → Personalization → Colors → Choose your app mode* and switches live.
- `Light` / `Dark` force a theme.

The choice is stored as `ui.theme=system|light|dark` in the MicMix config.

## Status

MicMix is in active development. Core mixing, source selection, UI controls, hotkey support, and config persistence are already implemented.

See [`DEVPLAN.md`](./DEVPLAN.md) for architecture details, roadmap, and test strategy.
