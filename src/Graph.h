#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace flexar {
using Id = uint64_t; // Stable, monotonic document-local IDs. Never reused in a graph.
enum class PinType { Texture, Float, Vector2, Color };
enum class Direction { Input, Output };
enum class Effect : uint32_t { Source, Output, Grade, Blur, Halftone, Xerox, Duotone, Grain, Liquid, Neon, Paper, Vignette, Outline, ColorMatrix, Blend, Chromatic, Posterize, Pixelate, Solarize, Scanlines, Mirror, Sharpen, Bloom, Threshold, GlowRays, Streaks, Shake, LensWarp, Prism, Flicker, Custom, Compositor, GridPixel, ColorSteps, RadialChromatic, ColorBalance, EdgeGlow, RadialBlur, Ripple, LumaKey, DropShadow, Mask };
inline constexpr int PresetCount=20;
inline constexpr const char* PresetCodes[]={"x000cc","x001lc","x002ap","x003nm","x004gc","x005lg","x006af","x007ds","x008pg","x009fb","x010vx","x011ht","x012cr","x013ir","x014px","x015gl","x016op","x017pr","x018ch","x019fl"};
inline constexpr const char* PresetNames[]={"Clean","Mercury","Acid press","Neon flow","Tape collage","Ghost optics","Cinema streak","Star filter","Glass shards","Warm flash","Violet ink","Newsprint","CRT signal","Solar ink","Pixel chrome","Edge fire","Optical haze","Prism drift","Chrome noir","Flash print"};
inline constexpr const char* PresetShaders[]={nullptr,"Liquid Chrome","Acid Print","Neon Melt","Glitch Collage","Lens Ghosts","Anamorphic Flare","Diffraction Star","Prismatic Glass","Flash Bloom",nullptr,nullptr,nullptr,nullptr,"Liquid Chrome",nullptr,"Lens Ghosts","Prismatic Glass","Liquid Chrome","Flash Bloom"};
inline constexpr const char* BlendModes[]={"Normal","Multiply","Screen","Overlay","Soft Light","Hard Light","Difference","Exclusion","Add","Subtract","Darken","Lighten"};
struct Pin { Id id{}, node{}; Direction direction{}; PinType type{PinType::Texture}; };
struct Parameter { std::string name; float value, min, max; };
struct PathPoint { float x=0,y=0; };
struct CompositeLayer {
    float x=0,y=0,scaleX=1,scaleY=1,rotation=0,opacity=1;
    int mode=0;bool enabled=true,flipX=false,flipY=false;
    float cropLeft=0,cropTop=0,cropRight=0,cropBottom=0;
    // Vector mask authored with the pen tool. Points live in layer source
    // coordinates (0..1) so the mask follows the layer transform.
    bool locked=false,maskEnabled=true,maskInvert=false;
    float maskFeather=.01f;
    std::vector<PathPoint> path;
};
struct Node {
    Id id{}; Effect effect{}; std::string name; std::string shaderCode;
    std::vector<Pin> inputs, outputs;
    std::vector<Parameter> parameters;
    std::vector<CompositeLayer> layers;
    // Composition metadata drives the studio viewer and timeline.
    int compWidth=1920,compHeight=1080,compFps=30;
    float compDuration=10.f,compBgR=.055f,compBgG=.062f,compBgB=.06f;
    float x{}, y{}; bool enabled{true};
};
inline Id compositionMaskKey(Id node,size_t layer){return node*16+layer;}
struct Connection { Id from{}, to{}; };
struct EffectInfo { Effect effect; const char* name; const char* description; };
const std::vector<EffectInfo>& catalog();
class Graph {
public:
    Id add(Effect effect, float x = 0, float y = 0);
    bool connect(Id from, Id to, std::string& error);
    void disconnect(Id input);
    void setLayerCount(Id node,size_t count);
    void remove(Id node);
    Node* find(Id id);
    const Node* find(Id id) const;
    const Pin* pin(Id id) const;
    std::vector<Id> sorted() const;
    std::vector<Id> execution(Id output) const;
    void preset(int index);
    std::vector<Node> nodes;
    std::vector<Connection> connections;
private:
    Id next_{1};
};
}
