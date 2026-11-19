// Original Flexar optical canvas_item effect.
shader_type canvas_item;
uniform float Threshold : hint_range(0, 1) = 0.6;
uniform float Radius : hint_range(1, 60) = 16;
uniform float Intensity : hint_range(0, 5) = 1.5;
uniform float Pulse : hint_range(0, 1) = 0.25;
uniform float Speed : hint_range(0, 2) = 0.4;
uniform float Warmth : hint_range(0, 1) = 0.35;
void fragment(){vec4 src=texture(TEXTURE,UV);vec3 glow=vec3(0.0);float weights=0.0;
for(int y=-3;y<=3;y++){for(int x=-3;x<=3;x++){float weight=exp(-float(x*x+y*y)*0.3);vec3 v=texture(TEXTURE,UV+vec2(float(x),float(y))*TEXTURE_PIXEL_SIZE*Radius/3.0).rgb;glow+=v*smoothstep(Threshold,1.0,dot(v,vec3(0.299,0.587,0.114)))*weight;weights+=weight;}}
float pulse=1.0+Pulse*sin(TIME*Speed*6.283185);vec3 tint=mix(vec3(1.0),vec3(1.0,0.55,0.3),Warmth);COLOR=vec4(src.rgb+glow/weights*Intensity*pulse*tint,src.a);}
