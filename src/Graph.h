#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace flexar {
using Id = uint64_t; // Stable, monotonic document-local IDs. Never reused in a graph.
enum class PinType { Texture, Float, Vector2, Color };
enum class Direction { Input, Output };
enum class Effect : uint32_t { Source, Output, Grade, Blur, Halftone, Xerox, Duotone, Grain, Liquid, Neon, Paper, Vignette, Outline, ColorMatrix, Blend, Chromatic, Posterize, Pixelate, Solarize, Scanlines, Mirror, Sharpen, Bloom, Threshold, GlowRays, Streaks, Shake, LensWarp, Prism, Flicker };
inline constexpr int PresetCount=10;
inline constexpr const char* PresetCodes[]={"x000cc","x001vx","x002ht","x003lw","x004sg","x005nf","x006pp","x007cr","x008gl","x009ir"};
inline constexpr const char* BlendModes[]={"Normal","Multiply","Screen","Overlay","Soft Light","Hard Light","Difference","Exclusion","Add","Subtract","Darken","Lighten"};
struct Pin { Id id{}, node{}; Direction direction{}; PinType type{PinType::Texture}; };
struct Parameter { const char* name; float value, min, max; };
struct Node {
    Id id{}; Effect effect{}; std::string name;
    std::vector<Pin> inputs, outputs;
    std::vector<Parameter> parameters;
    float x{}, y{}; bool enabled{true};
};
struct Connection { Id from{}, to{}; };
struct EffectInfo { Effect effect; const char* name; const char* description; };
const std::vector<EffectInfo>& catalog();
class Graph {
public:
    Id add(Effect effect, float x = 0, float y = 0);
    bool connect(Id from, Id to, std::string& error);
    void disconnect(Id input);
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
