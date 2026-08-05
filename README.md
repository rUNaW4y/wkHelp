# wkHelp

`wkHelp` is a 32-bit WormKit module for **Worms Armageddon**. During a match it provides two configurable, in-game overlays:

- a visual guide to the game controls;
- a detailed view of the currently active scheme, including weapon settings and Extended Game Options.

The default hotkeys are **H** for Help and **I** for Scheme Information. Both can be changed in `wkHelp.ini`.

## Features

### Help overlay

- High-resolution mouse, keyboard, weapon and general-control illustrations.
- English in-game control reference.
- Worms Armageddon logo and game-style typography.
- Vertically scrollable content.
- Resolution-aware layout and high-quality image scaling.

### Scheme Information overlay

The scheme overlay reads the active scheme directly from Worms Armageddon's in-memory scheme structure. This allows it to work both when hosting and when receiving a scheme as a client.

It displays:

- general match settings;
- worm, map, mine, crate and Sudden Death settings;
- game flags and weapon upgrades;
- the complete 64-entry `.wsc` weapon table with Ammo, Power, Delay and Probability;
- Skip Turn and Surrender availability;
- Extended Game Options grouped as Physics, Gameplay 1, Gameplay 2, Glitch Emulation, Input, Visual and RubberWorm.

Version 3 schemes can contain up to 110 bytes of Extended Game Options. If an older or network-transmitted scheme omits newer trailing fields, `wkHelp` uses the official Worms Armageddon defaults for those unavailable fields.

### Match and chat awareness

- Overlays are available only after the gameplay scenario has loaded.
- They do not open in the lobby.
- Opening the in-game chat with Page Down disables the overlay hotkeys and closes a visible overlay.
- Closing the chat with Page Up enables the hotkeys again.
- Starting or leaving a match resets and hides the overlay.

## Installation

1. Use a current installation of Worms Armageddon with WormKit module loading enabled.
2. Copy `wkHelp.dll` and `wkHelp.ini` into the game directory containing `WA.exe`.
3. Start or restart the game.

Typical Steam installation directory:

```text
C:\Program Files\Worms Armageddon\Team17\Worms Armageddon
```

The repository includes the latest compiled release build in `wkHelp.dll`.

## Usage

During a match, with the chat closed:

- press **H** to open or close Help;
- press **I** to open or close Scheme Information;
- pressing the other overlay hotkey switches directly between the two views;
- use the mouse wheel, Up/Down, Page Up/Page Down, Home or End to navigate long content.

The scrollbar currently provides a visual position indicator. Scrolling is performed with the mouse wheel or keyboard.

## Configuration

`wkHelp.ini` is divided into three sections.

### General

| Key | Default | Description |
| --- | --- | --- |
| `HelpHotkey` | `H` | Opens or closes the Help overlay. |
| `SchemeHotkey` | `I` | Opens or closes Scheme Information. |
| `AnimationDurationMs` | `260` | Slide animation duration. |
| `PollIntervalMs` | `10` | Input and game-state polling interval. |
| `StartOpen` | `0` | Initial overlay state; normally keep disabled. |
| `ScenarioReadyDelayMs` | `2500` | Delay after match construction before overlays become available. |

### Window

| Key | Default | Description |
| --- | --- | --- |
| `WidthPercent` | `55` | Overlay width as a percentage of the game client area. |
| `HeightPercent` | `90` | Overlay height as a percentage of the game client area. |
| `TopPercent` | `5` | Vertical offset as a percentage of the game client area. |
| `RightMargin` | `0` | Distance from the right edge. |
| `BackgroundColor` | `0,0,0` | Background RGB color. |
| `Opacity` | `245` | Window opacity from 20 to 255. |
| `Padding` | `28` | Internal content padding. |
| `ScrollbarWidth` | `14` | Scrollbar width. |
| `ScrollbarColor` | `105,105,105` | Scrollbar track RGB color. |
| `ScrollbarThumbColor` | `220,220,220` | Scrollbar thumb RGB color. |
| `MouseWheelLines` | `5` | Amount scrolled per wheel notch. |

### Text

| Key | Default | Description |
| --- | --- | --- |
| `FontName` | `Worms Armageddon` | Preferred display font. |
| `FallbackFontName` | `Arial` | Font used when the preferred font is unavailable. |
| `TitleSize` | `28` | Main title size. |
| `HeadingSize` | `20` | Section heading size. |
| `BodySize` | `16` | Body and table text size. |
| `TextColor` | `255,255,255` | Main text RGB color. |
| `MutedTextColor` | `185,185,185` | Secondary text RGB color. |
| `LineSpacing` | `7` | Spacing between text lines. |
| `TableRowPadding` | `8` | Vertical padding in table rows. |
| `KeyColumnPercent` | `28` | Width of the key column in the controls table. |

See `wkHelp_Hotkeys.txt` for every accepted hotkey name. Hotkeys are case-insensitive. Modifier combinations such as `Ctrl+H` are not currently supported.

## Diagnostics

The module creates `wkHelp.log` beside the DLL. It records:

- module and resource initialization;
- game-state hook installation;
- match start and end detection;
- overlay hotkey decisions;
- chat open/close detection;
- scheme version, weapon count and Extended Game Options byte count.

When reporting a problem, include this log and describe the W:A renderer, display mode, resolution, Windows version and the exact reproduction steps.

## Building from source

### Requirements

- Windows and a Visual Studio C++ toolchain capable of producing Win32/x86 binaries;
- CMake 3.20 or newer;
- C++20 support;
- the sibling `wkWormsTracker` source tree used by the current CMake project for `hacklib`, WormKit headers and bundled third-party libraries.

Expected checkout layout:

```text
workspace/
├── wkHelp/
└── wkWormsTracker/
```

Configure and build a 32-bit Release binary with a Visual Studio generator:

```powershell
cmake -S wkHelp -B wkHelp/build-win32 -A Win32
cmake --build wkHelp/build-win32 --config Release
```

The resulting DLL is written to:

```text
wkHelp/build-win32/Release/wkHelp.dll
```

The module links the MSVC runtime statically and uses Win32, GDI+, PolyHook 2, Capstone and the local `hacklib` sources.

## Implementation overview

- `src/main.cpp` implements configuration, input polling, the GDI+ overlay window, animation and rendering.
- `src/GameState.*` detects construction and destruction of the active match state.
- `src/scheme/SchemeInfo.*` reads and decodes standard and extended scheme data.
- `src/scheme/Hooks.*` provides the x86 hooks used by scheme detection.
- `resources.rc` embeds the Help artwork into the DLL.

The overlay is a non-activating, layered Win32 popup anchored to the W:A client area. Rendering is double-buffered and scaled relative to the active game resolution.

## Known limitations

- This build targets 32-bit Worms Armageddon and its current supported memory layouts.
- The scrollbar is not draggable yet.
- Chat detection follows W:A's Page Down/Page Up chat controls.
- Opening or interacting with a separate overlay window may behave differently with unusual fullscreen, mouse-driver or third-party overlay configurations.

## Credits and information sources

- Worms Armageddon and its artwork are property of their respective owners.
- Scheme format and game-control information were validated against the Worms Knowledge Base at [worms2d.info](https://worms2d.info/).
- The control artwork included in this project was prepared for the module from the supplied reference material.
