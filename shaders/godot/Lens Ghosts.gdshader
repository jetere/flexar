// Original Flexar optical canvas_item effect.
shader_type canvas_item;
uniform float Threshold : hint_range(0, 1) = 0.65;
uniform float Intensity : hint_range(0, 4) = 1.2;
uniform float Spacing : hint_range(0, 1.5) = 0.65;
uniform float Dispersion : hint_range(0, 0.08) = 0.018;
uniform float Halo : hint_range(0, 2) = 0.5;
void fragment(){vec4 src=texture(TEXTURE,UV);vec2 axis=vec2(0.5)-UV;vec3 glow=vec3(0.0);
for(int i=1;i<=7;i++){float f=float(i)/7.0;vec2 q=UV+axis*(1.0+f*Spacing*3.0);vec3 v=vec3(texture(TEXTURE,q+axis*Dispersion).r,texture(TEXTURE,q).g,texture(TEXTURE,q-axis*Dispersion).b);float edge=step(0.0,q.x)*step(q.x,1.0)*step(0.0,q.y)*step(q.y,1.0);float light=smoothstep(Threshold,1.0,dot(v,vec3(0.299,0.587,0.114)));vec3 tint=0.5+0.5*cos(vec3(0.0,2.1,4.2)+f*6.0);glow+=v*tint*light*edge*(1.0-f*0.7);}
float ring=exp(-abs(length(axis)-0.32)*55.0);vec3 center=texture(TEXTURE,vec2(0.5)).rgb;COLOR=vec4(src.rgb+glow*Intensity+ring*center*Halo,src.a);}
