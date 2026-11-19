# Godot-style effects

Flexar translates a limited 2D `canvas_item` fragment language into HLSL compute shaders. This is an independent compatibility layer, not Godot's renderer or a full Godot compiler.

Use **+ Node > Godot Shader / custom**. Paste code into Node Controls and choose **Compile / Apply**. Successful compilation creates sliders for scalar uniforms. Compilation errors appear in the bottom status bar; the last valid shader remains active. **Edit shader** restores the applied code to the editor. Project files embed the applied shader source and parameter values.

Supported: one `void fragment()` function; `UV`, `COLOR`, `TIME`, `TEXTURE_PIXEL_SIZE`, `SCREEN_PIXEL_SIZE`; `texture(TEXTURE, uv)`; float/int/bool uniforms with constant defaults and optional `hint_range`; vec2/vec3/vec4, common arithmetic and HLSL-compatible functions, mix/fract/mod mappings. Up to 12 scalar uniforms and 64KB source. Integer/bool uniforms use the GPU cast of slider values.

Not supported: spatial, sky, particle, vertex or light shaders; external samplers and screen/depth textures; vector uniforms; helper functions; includes; Godot render modes, lighting, scene data or general GLSL compatibility. Unsupported syntax produces a compilation error. This is a starting subset for image effects, not a promise that arbitrary .gdshader files will work unchanged.

Bundled nodes are adaptations of these CC0 shader posts (code only; no site media included):

| Node | Original / author | Adaptation |
| --- | --- | --- |
| x010gp | [Pixelate](https://godotshaders.com/shader/pixelate/) / godotshaders | Added bounded amount slider |
| x011cp | [256 colour pixelation](https://godotshaders.com/shader/256-colour-pixelation/) / SolarGranulation | Scalar color step; quantizes all RGB values |
| x012ca | [Radial Chromatic Aberration](https://godotshaders.com/shader/radial-chromatic-aberration/) / gorroto | Distance falloff replaces curve texture; preserves alpha |

The original posts designate their shader code CC0. See [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). Attribution and original URLs are retained in each shader file. The third node intentionally differs from the original curve-driven effect.

## Multiple sources

Import media into the first Source Clip to establish the working resolution and timeline. Add **+ Node > + Source Clip**, click that new node, choose IMAGE or VIDEO in the toolbar, and import its media from Controls. Each source has its own decoder, texture, and thumbnail. Additional sources stretch to the working resolution, run from time zero on the shared timeline, and hold their final frame when shorter than the primary clip.

Add **Blend**, connect one source/branch to A and another to B. Normal provides alpha compositing; the other 11 modes provide blend operations. Use opacity to adjust the layer. Additional source nodes can be removed from their controls. The first source is the canvas/timeline owner and remains required. MP4 duration follows that source video; no per-source offsets, independent timelines or audio mixing are provided yet.

Project format v3 stores each additional source path and shader code. v1/v2 files remain readable; older Flexar versions cannot read v3 projects. Source media is referenced by path rather than embedded.

## Artistic shader nodes

The regular + Node menu includes Liquid Chrome, Acid Print, Neon Melt, and Glitch Collage. These are original Flexar effects authored as Godot-style canvas_item fragment shaders, executed through the existing supported-subset HLSL translator. They expose visual sliders without a code editor. Code editing remains available on Custom Shader.

- Liquid Chrome: iridescent relief, flowing distortion, metallic bands and highlights.
- Acid Print: stepped acid colors, misregistered ink, halftone dots and grit.
- Neon Melt: curved luminous extrusion with animated flow and palette controls.
- Glitch Collage: animated strip displacement, RGB splitting, negative bands and posterization.

Each has a Mix slider. Start with one artistic node directly after a source, then adjust its controls. Play animates effects with Speed controls; Speed zero freezes their procedural motion. These additions do not embed Godot's renderer or extend the supported language subset.
