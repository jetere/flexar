// Original Flexar optical canvas_item effect.
shader_type canvas_item;
uniform float Cells : hint_range(2, 40) = 10;
uniform float Refraction : hint_range(0, 0.2) = 0.045;
uniform float Split : hint_range(0, 0.06) = 0.014;
uniform float Edge : hint_range(0, 2) = 0.4;
uniform float Speed : hint_range(0, 2) = 0.2;
uniform float Mix : hint_range(0, 1) = 1;
void fragment(){vec4 src=texture(TEXTURE,UV);vec2 cell=floor(UV*Cells);vec2 local=fract(UV*Cells)-vec2(0.5);float seed=fract(sin(dot(cell,vec2(127.1,311.7)))*43758.5453);float angle=seed*6.283185+TIME*Speed;vec2 offset=vec2(cos(angle),sin(angle))*Refraction;vec2 q=UV+offset;vec3 glass=vec3(texture(TEXTURE,q+offset*Split*20.0).r,texture(TEXTURE,q).g,texture(TEXTURE,q-offset*Split*20.0).b);float rim=pow(max(abs(local.x),abs(local.y))*2.0,18.0);glass+=rim*Edge*(0.5+0.5*cos(vec3(0.0,2.1,4.2)+seed*6.0));COLOR=vec4(mix(src.rgb,glass,Mix),src.a);}
