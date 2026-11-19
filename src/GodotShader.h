#pragma once
#include "Graph.h"
#include <regex>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace flexar {
// Deliberately limited canvas_item compatibility; unsupported constructs fail compilation.
inline std::string translateShader(Node& node) {
    auto code=std::regex_replace(node.shaderCode,std::regex(R"(/\*[\s\S]*?\*/|//[^\n]*)"),"");
    if(code.size()>65536||code.find('#')!=std::string::npos)throw std::runtime_error("Shader exceeds 64KB or uses unsupported preprocessor directives");
    if(!std::regex_search(code,std::regex(R"(shader_type\s+canvas_item\s*;)")))throw std::runtime_error("Expected shader_type canvas_item;");
    code=std::regex_replace(code,std::regex(R"(shader_type\s+canvas_item\s*;)"),"");
    std::regex uniform(R"(uniform\s+(float|int|bool)\s+(\w+)\s*(?::\s*hint_range\s*\(\s*([-+.0-9]+)\s*,\s*([-+.0-9]+)(?:\s*,\s*[-+.0-9]+)?\s*\))?\s*=\s*([-+.0-9]+|true|false)\s*;)");
    std::smatch m;std::string declarations;std::vector<Parameter> parameters;
    while(std::regex_search(code,m,uniform)){
        if(parameters.size()>=12)throw std::runtime_error("At most 12 scalar uniforms are supported");
        float value=m[5]=="true"?1:m[5]=="false"?0:std::stof(m[5]);
        float lo=m[3].matched?std::stof(m[3]):-1000,hi=m[4].matched?std::stof(m[4]):1000;
        if(!std::isfinite(value)||!std::isfinite(lo)||!std::isfinite(hi)||lo>hi)throw std::runtime_error("Invalid uniform range");
        auto i=parameters.size();parameters.push_back({m[2],std::clamp(value,lo,hi),lo,hi});
        declarations+=m[1].str()+" "+m[2].str()+" = ("+m[1].str()+")params["+std::to_string(i/4)+"]["+std::to_string(i%4)+"];\n";
        code.replace(m.position(),m.length(),"");
    }
    if(code.find("uniform")!=std::string::npos||code.find("vertex")!=std::string::npos||code.find("varying")!=std::string::npos)throw std::runtime_error("Only fragment effects with scalar uniforms are supported");
    std::regex fragment(R"(^\s*void\s+fragment\s*\(\s*\)\s*\{([\s\S]*)\}\s*$)");
    if(!std::regex_match(code,m,fragment))throw std::runtime_error("Expected one void fragment() function");
    code=m[1];
    for(auto pair: {std::pair{"vec2","float2"},{"vec3","float3"},{"vec4","float4"},{"mix","lerp"},{"fract","frac"},{"mod","godot_mod"}})code=std::regex_replace(code,std::regex(std::string("\\b")+pair.first+"\\b"),pair.second);
    code=std::regex_replace(code,std::regex(R"(texture\s*\(\s*TEXTURE\s*,)"),"sampleInput(");
    // GLSL scalar splats need explicit HLSL expansion.
    for(int n=2;n<=4;++n){std::string type="float"+std::to_string(n);std::string replacement=type+"($1";for(int i=1;i<n;++i)replacement+=",$1";replacement+=")";code=std::regex_replace(code,std::regex(type+R"(\(\s*([\w.]+)\s*\))"),replacement);}
    node.parameters=std::move(parameters);
    return "Texture2D<float4> Source:register(t0); RWTexture2D<float4> Target:register(u0); SamplerState LinearClamp:register(s0); cbuffer C:register(b0){uint effect; uint width; uint height; float TIME; float4 params[4];};\nfloat4 sampleInput(float2 uv){return Source.SampleLevel(LinearClamp,uv,0);}\nfloat godot_mod(float a,float b){return a-b*floor(a/b);}\nfloat2 godot_mod(float2 a,float2 b){return a-b*floor(a/b);}\nfloat3 godot_mod(float3 a,float3 b){return a-b*floor(a/b);}\n[numthreads(8,8,1)] void main(uint3 tid:SV_DispatchThreadID){if(tid.x>=width||tid.y>=height)return;float2 UV=(float2(tid.xy)+0.5)/float2(width,height);float2 TEXTURE_PIXEL_SIZE=1.0/float2(width,height);float2 SCREEN_PIXEL_SIZE=TEXTURE_PIXEL_SIZE;float4 COLOR=sampleInput(UV);\n"+declarations+code+"\nTarget[tid.xy]=COLOR;}";
}
}
