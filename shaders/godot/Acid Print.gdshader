// Original Flexar canvas_item shader: acid ink, misregistered print and grit.
shader_type canvas_item;
uniform float Ink : hint_range(0, 1) = 0.48;
uniform float Bands : hint_range(2, 12) = 5;
uniform float Palette : hint_range(0, 6.28) = 0.5;
uniform float Misprint : hint_range(0, 24) = 5;
uniform float Grit : hint_range(0, 1) = 0.35;
uniform float DotSize : hint_range(2, 16) = 4;
uniform float Mix : hint_range(0, 1) = 1;
void fragment(){
 vec4 src=texture(TEXTURE,UV);
 vec2 px=UV/TEXTURE_PIXEL_SIZE;
 float noise=fract(sin(dot(floor(px),vec2(127.1,311.7)))*43758.5453);
 float l=dot(src.rgb,vec3(0.299,0.587,0.114));
 float shifted=dot(texture(TEXTURE,UV+vec2(Misprint,0.0)*TEXTURE_PIXEL_SIZE).rgb,vec3(0.299,0.587,0.114));
 float stepped=floor(l*Bands)/Bands;
 vec3 paint=0.5+0.5*cos(vec3(0.0,2.1,4.2)+stepped*7.0+Palette);
 vec2 grid=fract(vec2(px.x+px.y,px.y-px.x)/DotSize)-vec2(0.5);
 float dots=step(length(grid),sqrt(max(l,0.0))*0.65);
 float cut=step(Ink,l+(noise-0.5)*Grit);
 vec3 ink=mix(vec3(0.015,0.005,0.03),paint,cut)*mix(1.0,dots,Grit);
 ink+=abs(step(Ink,shifted)-cut)*vec3(0.95,0.12,0.65);
 COLOR=vec4(mix(src.rgb,ink,Mix),src.a);
}
