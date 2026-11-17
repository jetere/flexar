# Flexar architecture and implementation status

## Components

| Module | Responsibility |
|---|---|
| `src/Graph.h`, `Graph.cpp` | Nodes, typed/directional pins, connections, effect metadata/defaults, presets, DFS topological order and cycle checking |
| `src/Renderer.h`, `Renderer.cpp` | Win32 swapchain, D3D12 device/queue/fence, upload texture, two reusable effect textures, SRV/UAV descriptors, compute dispatch, ImGui rendering |
| `shaders/Effects.hlsl` | Effect kernels selected by numeric effect ID; parameters mapped into root constants |
| `src/Media.h`, `Media.cpp` | WIC image decoding/scaling and synchronous Media Foundation RGB32 source-reader playback |
| `src/Main.cpp`, `EditorUI.inl` | Win32 loop, editor state, clock, gothic graph UI, thumbnails, inspector, dialogs and persistence |
| `src/GpuValidation.inl` | Actual GPU pixel checks for blending, alpha and branch resource lifetime |

## Actual frame path

```text
Image file -> WIC RGBA conversion / preview scaling ---+
                                                     |
Video file -> MF source reader -> CPU BGRA -> CPU RGBA +-> D3D12 upload -> source texture
                                                                            |
                            reachable graph in topological order <-----------+
                                           |
                        compute shaders: liveness-managed texture pool
                                           |
                                final SRV -> ImGui -> swapchain
```

The source texture is preserved for comparison and the Source Clip thumbnail. Effects use SRV inputs t0/t1 (t1 is used by Blend), one UAV output, a linear-clamp sampler and 20 DWORD root constants (effect ID, dimensions, time, 16 parameters). Dispatch size is 8x8 with bounds checks. Textures use RGBA8 UNORM. Gaussian blur uses horizontal/vertical passes. Resource transitions and a frame fence protect memory ordering and resource lifetimes.

Blend has two inputs; other effects are unary. The evaluator counts remaining consumers, reuses only textures with no live references, and accounts for bypass aliases and Gaussian intermediates. Chains reuse two work textures while forks retain their results through the merge. Enabled Blend requires A and B; bypass requires only A. The final texture remains valid for presentation. Explicit CPU readback is used only for validation, never normal preview.

The preview cache hashes effect IDs, parameters, enabled state, connections and playhead time; source uploads invalidate it. Graph position changes do not invalidate rendered pixels. A disconnected Output yields an explicit empty-viewer message. The renderer does not silently substitute the source when graph wiring is incomplete.

## Graph correctness

Each node and pin receives a monotonically increasing 64-bit document-local ID. Connections reference pins. `connect` validates existence, direction, type and self-edges, tentatively replaces the target input's connection, then rolls back if DFS detects a cycle. Removing a node removes incident connections first. Execution prunes nodes that cannot reach Output. Project loading constructs a separate graph and validates it before replacing the editor's graph.

Parameter ordering in `Graph.cpp` matches HLSL. Effect enum values are stable shader/project IDs. Text `.flexar` version 2 stores a quoted source path, node records, and edges with source/destination node IDs plus destination input index. Version 1 edges default to input zero. The loader remaps IDs. Reference artwork is never embedded or loaded automatically.

## Playback

The application uses `std::chrono::steady_clock` (the Windows implementation uses a high-resolution monotonic clock). Playback advances by elapsed wall time and loops at clip duration. Source-reader seek requests use 100 ns timestamps; decode runs through keyframe preroll and retains one future sample so it is not presented early. Backward or large jumps trigger seeks. Frame dimensions/stride are refreshed on normal Media Foundation type-change notifications. CPU copies convert BGRA to RGBA while respecting signed stride.

Synchronous decode and seek can block UI interaction on difficult media. The next playback iteration should move decoding into a worker with a bounded queue, cancellation/generation IDs for seeks, and a separate GPU upload ring. Audio is absent, so the current clock is not audio-mastered.

## Zero-copy correction and future work

Media Foundation's standard `IMFDXGIDeviceManager` associates a **Direct3D 11** device, not an `ID3D12Device`. See [Microsoft's device-manager documentation](https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nn-mfobjects-imfdxgidevicemanager) and [D3D11-aware decoder guidance](https://learn.microsoft.com/en-us/windows/win32/medfound/supporting-direct3d-11-video-decoding-in-media-foundation).

The production path must choose and validate either D3D11 video decode with shared-resource interoperability and explicit cross-API synchronization, or a dedicated D3D12 Video decode backend plus demuxing and codec-specific bitstream management. NV12/P010 plane handling, video processor/color conversion, adapter capability queries, queue fences and format support must be implemented before describing the pipeline as zero-copy. Installing a codec does not imply a GPU supports that codec/profile/resolution.

Remaining roadmap:

1. Decode worker, cancellation, frame-upload ring and measured GPU/CPU timings.
2. D3D11/D3D12 decode interop or dedicated D3D12 Video implementation, with explicit fallback reporting.
3. WASAPI audio rendering and audio-clock-driven video scheduling.
4. Independent multi-media sources, masks, keyframes, undo/redo and relinking. Two-input branch blending is implemented.
5. Export, color-space metadata, HDR handling, device-removal recovery and capability-based effect limits.
6. Target-hardware performance tests at representative 1080p/4K resolutions. Report median/p95 frame times and dropped frames rather than assuming a GUI framework guarantees 60/120 FPS.

No API keys, remote processing or cloud services are required at runtime.
