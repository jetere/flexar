// Original Flexar optical canvas_item effect.
shader_type canvas_item;
uniform float Threshold : hint_range(0, 1) = 0.7;
uniform float Length : hint_range(0, 0.3) = 0.08;
uniform float Intensity : hint_range(0, 5) = 1.5;
uniform float Rotation : hint_range(-180, 180) = 25;
uniform float Prism : hint_range(0, 1) = 0.65;
void fragment(){vec4 src=texture(TEXTURE,UV);vec3 flare=vec3(0.0);
for(int ray=0;ray<6;ray++){float angle=Rotation*0.01745329+float(ray)*1.04719755;vec2 axis=vec2(cos(angle),sin(angle));for(int i=1;i<=12;i++){float f=float(i)/12.0;vec3 v=texture(TEXTURE,UV+axis*f*Length).rgb;float l=smoothstep(Threshold,1.0,dot(v,vec3(0.299,0.587,0.114)));vec3 tint=mix(vec3(1.0),0.5+0.5*cos(vec3(0.0,2.1,4.2)+f*6.0),Prism);flare+=v*l*tint*(1.0-f);}}
COLOR=vec4(src.rgb+flare*Intensity/20.0,src.a);}
