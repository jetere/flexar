// Original Flexar canvas_item shader: torn bands, channel offsets and negative collage.
shader_type canvas_item;
uniform float Tear : hint_range(0, 0.5) = 0.15;
uniform float Strips : hint_range(2, 100) = 22;
uniform float Speed : hint_range(0, 20) = 6;
uniform float Split : hint_range(0, 40) = 8;
uniform float Invert : hint_range(0, 1) = 0.3;
uniform float Poster : hint_range(2, 16) = 5;
uniform float Mix : hint_range(0, 1) = 1;
void fragment(){
 vec4 src=texture(TEXTURE,UV);
 float band=floor(UV.y*Strips),tick=floor(TIME*Speed);
 float rnd=fract(sin(band*127.1+tick*311.7)*43758.5453);
 float shift=(rnd-0.5)*Tear*step(0.45,rnd);
 vec2 q=vec2(fract(UV.x+shift),UV.y);
 vec2 d=vec2(Split*TEXTURE_PIXEL_SIZE.x,0.0);
 vec3 color=vec3(texture(TEXTURE,q+d).r,texture(TEXTURE,q).g,texture(TEXTURE,q-d).b);
 color=floor(color*Poster)/max(Poster-1.0,1.0);
 color=mix(color,vec3(1.0)-color,step(1.0-Invert,rnd));
 color*=vec3(1.0,0.65+0.35*rnd,1.0);
 COLOR=vec4(mix(src.rgb,color,Mix),src.a);
}
