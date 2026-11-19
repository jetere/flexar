#pragma once
#include "Graph.h"
#include <unordered_map>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <filesystem>
namespace flexar {
using Microsoft::WRL::ComPtr;
void verify(HRESULT hr);
struct Image { uint32_t width{},height{}; std::vector<uint8_t> pixels; };
class Renderer {
public:
    void init(HWND window,const std::filesystem::path& shader);
    void shutdown();
    void resize(uint32_t width,uint32_t height);
    void begin();
    void upload(const Image& image);
    uint64_t uploadBackdrop(const Image& image,size_t index);
    void uploadSource(Id id,const Image& image);
    void uploadMask(Id key,const Image& image);
    void bindPrimary(Id id){sourceSlots_[id]=0;}
    void clearSources(){sourceSlots_.clear();maskSlots_.clear();cacheValid_=false;}
    uint64_t sourceHandle(Id id)const {auto it=sourceSlots_.find(id);return it==sourceSlots_.end()?0:gpu(1+it->second*2).ptr;}
    void compileShader(Node& node);
    uint64_t evaluate(const Graph& graph,float time,bool original,Id target=0);
    void present();
    void wait();
    Image readback(uint64_t textureHandle); // Explicit CPU readback for GPU validation, never used by preview.
    uint32_t width() const {return width_;} uint32_t height() const {return height_;}
    uint64_t sourceHandle() const {return gpu(1).ptr;}
private:
    struct Texture {ComPtr<ID3D12Resource> resource;D3D12_RESOURCE_STATES state{D3D12_RESOURCE_STATE_COMMON};};
    void targets();
    void ensureTexture(size_t index);
    void uploadTexture(size_t slot,const Image& image);
    void transition(Texture& texture,D3D12_RESOURCE_STATES state);
    void dispatch(size_t input,size_t output,const Node& node,float time,bool vertical=false,size_t layer=0);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu(size_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(size_t index) const;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swap_;
    ComPtr<ID3D12DescriptorHeap> srv_,rtv_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> commands_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12Resource> back_[2],upload_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    std::vector<Texture> textures_;
    Texture backdrops_[4];
    HANDLE event_{};uint64_t signal_{};UINT descriptorSize_{},rtvSize_{};
    uint32_t width_{},height_{};bool imguiReady_{};
    std::unordered_map<Id,size_t> sourceSlots_;
    std::unordered_map<Id,size_t> maskSlots_;
    std::unordered_map<std::string,ComPtr<ID3D12PipelineState>> customPipelines_;
    std::vector<ComPtr<ID3D12Resource>> uploadKeepAlive_;
    std::vector<size_t> frameRetained_;
    size_t cachedHash_{};uint64_t cachedHandle_{};bool cacheValid_{};
};
}
