Texture2D<float4> Source : register(t0);
Texture2D<float4> Layer : register(t1);
RWTexture2D<float4> Target : register(u0);
SamplerState LinearClamp : register(s0);
cbuffer Controls : register(b0) {
    uint effect; uint width; uint height; float time;
    float4 p0; float4 p1; float4 p2; float4 p3;
};
float lum(float3 c) { return dot(c,float3(.2126,.7152,.0722)); }
float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float noise(float2 p) {
    float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
    return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
}
float3 sampleColor(float2 uv) {return Source.SampleLevel(LinearClamp,uv,0).rgb;}
float3 thermal(float v) {
    return v<.5 ? lerp(float3(.008,.004,.04),float3(.65,.035,.9),v*2) : lerp(float3(.65,.035,.9),float3(1,.64,.015),(v-.5)*2);
}
float3 blendColor(float3 a,float3 b,int mode) {
    if(mode==1)return a*b;
    if(mode==2)return 1-(1-a)*(1-b);
    if(mode==3)return lerp(2*a*b,1-2*(1-a)*(1-b),step(.5,a));
    if(mode==4){float3 d=lerp(((16*a-12)*a+4)*a,sqrt(a),step(.25,a));return lerp(a-(1-2*b)*a*(1-a),a+(2*b-1)*(d-a),step(.5,b));}
    if(mode==5)return lerp(2*a*b,1-2*(1-a)*(1-b),step(.5,b));
    if(mode==6)return abs(a-b);
    if(mode==7)return a+b-2*a*b;
    if(mode==8)return saturate(a+b);
    if(mode==9)return saturate(a-b);
    if(mode==10)return min(a,b);
    if(mode==11)return max(a,b);
    return b;
}
[numthreads(8,8,1)]
void main(uint3 tid:SV_DispatchThreadID) {
    if(tid.x>=width||tid.y>=height)return;
    float2 pixel=float2(tid.xy),size=float2(width,height),uv=(pixel+.5)/size;
    float4 source=Source.Load(int3(tid.xy,0));float3 c=source.rgb;
    if(effect==2) { c=(c*exp2(p0.x)-.5)*p0.y+.5;c=lerp(lum(c).xxx,c,p0.z); }
    else if(effect==3) {
        float radius=p0.x;float sigma=max(.5,radius*.45);float3 sum=0;float total=0;
        int r=(int)ceil(radius);float2 axis=p3.x==0?float2(1,0):float2(0,1);
        for(int k=-r;k<=r;++k){float w=exp(-.5*k*k/(sigma*sigma));sum+=sampleColor(uv+axis*k/size)*w;total+=w;}
        c=sum/max(total,.00001);
    }
    else if(effect==4) {
        float angle=radians(p0.y);float2x2 rot=float2x2(cos(angle),-sin(angle),sin(angle),cos(angle));
        float2 cell=mul(rot,pixel)/p0.x;
        float2 center=mul(transpose(rot),(floor(cell)+.5)*p0.x)/size;
        float ink=saturate((1-lum(sampleColor(center)))*p0.z);
        float distance=length(frac(cell)-.5);float radius=sqrt(ink)*.7071;
        c=smoothstep(radius-.6/p0.x,radius+.6/p0.x,distance).xxx;
    }
    else if(effect==5) {
        float grit=hash(pixel)-.5;float threshold=p0.x+grit*p0.y;
        float v=step(threshold,lum(c));if(p0.z>.5)v=1-v;c=v.xxx;
    }
    else if(effect==6) {
        float v=saturate(lum(c));float3 mapped;
        if(p0.x<.5)mapped=lerp(float3(.035,.005,.075),float3(.94,.81,1),v);
        else if(p0.x<1.5)mapped=lerp(float3(.045,.055,.065),float3(.85,.70,.44),v);
        else mapped=thermal(v);
        c=lerp(c,mapped,p0.y);
    }
    else if(effect==7) {float frame=floor(time*24)*step(.5,p0.z);float n=hash(floor(pixel/max(1,p0.y))+frame)-.5;c+=n*p0.x;}
    else if(effect==8) {
        float t=time*p0.z;float2 flow=float2(sin(uv.y*p0.y+t)+.4*sin(uv.y*p0.y*2.3-t),cos(uv.x*p0.y*.7-t));
        c=sampleColor(uv+flow*p0.x);
    }
    else if(effect==9) {
        float energy=0;
        [unroll]for(int k=0;k<32;++k) {
            float s=k/31.;float2 offset=float2(sin(uv.y*9-s*6+time*.25)*p0.z*s,-s*p0.x);
            energy=max(energy,lum(sampleColor(uv+offset))*(1-s));
        }
        c=max(c,thermal(saturate(energy))*energy*p0.y);
    }
    else if(effect==10) {
        float2 q=uv*p0.y;float fold=abs(noise(q)*2-1);
        float detail=noise(q*4)*.25+noise(q*13)*.06;
        float shade=(fold+detail-.5)*p0.x;
        c=c*(1+shade)+shade*.25;
        c*=lerp(float3(1,1,1),float3(1,.84,.82),p0.z);
    }
    else if(effect==11) {
        float d=length((uv-.5)*1.4142);float v=smoothstep(1-p0.y,1,d);c*=1-v*p0.x;
    }
    else if(effect==12) {
        float2 d=p0.x/size;
        float gx=lum(sampleColor(uv+float2(d.x,0)))-lum(sampleColor(uv-float2(d.x,0)));
        float gy=lum(sampleColor(uv+float2(0,d.y)))-lum(sampleColor(uv-float2(0,d.y)));
        float e=saturate(length(float2(gx,gy))*p0.y);c=lerp(e.xxx,c,p0.z);
    }
    else if(effect==13) {c=float3(dot(c,p0.xyz),dot(c,float3(p0.w,p1.xy)),dot(c,float3(p1.zw,p2.x)));}
    else if(effect==14) {
        float4 b=Layer.SampleLevel(LinearClamp,uv,0);float ab=source.a,as=b.a*p0.y;
        float alpha=as+ab*(1-as);
        float3 rgb=((1-as)*ab*c+(1-ab)*as*b.rgb+ab*as*blendColor(c,b.rgb,(int)round(p0.x)))/max(alpha,.00001);
        Target[tid.xy]=float4(saturate(rgb),alpha);return;
    }
    else if(effect==15){float a=radians(p0.y);float2 d=float2(cos(a),sin(a))*p0.x/size;c=float3(sampleColor(uv+d).r,c.g,sampleColor(uv-d).b);}
    else if(effect==16){float levels=max(2,round(p0.x));c=round(c*(levels-1))/(levels-1);}
    else if(effect==17){float block=max(1,round(p0.x));c=sampleColor((floor(pixel/block)*block+block*.5)/size);}
    else if(effect==18){c=lerp(c,lerp(c,1-c,step(p0.x,c)),p0.y);}
    else if(effect==19){float scan=.5+.5*cos(pixel.y*6.283185/max(2,p0.x));c*=1-scan*p0.y;c+=(hash(float2(floor(pixel.y),floor(time*24)))-.5)*p0.z;}
    else if(effect==20){float2 q=uv;if(p0.x<.5||p0.x>1.5)q.x=abs(frac(uv.x*round(p0.y))*2-1);if(p0.x>.5)q.y=abs(frac(uv.y*round(p0.y))*2-1);c=sampleColor(q);}
    else if(effect==21){float2 d=p0.y/size;float3 avg=(sampleColor(uv+float2(d.x,0))+sampleColor(uv-float2(d.x,0))+sampleColor(uv+float2(0,d.y))+sampleColor(uv-float2(0,d.y)))*.25;c+=p0.x*(c-avg);}
    else if(effect==22){float3 glow=0;float weights=0;[unroll]for(int y=-2;y<=2;++y){[unroll]for(int x=-2;x<=2;++x){float w=exp(-.5*(x*x+y*y));float3 s=sampleColor(uv+float2(x,y)*p0.y/size);glow+=s*saturate((lum(s)-p0.x)/.15)*w;weights+=w;}}c+=glow/max(weights,.0001)*p0.z;}
    else if(effect==23){float ink=smoothstep(p0.x-p0.y,p0.x+p0.y,lum(c));c=lerp(ink.xxx,1-ink.xxx,step(.5,p0.z));}
    else if(effect==24){float3 rays=0;float2 center=float2(p0.w,p1.x);[unroll]for(int k=1;k<=24;++k){float s=k/24.;float3 v=sampleColor(lerp(uv,center,s*p0.y));rays+=v*saturate((lum(v)-p0.z)/max(.01,1-p0.z))*(1-s);}c+=rays*(p0.x/12);}
    else if(effect==25){float angle=radians(p0.z);float2 axis=float2(cos(angle),sin(angle));axis.x*=size.y/size.x;float3 streak=0;float weights=0;[unroll]for(int k=-16;k<=16;++k){float s=k/16.;float weight=1-abs(s);float3 v=sampleColor(uv+axis*s*p0.y);streak+=v*saturate((lum(v)-p0.w)/max(.01,1-p0.w))*weight;weights+=weight;}c+=streak/max(weights,.001)*p0.x;}
    else if(effect==26){float t=time*p0.y;float2 shift=float2(noise(float2(t,p0.w)),noise(float2(t+59,p0.w+13)))*2-1;float angle=(noise(float2(t+27,p0.w+31))*2-1)*radians(p0.z);float2 q=(uv-.5)*float2(size.x/size.y,1);float2 r=float2(q.x*cos(angle)-q.y*sin(angle),q.x*sin(angle)+q.y*cos(angle));c=sampleColor(r/float2(size.x/size.y,1)+.5+shift*p0.x);}
    else if(effect==27){float2 center=p0.zw;float2 q=(uv-center)/p0.y;float aspect=size.x/size.y;float r2=dot(q*float2(aspect,1),q*float2(aspect,1));c=sampleColor(center+q*(1+p0.x*r2));}
    else if(effect==28){float2 center=p0.zw;float2 q=(uv-center)/p0.y;float distance=length(q);float2 d=q*distance*p0.x;c=float3(sampleColor(center+q+d).r,sampleColor(center+q).g,sampleColor(center+q-d).b);}
    else if(effect==29){float t=floor(time*p0.y);float exposure=(hash(float2(t,p0.w))*2-1)*p0.x;float drift=(hash(float2(t+17,p0.w))*2-1)*p0.z;c*=exp2(exposure)*float3(1+drift,1,1-drift);}
    else if(effect==31){
        // Parameters: position.xy, scale.xy, rotation, opacity, mode, first layer,
        // flips.xy, crop left/top/right/bottom. Coordinates are centered pixels.
        float2 q=(uv-.5)*size-p0.xy*size;
        float angle=radians(p1.x),cs=cos(angle),sn=sin(angle);
        q=float2(cs*q.x+sn*q.y,-sn*q.x+cs*q.y)/max(p0.zw,.001)/size+.5;
        if(p2.x>.5)q.x=1-q.x;if(p2.y>.5)q.y=1-q.y;
        bool inside=all(q>=float2(p2.z,p2.w))&&all(q<=1-float2(p3.x,p3.y));
        float4 top=inside?Layer.SampleLevel(LinearClamp,q,0):float4(0,0,0,0);
        float4 bottom=p1.w>.5?float4(0,0,0,0):source;
        float ab=bottom.a,at=top.a*p1.y,alpha=at+ab*(1-at);
        float3 rgb=((1-at)*ab*bottom.rgb+(1-ab)*at*top.rgb+ab*at*blendColor(bottom.rgb,top.rgb,(int)p1.z))/max(alpha,.00001);
        Target[tid.xy]=float4(saturate(rgb),alpha);return;
    }
    else if(effect==32){float divisions=max(1,round(p0.x));Target[tid.xy]=Source.SampleLevel(LinearClamp,round(uv*divisions)/divisions,0);return;}
    else if(effect==33){float2 grid=max(round(p0.xy),1);float4 v=Source.SampleLevel(LinearClamp,floor(uv*grid)/grid,0);v.rgb=floor(v.rgb/max(.01,p0.z))*max(.01,p0.z);Target[tid.xy]=v;return;}
    else if(effect==34){float2 q=uv-.5,d=q*length(q)/size*p0.x;c=float3(sampleColor(uv-d).r,sampleColor(uv-d*.5).g,c.b);}
    else if(effect==35){c=c*p0.xyz+float3(p0.w,p1.xy);}
    else if(effect==36){float2 d=p0.x/size;float gx=lum(sampleColor(uv+float2(d.x,0)))-lum(sampleColor(uv-float2(d.x,0)));float gy=lum(sampleColor(uv+float2(0,d.y)))-lum(sampleColor(uv-float2(0,d.y)));float edge=max(0,length(float2(gx,gy))-p0.z)*p0.y;c=c*p0.w+edge*p1.xyz;}
    else if(effect==37){float4 total=0;[unroll]for(int k=0;k<24;++k)total+=Source.SampleLevel(LinearClamp,lerp(uv,p0.yz,(k/23.)*p0.x),0);Target[tid.xy]=lerp(source,total/24,p0.w);return;}
    else if(effect==38){float2 q=(uv-p1.xy)*float2(size.x/size.y,1);float r=length(q);float2 offset=q/max(r,.0001)*sin(r*p0.y-time*p0.z)*p0.x*exp(-r*p0.w);offset.x*=size.y/size.x;float2 target=uv+offset;Target[tid.xy]=Source.SampleLevel(LinearClamp,target,0);return;}
    else if(effect==39){float a=smoothstep(p0.x-p0.y,p0.x+p0.y,lum(c));if(p0.z>.5)a=1-a;Target[tid.xy]=float4(c,source.a*a);return;}
    else if(effect==40){float shadow=0;[unroll]for(int y=-2;y<=2;++y)[unroll]for(int x=-2;x<=2;++x){float2 q=uv-p0.xy/size+float2(x,y)*p0.z/size;shadow+=(all(q>=0)&&all(q<=1))?Source.SampleLevel(LinearClamp,q,0).a:0;}shadow=shadow/25*p0.w;float a=source.a+shadow*(1-source.a);float3 rgb=(c*source.a+p1.xyz*shadow*(1-source.a))/max(a,.00001);Target[tid.xy]=float4(rgb,a);return;}
    else if(effect==41){float mask=Layer.SampleLevel(LinearClamp,uv,0).r;Target[tid.xy]=float4(c,source.a*mask);return;}

    Target[tid.xy]=float4(saturate(c),source.a);
}
