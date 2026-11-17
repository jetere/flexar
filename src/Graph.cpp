#include "Graph.h"
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace flexar {
const std::vector<EffectInfo>& catalog() {
    static const std::vector<EffectInfo> entries{
        {Effect::Grade,"Brightness / Contrast","Exposure, contrast and saturation"},
        {Effect::Blur,"Gaussian Blur","Two-pass separable Gaussian blur"},
        {Effect::Halftone,"Print Halftone","Rotated ink dots"},
        {Effect::Xerox,"Distressed Xerox","Thresholded ink with eroded, noisy edges"},
        {Effect::Duotone,"Duotone","Violet ink, warm sepia or thermal palette"},
        {Effect::Grain,"Film Grain","Seeded animated luminance grain"},
        {Effect::Liquid,"Liquid Warp","Flowing, time-varying distortion"},
        {Effect::Neon,"Neon Trails","Directional luminous smears"},
        {Effect::Paper,"Crumpled Paper","Procedural creases, fibers and paper tint"},
        {Effect::Vignette,"Vignette","Soft edge falloff"},
        {Effect::Outline,"Ink Outline","Luminance edges with optional source blend"},
        {Effect::ColorMatrix,"Color Matrix","3 x 3 RGB transformation"},
        {Effect::Blend,"Blend","Combine two graph branches: A base / B layer"},
        {Effect::Chromatic,"Chromatic Split","Offset red and blue channels"},
        {Effect::Posterize,"Posterize","Quantized print-like color bands"},
        {Effect::Pixelate,"Pixelate","Square sampled pixel blocks"},
        {Effect::Solarize,"Solarize","Invert highlights beyond a threshold"},
        {Effect::Scanlines,"CRT Scanlines","Analog lines and animated signal interference"},
        {Effect::Mirror,"Mirror Fold","Reflect and repeat the image"},
        {Effect::Sharpen,"Sharpen","Adjustable unsharp detail enhancement"},
        {Effect::Bloom,"Bloom","Soft highlight glow"},
        {Effect::Threshold,"Ink Threshold","Two-tone silhouettes with soft edges"},
        {Effect::GlowRays,"Light Rays","Radial highlight beams from a movable center"},
        {Effect::Streaks,"Star Streaks","Angled highlight streaks with adjustable spread"},
        {Effect::Shake,"Camera Shake","Animated smooth handheld displacement and rotation"},
        {Effect::LensWarp,"Lens Distortion","Barrel or pincushion lens deformation"},
        {Effect::Prism,"Prism Dispersion","Radial RGB refraction around the optical center"},
        {Effect::Flicker,"Film Flicker","Seeded exposure flutter and color instability"}
    }; return entries;
}
Id Graph::add(Effect effect, float x, float y) {
    Node n; n.id=next_++; n.effect=effect; n.x=x; n.y=y;
    n.name=effect==Effect::Source?"Source Clip":effect==Effect::Output?"Output":"Effect";
    for (auto& e:catalog()) if(e.effect==effect) n.name=e.name;
    if(effect!=Effect::Source) n.inputs.push_back({next_++,n.id,Direction::Input});
    if(effect==Effect::Blend) n.inputs.push_back({next_++,n.id,Direction::Input});
    if(effect!=Effect::Output) n.outputs.push_back({next_++,n.id,Direction::Output});
    switch(effect) {
    case Effect::Grade: n.parameters={{"Exposure",0,-3,3},{"Contrast",1,0,3},{"Saturation",1,0,2}}; break;
    case Effect::Blur: n.parameters={{"Radius",4,0,24}}; break;
    case Effect::Halftone: n.parameters={{"Dot size",5,2,20},{"Angle",45,0,180},{"Ink contrast",1.2f,.2f,3}}; break;
    case Effect::Xerox: n.parameters={{"Threshold",.48f,0,1},{"Distress",.25f,0,1},{"Invert",0,0,1}}; break;
    case Effect::Duotone: n.parameters={{"Palette",0,0,2},{"Mix",1,0,1}}; break;
    case Effect::Grain: n.parameters={{"Amount",.12f,0,.5f},{"Size",1,1,8},{"Motion",1,0,1}}; break;
    case Effect::Liquid: n.parameters={{"Amount",.035f,0,.2f},{"Frequency",12,1,40},{"Speed",.5f,0,3}}; break;
    case Effect::Neon: n.parameters={{"Length",.12f,0,.4f},{"Glow",1.3f,0,4},{"Curve",.05f,0,.3f}}; break;
    case Effect::Paper: n.parameters={{"Creases",.22f,0,1},{"Scale",12,2,50},{"Tint",.3f,0,1}}; break;
    case Effect::Vignette: n.parameters={{"Amount",.5f,0,1},{"Softness",.5f,.05f,1}}; break;
    case Effect::Outline: n.parameters={{"Width",1,1,6},{"Intensity",2,0,8},{"Source mix",0,0,1}}; break;
    case Effect::ColorMatrix: n.parameters={{"R from R",1,-2,2},{"R from G",0,-2,2},{"R from B",0,-2,2},{"G from R",0,-2,2},{"G from G",1,-2,2},{"G from B",0,-2,2},{"B from R",0,-2,2},{"B from G",0,-2,2},{"B from B",1,-2,2}}; break;
    case Effect::Blend: n.parameters={{"Mode",2,0,11},{"Opacity",.5f,0,1}};break;
    case Effect::Chromatic: n.parameters={{"Offset",5,0,40},{"Angle",0,0,360}};break;
    case Effect::Posterize: n.parameters={{"Levels",5,2,32}};break;
    case Effect::Pixelate: n.parameters={{"Block size",8,1,96}};break;
    case Effect::Solarize: n.parameters={{"Threshold",.5f,0,1},{"Mix",1,0,1}};break;
    case Effect::Scanlines: n.parameters={{"Spacing",4,2,20},{"Strength",.35f,0,1},{"Interference",.08f,0,.5f}};break;
    case Effect::Mirror: n.parameters={{"Axis",0,0,2},{"Repeats",1,1,8}};break;
    case Effect::Sharpen: n.parameters={{"Amount",1,0,4},{"Radius",1,1,6}};break;
    case Effect::Bloom: n.parameters={{"Threshold",.65f,0,1},{"Radius",6,1,24},{"Intensity",.8f,0,3}};break;
    case Effect::Threshold: n.parameters={{"Threshold",.5f,0,1},{"Softness",.02f,.001f,.3f},{"Invert",0,0,1}};break;
    case Effect::GlowRays:n.parameters={{"Intensity",.7f,0,3},{"Length",.6f,0,1},{"Threshold",.65f,0,1},{"Center X",.5f,0,1},{"Center Y",.5f,0,1}};break;
    case Effect::Streaks:n.parameters={{"Intensity",.8f,0,3},{"Length",.08f,0,.4f},{"Angle",0,0,180},{"Threshold",.65f,0,1}};break;
    case Effect::Shake:n.parameters={{"Amount",.015f,0,.12f},{"Speed",4,0,20},{"Rotation",1,0,10},{"Seed",1,0,100}};break;
    case Effect::LensWarp:n.parameters={{"Distortion",.3f,-1,1},{"Zoom",1,.5f,2},{"Center X",.5f,0,1},{"Center Y",.5f,0,1}};break;
    case Effect::Prism:n.parameters={{"Spread",.025f,0,.15f},{"Zoom",1,.5f,2},{"Center X",.5f,0,1},{"Center Y",.5f,0,1}};break;
    case Effect::Flicker:n.parameters={{"Amount",.25f,0,1},{"Speed",8,0,30},{"Color drift",.1f,0,1},{"Seed",1,0,100}};break;
    default: break;
    }
    nodes.push_back(n); return n.id;
}
Node* Graph::find(Id id) { for(auto& n:nodes) if(n.id==id)return &n; return nullptr; }
const Node* Graph::find(Id id) const { for(auto& n:nodes) if(n.id==id)return &n; return nullptr; }
const Pin* Graph::pin(Id id) const { for(auto& n:nodes) {for(auto& p:n.inputs)if(p.id==id)return &p;for(auto& p:n.outputs)if(p.id==id)return &p;}return nullptr; }
void Graph::disconnect(Id input) { std::erase_if(connections,[&](auto& c){return c.to==input;}); }
void Graph::remove(Id id) {
    std::erase_if(connections,[&](auto& c){auto a=pin(c.from),b=pin(c.to); return !a||!b||a->node==id||b->node==id;});
    std::erase_if(nodes,[&](auto& n){return n.id==id;});
}
std::vector<Id> Graph::sorted() const {
    std::unordered_map<Id,int> state; std::vector<Id> result;
    std::function<void(Id)> visit=[&](Id id) {
        if(state[id]==1)throw std::runtime_error("Connection would create a cycle");
        if(state[id]==2)return;
        if(!find(id))throw std::runtime_error("Missing node");
        state[id]=1;
        for(auto& c:connections) {
            auto a=pin(c.from),b=pin(c.to);
            if(!a||!b)throw std::runtime_error("Dangling pin");
            if(b->node==id)visit(a->node);
        }
        state[id]=2;result.push_back(id);
    };
    for(auto& n:nodes)visit(n.id); return result;
}
bool Graph::connect(Id from,Id to,std::string& error) {
    const auto a=pin(from), b=pin(to);
    if(!a||!b||a->direction!=Direction::Output||b->direction!=Direction::Input||a->type!=b->type||a->node==b->node) {error="Connect an output to a compatible input on another node";return false;}
    auto old=connections; disconnect(to);connections.push_back({from,to});
    try {sorted();}catch(const std::exception& e){connections=old;error=e.what();return false;}
    error.clear();return true;
}
std::vector<Id> Graph::execution(Id output) const {
    std::unordered_set<Id> needed;
    std::function<void(Id)> collect=[&](Id id){if(!needed.insert(id).second)return;for(auto& c:connections)if(pin(c.to)->node==id)collect(pin(c.from)->node);};
    auto order=sorted(); if(!find(output))throw std::runtime_error("Output node missing"); collect(output);
    std::erase_if(order,[&](Id id){return !needed.contains(id);});return order;
}
void Graph::preset(int index) {
    nodes.clear();connections.clear();
    std::vector<Effect> effects;
    switch(index) {
    case 0: effects={Effect::Grade};break;
    case 1: effects={Effect::Xerox,Effect::Duotone,Effect::Grain};break;
    case 2: effects={Effect::Grade,Effect::Halftone,Effect::Xerox};break;
    case 3: effects={Effect::Liquid,Effect::Outline};break;
    case 4: effects={Effect::Grade,Effect::Duotone,Effect::Grain,Effect::Vignette};break;
    case 5: effects={Effect::Liquid,Effect::Neon,Effect::Grain};break;
    case 6: effects={Effect::Grade,Effect::Paper,Effect::Grain};break;
    case 7: effects={Effect::Chromatic,Effect::Scanlines,Effect::Vignette};break;
    case 8: effects={Effect::Posterize,Effect::Bloom,Effect::Grain};break;
    case 9: effects={Effect::Solarize,Effect::Duotone,Effect::Sharpen};break;
    default: throw std::runtime_error("Unknown preset");
    }
    Id previous=add(Effect::Source,30,80); int i=1; std::string error;
    effects.push_back(Effect::Output);
    for(auto e:effects) {Id current=add(e,30.f+310.f*i++,80);connect(find(previous)->outputs[0].id,find(current)->inputs[0].id,error);previous=current;}
    if(index==4)for(auto& n:nodes)if(n.effect==Effect::Duotone)n.parameters[0].value=1;
    if(index==2)for(auto& n:nodes)if(n.effect==Effect::Xerox)n.parameters[1].value=.08f;
}
}
