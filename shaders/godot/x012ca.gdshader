// Adapted from gorroto, CC0: https://godotshaders.com/shader/radial-chromatic-aberration/
// Procedural distance falloff replaces the external curve texture.
shader_type canvas_item;
uniform float intensity : hint_range(0, 200) = 40;
void fragment() {
    vec2 centered_uv = UV - vec2(0.5, 0.5);
    float curve_distance_intensity = length(centered_uv);
    vec4 base = texture(TEXTURE, UV);
    float red_channel = texture(TEXTURE, UV - centered_uv * curve_distance_intensity * SCREEN_PIXEL_SIZE * intensity).r;
    float green_channel = texture(TEXTURE, UV - centered_uv * curve_distance_intensity * SCREEN_PIXEL_SIZE * intensity / 2.0).g;
    COLOR = vec4(red_channel, green_channel, base.b, base.a);
}
