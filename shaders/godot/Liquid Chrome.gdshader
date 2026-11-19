// Original Flexar canvas_item shader: iridescent liquid relief.
shader_type canvas_item;
uniform float Relief : hint_range(0, 6) = 2.8;
uniform float Flow : hint_range(0, 0.15) = 0.035;
uniform float Frequency : hint_range(1, 40) = 12;
uniform float Speed : hint_range(0, 3) = 0.3;
uniform float Iridescence : hint_range(0, 2) = 0.8;
uniform float Contrast : hint_range(0.5, 4) = 1.8;
uniform float Mix : hint_range(0, 1) = 1;
void fragment(){
 vec4 original = texture(TEXTURE, UV);
 float t = TIME * Speed;
 vec2 q = UV + Flow * vec2(sin(UV.y*Frequency+t+cos(UV.x*9.0)),cos(UV.x*Frequency-t+sin(UV.y*11.0)));
 vec4 src = texture(TEXTURE,q);
 vec2 d = TEXTURE_PIXEL_SIZE * 2.0;
 float gx=dot(texture(TEXTURE,q+vec2(d.x,0.0)).rgb-texture(TEXTURE,q-vec2(d.x,0.0)).rgb,vec3(0.299,0.587,0.114));
 float gy=dot(texture(TEXTURE,q+vec2(0.0,d.y)).rgb-texture(TEXTURE,q-vec2(0.0,d.y)).rgb,vec3(0.299,0.587,0.114));
 float l=dot(src.rgb,vec3(0.299,0.587,0.114));
 float field=l*Relief+gx*8.0-gy*6.0+sin(q.y*5.0+t)*0.2;
 float metal=pow(0.5+0.5*sin(field*9.0),Contrast);
 vec3 rainbow=0.5+0.5*cos(vec3(0.0,2.1,4.2)+field*5.0+t);
 vec3 finish=mix(vec3(metal),rainbow*metal,Iridescence)+pow(metal,8.0)*0.6;
 COLOR=vec4(mix(original.rgb,finish,Mix),original.a);
}
