// Adapted from godotshaders, CC0: https://godotshaders.com/shader/pixelate/
shader_type canvas_item;
uniform int amount : hint_range(1, 256) = 40;
void fragment() {
    vec2 grid_uv = round(UV * float(amount)) / float(amount);
    COLOR = texture(TEXTURE, grid_uv);
}
