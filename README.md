<div align="center">

Create cool atmospheric visuals with GPU effects, branching graphs, and a workspace.

![C++](https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg)
![Dear ImGui](https://img.shields.io/badge/GUI-Dear%20ImGui%201.91.8-7861A8.svg)
![DirectX](https://img.shields.io/badge/Rendering-DirectX%2012-107C10.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-lightgrey.svg)
![Status](https://img.shields.io/badge/Status-Prototype-b65b73.svg)

[Getting started](#getting-started) · [Controls](#controls) · [Effects](#effects) · [Architecture](docs/Architecture.md)

</div>


## Highlights

- **Image and video input** — switch import modes from the toolbar; view the source and processed output directly on their nodes.
- **28 GPU effects** — combine color, print, distortion, texture, and lighting effects with live parameter adjustments.
- **Branching graphs** — connect typed pins, blend two branches, and reject circular dependencies.
- **12 blend modes** — composite with opacity and alpha using a dedicated two-input Blend node.
- **10 coded looks** — start with presets named `x000cc` through `x009ir`.
- **Independent preview and controls** — expand the preview into a player with a timeline, original comparison, and fullscreen mode.
- **PNG and MP4 export** — save a processed frame or render the full source video.
- **Project persistence** — save media paths, connections, parameters, positions, and enabled states in `.flexar` files.

## Getting started

### Requirements

- Windows 10 or 11, with a Direct3D 12-capable GPU.
- Visual Studio C++ Build Tools with **Desktop development with C++**, CMake, and a Windows SDK.
- Git and PowerShell; internet access for the initial GothicByte font download.

### Build and launch

```powershell
git clone https://github.com/jetere/flexar.git
cd flexar
./bootstrap.ps1
./build-pixel/Release/flexar-canvas.exe
```

The bootstrap script configures an x64 Release build and runs the test suite. Dear ImGui 1.91.8 is vendored. Keep the generated `shaders` and `assets/fonts` directories beside the executable when moving a build.

Close the executable before rebuilding. To use another build folder:

```powershell
./bootstrap.ps1 -BuildDirectory build-local
```

### First graph

1. Choose **IMAGE** or **VIDEO**, then import your media.
2. Open **+ Node** or right-click the canvas to add an effect before Output.
3. Connect output pins to input pins and click a node to adjust its controls.
4. Toggle **Preview** or open **Expand / Play** to inspect the result.
5. Use **Save** for the graph or **EXPORT** for processed media.

Applying a Look preset replaces the current graph. Save your graph first if you want to keep it.

## Controls

| Action | Control |
| --- | --- |
| Move a node | Drag its header |
| Pan the canvas | Hold **Space + left-drag**, or middle-drag |
| Zoom | Mouse wheel |
| Frame or organize nodes | **Fit** / **Arrange** |
| Connect nodes | Drag an output pin to an input pin |
| Disconnect an input | Right-click its pin |
| Open parameters | Click a node |
| Recover and focus controls | Double-click any node, including Source and Output |
| Show or hide side preview | **Preview** toolbar button |
| Close controls | **X** on the Node Controls title bar |
| Playback | **Play / Pause**, timeline, and restart button |
| Large preview | Click the preview image or **Expand / Play** |
| Close large preview | **Escape** or its close button |

Space is reserved for canvas panning; it does not toggle playback.

## Effects

| Category | Nodes |
| --- | --- |
| Color and tone | Brightness / Contrast, Color Matrix, Duotone, Posterize, Solarize, Ink Threshold |
| Print and texture | Print Halftone, Distressed Xerox, Film Grain, Crumpled Paper, CRT Scanlines, Pixelate |
| Detail and finish | Gaussian Blur, Sharpen, Ink Outline, Vignette, Bloom |
| Distortion | Liquid Warp, Chromatic Split, Mirror Fold, Camera Shake, Lens Distortion, Prism Dispersion |
| Light and motion | Neon Trails, Light Rays, Star Streaks, Film Flicker |
| Compositing | Blend |

Light Rays, Star Streaks, Camera Shake, Lens Distortion, Prism Dispersion, and Film Flicker are original Flexar shader effects inspired by common compositing techniques. They are not bundled Sapphire plugins.

### Blending

Blend accepts **A / base** and **B / layer**. Available modes:

**Normal · Multiply · Screen · Overlay · Soft Light · Hard Light · Difference · Exclusion · Add · Subtract · Darken · Lighten**

Opacity combines with the layer's alpha using straight-alpha source-over compositing. A newly inserted Blend uses the preceding effect as A and the source as B; reconnect the pins to combine other branches. Bypass returns A. Both inputs are required when enabled. Branches currently share one imported media source.

## Export

| Format | Behavior |
| --- | --- |
| **PNG** | Saves the current processed frame, preserving alpha. |
| **MP4** | Renders the full source video as silent H.264 at 30 FPS and 16 Mbps. Transparency is composited over black; odd dimensions are padded to even dimensions. |

Exports use the current working resolution. Image imports may be scaled down. Video export provides progress and cancellation; cancelling retains a playable partial file. The original playhead is restored afterward.

## Architecture and validation

```text
Image / Video
     |
WIC / Media Foundation
     |
CPU frame -> GPU upload
     |
DAG evaluation -> DirectX 12 / HLSL effects
     |
Preview / PNG / H.264 MP4
```

The C++20 graph engine evaluates dependencies, while DirectX 12 executes effects and reuses GPU textures. The interface uses Dear ImGui, GothicByte headings, and ProggyClean body text. See [Architecture](docs/Architecture.md) for implementation details.

Run the checks after a build:

```powershell
ctest --test-dir build-pixel -C Release --output-on-failure
```

| Test | Coverage |
| --- | --- |
| `graph_tests` | Connections, cycles, graph changes, presets, and branching |
| `media_tests` | Image import, PNG roundtrip, H.264 encode/decode, seeking, and EOF |
| `gpu_smoke` | GPU pixel comparisons, blend modes and alpha, branching, presets, effects, and project roundtrip |

## Current limitations

Flexar is a working prototype. Video decoding currently uses CPU RGB32 frames followed by a GPU upload; zero-copy decoding is not implemented. Audio playback/export, keyframes, undo/redo, HDR, and independent media layers are not yet available.

Graphs are limited to 32 nodes. Images use a maximum 1920-pixel preview edge; videos are capped at 4096 pixels per axis. Codec support depends on the system; H.264 is tested. Performance depends on the GPU, resolution, and graph complexity.

## Fonts and third-party notices

- **Dear ImGui** is vendored under its [MIT license](third_party/imgui-1.91.8/LICENSE.txt).
- **ProggyClean** is bundled with Dear ImGui; see the embedded font notice in [imgui_draw.cpp](third_party/imgui-1.91.8/imgui_draw.cpp).
- **GothicByte** by HipFonts is personal-use-only. Its binary is excluded from Git and downloaded separately by bootstrap. See the [font notice](assets/fonts/LICENSE.txt) for source and commercial licensing details.
- **Pixelify Sans** is retained in the font assets with its SIL Open Font License.

The `main` branch contains the native C++ implementation. The earlier Rust implementation remains on `dpbuild`. Build outputs and personal reference artwork are excluded from this repository.
