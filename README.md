# flexar

Native C++20 image/video node editor with Dear ImGui and Direct3D 12 compute effects.

## Launch

Run **`build-pixel/Release/flexar-canvas.exe`**. This is the latest gothic redesign. Keep `shaders` and `assets/fonts` beside it. Earlier builds are left alone because an older editor may still be open.

The app starts with an empty source. Supplied reference artwork is **not loaded, listed, copied into builds, or used as project content**. Its original folder is untouched and excluded by `.gitignore`.

## Workspace

- The title is simply **flexar**, set in **GothicByte**. Warm ivory text, burgundy accents and a dark graph canvas form the theme. Controls use the open-source Pixelify Sans pixel font.
- The graph occupies the main workspace. Source and Output nodes show live thumbnails for images and video.
- Toggle **Side preview** for a larger preview and node controls. Double-click an effect to reveal its controls if hidden.
- **IMAGE / VIDEO** chooses the import dialog type. Click **Import image/video**, or double-click Source Clip. Switching the button does not convert or replace loaded media.
- **Play / Pause**, Space, the timeline and `|<` control playback. Still images can animate time-dependent effects.
- **Look library** contains ten coded presets (`x000cc` through `x009ir`). Hover a code for its description. Choosing a preset replaces the graph.
- **+ Add node**, or right-click the canvas, opens the 28-effect catalog. Nodes are inserted before Output.
- Drag output to input pins; right-click an input to disconnect. Cycles are rejected.
- Drag nodes, middle-drag to pan, wheel to zoom, **Fit** to frame, **Arrange** to lay out rows.
- **Open/Save graph** stores source path, nodes, parameters, positions, enabled states and exact input connections. Version 2 supports Blend A/B and reads version 1 files.

## Effects and blending

28 nodes: brightness/contrast/saturation, Gaussian blur, halftone, distressed Xerox, duotone, grain, liquid warp, neon trails, paper texture, vignette, outline, RGB matrix, **Blend**, chromatic split, posterize, pixelate, solarize, CRT scanlines, mirror fold, sharpen, bloom and ink threshold.

**Blend** takes **A / base** and **B / layer**. Its 12 modes are Normal, Multiply, Screen, Overlay, Soft Light, Hard Light, Difference, Exclusion, Add, Subtract, Darken and Lighten. Opacity combines with B's alpha using straight-alpha source-over compositing.

On insertion, A receives the previous final effect and B the original source. Reconnect either to blend other branches. Bypass returns A. An enabled Blend with a missing input disconnects Output. Independent second-media layers are not implemented.

## Build / checks

Requires Windows 10/11, a Direct3D 12 GPU, and Visual Studio C++ Build Tools with CMake and a Windows SDK. Dear ImGui 1.91.8 is vendored with its MIT license.

```powershell
./bootstrap.ps1
```

Builds `build-pixel` Release and runs CTest. Close this executable before rebuilding, or pass `-BuildDirectory`.

- **graph_tests**: directions, cycle rollback, replacement, pruning, deletion, presets, Blend branches and effects.
- **media_tests**: H.264 fixture, conversion, duration, seeking, frame retention and EOF.
- **gpu_smoke**: pixel readback comparisons of all 12 blend modes with partial alpha and a branched Gaussian pass; bypass/missing-input behavior; Blend project roundtrip; every preset and all 22 effects.

## Font

GothicByte by HipFonts is included unmodified for this local personal prototype. The download is **personal-use-only**; obtain the author's commercial license before commercial use/distribution. Source and license link: `assets/fonts/LICENSE.txt`.

## Current limits

Media Foundation decodes video to CPU RGB32 before upload. Effects remain on the GPU. Zero-copy decode, WASAPI audio, keyframes, undo/redo, HDR and independent multi-media layers remain unimplemented. Codec availability determines format support; H.264 is tested. Images use a 1920-pixel preview edge; videos are capped at 4096 pixels per axis. Graphs are capped at 32 nodes. FPS depends on hardware and workload; 60+ FPS is not guaranteed.

See `docs/Architecture.md` for the implementation design.

## Export

Use EXPORT in the navbar: PNG saves the processed current frame with alpha; MP4 renders the full source video at 30 FPS with all graph effects. MP4 is H.264 at 16 Mbps, silent, with transparency composited over black and odd dimensions padded to even dimensions. Export uses the current preview resolution (images may have been scaled on import). Progress and Cancel remain available; cancel keeps a playable partial MP4. The original playhead/image returns after export. Pixelify Sans is licensed under the included SIL Open Font License.

## Preview player and light effects

Preview and Controls are independent bordered panels. Click Expand / Play or the preview image to open a resizable player with Resume/Pause, Restart, timeline, Original comparison, full-screen toggle, and title-bar close button. Escape closes the player. Playback shares the main playhead.

Six original Flexar effects extend the catalog: Light Rays, Star Streaks, Camera Shake, Lens Distortion, Prism Dispersion, and Film Flicker. These are independent shaders inspired by common compositing techniques, not commercial Sapphire plugins. All expose adjustable parameters and work in preview and export.

The graph backdrop is original pixel-snapped vector linework inspired by gothic castle/sigil references; no reference image is loaded. Preview visibility is controlled only by Preview ON/OFF. Click an effect to open Controls independently; its X closes it. The app uses a local ivory crosshair with rose press feedback, without changing Windows cursor settings.

Latest UI: Poison mountain/star/spire parallax port replaces the castle background. Body font is bundled ProggyClean; GothicByte headings remain. Preview is fixed at 320 x 310 and controlled by its toolbar button. Every node opens a movable, resizable Node Controls window with a title-bar X, independent from Preview.

Hold Space and left-drag to pan anywhere on the canvas, including over nodes. Middle-drag also pans. Space no longer toggles playback. Double-click any node (including Source/Output) to open, focus and recover its Controls window. Import media using the toolbar or Source Controls.

## GitHub checkout

Run ./bootstrap.ps1 after cloning. The personal-use GothicByte binary is excluded from Git; bootstrap downloads it from its published source. Its license note is tracked. Build output, generated test fixtures, and personal reference images are excluded. The current native C++ implementation is on main; the earlier Rust implementation remains on dpbuild.
