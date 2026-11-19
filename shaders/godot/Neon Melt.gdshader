// Original Flexar canvas_item shader: curved, luminous image extrusion.
shader_type canvas_item;
uniform float Length : hint_range(0, 0.5) = 0.18;
uniform float Curl : hint_range(0, 0.4) = 0.12;
uniform float Glow : hint_range(0, 4) = 1.8;
uniform float Threshold : hint_range(0, 1) = 0.35;
uniform float Palette : hint_range(0, 6.28) = 0.3;
uniform float Speed : hint_range(0, 3) = 0.25;
uniform float Mix : hint_range(0, 1) = 1;
void fragment(){
 vec4 src=texture(TEXTURE,UV);float energy=0.0;
 for(int i=0;i<40;i++){
 float s=float(i)/39.0;
 vec2 q=UV+vec2(sin(UV.y*7.0-s*5.0+TIME*Speed)*Curl*s,-s*Length);
 float l=dot(texture(TEXTURE,q).rgb,vec3(0.299,0.587,0.114));
 energy=max(energy,smoothstep(Threshold,1.0,l)*(1.0-s));
 }
 vec3 neon=0.5+0.5*cos(vec3(0.0,2.1,4.2)+energy*6.0+Palette);
 vec3 result=src.rgb*0.18+neon*energy*Glow+pow(energy,8.0)*0.6;
 COLOR=vec4(mix(src.rgb,result,Mix),src.a);
}
