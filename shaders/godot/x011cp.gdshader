// Adapted from SolarGranulation, CC0: https://godotshaders.com/shader/256-colour-pixelation/
// Scalar color step replaces vec3 uniform for Flexar compatibility.
shader_type canvas_item;
uniform int resX : hint_range(1, 512) = 64;
uniform int resY : hint_range(1, 512) = 64;
uniform float color_step : hint_range(0.01, 0.5) = 0.100392156862;
void fragment() {
    float uvX = UV.x - mod(UV.x * float(resX), 1.0) / float(resX);
    float uvY = UV.y - mod(UV.y * float(resY), 1.0) / float(resY);
    vec4 col = texture(TEXTURE, vec2(uvX, uvY));
    col.rgb = col.rgb - mod(col.rgb, vec3(color_step));
    COLOR = col;
}
