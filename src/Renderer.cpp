#include "Renderer.h"
#include <d3dcompiler.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <stdexcept>
#include <cstring>
#include <unordered_map>
#include <cstdio>
#include <bit>
namespace flexar {
void verify(HRESULT hr) {if(FAILED(hr)){char text[64];sprintf_s(text,"Windows / graphics error 0x%08X",static_cast<unsigned>(hr));throw std::runtime_error(text);}}
D3D12_CPU_DESCRIPTOR_HANDLE Renderer::cpu(size_t i)const {auto h=srv_->GetCPUDescriptorHandleForHeapStart();h.ptr+=i*descriptorSize_;return h;}
D3D12_GPU_DESCRIPTOR_HANDLE Renderer::gpu(size_t i)const {auto h=srv_->GetGPUDescriptorHandleForHeapStart();h.ptr+=i*descriptorSize_;return h;}
void Renderer::init(HWND window,const std::filesystem::path& shader) {
    ComPtr<IDXGIFactory4> factory;verify(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    verify(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device_)));
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;verify(device_->CreateCommandQueue(&q,IID_PPV_ARGS(&queue_)));
    DXGI_SWAP_CHAIN_DESC1 sd{};sd.BufferCount=2;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.SampleDesc.Count=1;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;verify(factory->CreateSwapChainForHwnd(queue_.Get(),window,&sd,nullptr,nullptr,&swap));verify(swap.As(&swap_));
    factory->MakeWindowAssociation(window,DXGI_MWA_NO_ALT_ENTER);
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=2;verify(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv_)));
    hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=513;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;verify(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv_)));
    descriptorSize_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);rtvSize_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    verify(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator_)));
    verify(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator_.Get(),nullptr,IID_PPV_ARGS(&commands_)));verify(commands_->Close());
    verify(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)));event_=CreateEvent(nullptr,FALSE,FALSE,nullptr);if(!event_)throw std::runtime_error("Cannot create GPU fence event");
    targets();
    D3D12_DESCRIPTOR_RANGE ranges[3]{};
    ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0};
    ranges[2]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,1,0,0};
    D3D12_ROOT_PARAMETER params[4]{};
    for(int i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i].DescriptorTable={1,&ranges[i]};}
    params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants={0,0,20};
    params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[3].DescriptorTable={1,&ranges[2]};
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=4;rd.pParameters=params;rd.NumStaticSamplers=1;rd.pStaticSamplers=&sampler;
    ComPtr<ID3DBlob> blob,error;verify(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error));verify(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)));
    HRESULT hr=D3DCompileFromFile(shader.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,"main","cs_5_1",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&error);
    if(FAILED(hr))throw std::runtime_error(error?static_cast<const char*>(error->GetBufferPointer()):"Effects.hlsl was not found beside the executable");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root_.Get();pd.CS={blob->GetBufferPointer(),blob->GetBufferSize()};verify(device_->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline_)));
    ImGui_ImplDX12_InitInfo info{};info.Device=device_.Get();info.CommandQueue=queue_.Get();info.NumFramesInFlight=2;info.RTVFormat=sd.Format;info.SrvDescriptorHeap=srv_.Get();info.UserData=this;
    info.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo* i,D3D12_CPU_DESCRIPTOR_HANDLE* c,D3D12_GPU_DESCRIPTOR_HANDLE* g){auto r=static_cast<Renderer*>(i->UserData);*c=r->cpu(0);*g=r->gpu(0);};
    info.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_GPU_DESCRIPTOR_HANDLE){};
    if(!ImGui_ImplDX12_Init(&info))throw std::runtime_error("ImGui initialization failed");imguiReady_=true;
}
void Renderer::targets(){auto h=rtv_->GetCPUDescriptorHandleForHeapStart();for(UINT i=0;i<2;++i){verify(swap_->GetBuffer(i,IID_PPV_ARGS(&back_[i])));device_->CreateRenderTargetView(back_[i].Get(),nullptr,h);h.ptr+=rtvSize_;}}
void Renderer::wait(){if(!queue_||!fence_)return;verify(queue_->Signal(fence_.Get(),++signal_));if(fence_->GetCompletedValue()<signal_){verify(fence_->SetEventOnCompletion(signal_,event_));WaitForSingleObject(event_,INFINITE);}}
void Renderer::resize(uint32_t w,uint32_t h){if(!swap_||!w||!h)return;wait();for(auto& b:back_)b.Reset();verify(swap_->ResizeBuffers(2,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,0));targets();}
void Renderer::shutdown(){wait();if(imguiReady_)ImGui_ImplDX12_Shutdown();imguiReady_=false;if(event_){CloseHandle(event_);event_=nullptr;}}
void Renderer::begin(){wait();upload_.Reset();verify(allocator_->Reset());verify(commands_->Reset(allocator_.Get(),nullptr));ID3D12DescriptorHeap* heaps[]={srv_.Get()};commands_->SetDescriptorHeaps(1,heaps);}
void Renderer::ensureTexture(size_t index){
    if(index>=256)throw std::runtime_error("Preview texture budget exceeded (256 textures)");
    while(textures_.size()<=index){Texture t;D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width_;d.Height=height_;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        verify(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,t.state,nullptr,IID_PPV_ARGS(&t.resource)));
        size_t i=textures_.size();D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.Format=d.Format;s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;s.Texture2D.MipLevels=1;
        device_->CreateShaderResourceView(t.resource.Get(),&s,cpu(1+i*2));
        D3D12_UNORDERED_ACCESS_VIEW_DESC u{};u.Format=d.Format;u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device_->CreateUnorderedAccessView(t.resource.Get(),nullptr,&u,cpu(2+i*2));textures_.push_back(std::move(t));
    }
}
void Renderer::transition(Texture& t,D3D12_RESOURCE_STATES state){if(t.state==state)return;D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={t.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,t.state,state};commands_->ResourceBarrier(1,&b);t.state=state;}
void Renderer::upload(const Image& image){
    cacheValid_=false;
    if(!image.width||!image.height||image.pixels.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Invalid RGBA image");
    if(width_!=image.width||height_!=image.height){textures_.clear();width_=image.width;height_=image.height;}
    ensureTexture(0);transition(textures_[0],D3D12_RESOURCE_STATE_COPY_DEST);
    auto desc=textures_[0].resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT64 bytes;device_->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    verify(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload_)));
    uint8_t* mapped;D3D12_RANGE read{0,0};verify(upload_->Map(0,&read,reinterpret_cast<void**>(&mapped)));
    for(UINT y=0;y<height_;++y)memcpy(mapped+y*footprint.Footprint.RowPitch,image.pixels.data()+size_t(y)*width_*4,size_t(width_)*4);upload_->Unmap(0,nullptr);
    D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=textures_[0].resource.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=upload_.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprint;
    commands_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(textures_[0],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
void Renderer::dispatch(size_t input,size_t output,const Node& n,float time,bool vertical,size_t layer){
    ensureTexture(output);transition(textures_[input],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(textures_[output],D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    struct Constants{uint32_t effect,w,h;float time;float p[16];} c{static_cast<uint32_t>(n.effect),width_,height_,time,{}};
    for(size_t i=0;i<n.parameters.size()&&i<16;++i)c.p[i]=n.parameters[i].value;c.p[12]=vertical?1.f:0.f;
    commands_->SetComputeRootSignature(root_.Get());commands_->SetPipelineState(pipeline_.Get());commands_->SetComputeRootDescriptorTable(0,gpu(1+input*2));commands_->SetComputeRootDescriptorTable(1,gpu(2+output*2));commands_->SetComputeRootDescriptorTable(3,gpu(1+layer*2));commands_->SetComputeRoot32BitConstants(2,20,&c,0);commands_->Dispatch((width_+7)/8,(height_+7)/8,1);
    transition(textures_[output],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
uint64_t Renderer::evaluate(const Graph& graph,float time,bool original){
    if(textures_.empty())return 0;if(original)return sourceHandle();
    size_t hash=std::bit_cast<uint32_t>(time);
    auto mix=[&](size_t v){hash^=v+0x9e3779b9+(hash<<6)+(hash>>2);};
    for(auto& n:graph.nodes){mix(n.id);mix(static_cast<size_t>(n.effect));mix(n.enabled);for(auto& p:n.parameters)mix(std::bit_cast<uint32_t>(p.value));}
    for(auto& c:graph.connections){mix(c.from);mix(c.to);}
    if(cacheValid_&&cachedHash_==hash)return cachedHandle_;
    Id output=0;for(auto& n:graph.nodes)if(n.effect==Effect::Output)output=n.id;
    if(!output)return 0;
    std::unordered_map<Id,size_t> results;
    std::unordered_map<Id,int> remaining;
    const auto order=graph.execution(output);
    for(Id id:order)for(auto& connection:graph.connections)if(graph.pin(connection.to)->node==id)++remaining[graph.pin(connection.from)->node];
    // Reuse only textures whose consumers have all finished. Aliases created by
    // bypass nodes are retained too, so merging graph branches cannot overwrite A or B.
    auto allocate=[&](size_t temporary=0){
        for(size_t slot=1;slot<256;++slot){bool live=slot==temporary;
            for(auto& [id,texture]:results)if(texture==slot&&remaining[id]>0)live=true;
            if(!live)return slot;
        }throw std::runtime_error("No free graph texture");
    };
    for(Id id:order) {
        const Node& n=*graph.find(id);if(n.effect==Effect::Source){results[id]=0;continue;}
        std::vector<Id> upstream;bool connected=true;
        for(auto& pin:n.inputs){Id producer=0;for(auto& c:graph.connections)if(c.to==pin.id)producer=graph.pin(c.from)->node;upstream.push_back(producer);if(!producer||!results.contains(producer))connected=false;}
        bool firstConnected=!upstream.empty()&&results.contains(upstream[0]);
        if((!n.enabled||n.effect==Effect::Output)&&firstConnected)results[id]=results[upstream[0]];
        else if(connected){size_t input=results[upstream[0]],slot=allocate();
            if(n.effect==Effect::Blur){dispatch(input,slot,n,time);input=slot;slot=allocate(input);dispatch(input,slot,n,time,true);}
            else dispatch(input,slot,n,time,false,n.effect==Effect::Blend?results[upstream[1]]:0);
            results[id]=slot;
        }
        for(Id producer:upstream)if(producer)--remaining[producer];
    }
    cachedHandle_=results.contains(output)?gpu(1+results[output]*2).ptr:0;cachedHash_=hash;cacheValid_=true;return cachedHandle_;
}
void Renderer::present(){
    UINT index=swap_->GetCurrentBackBufferIndex();D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={back_[index].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET};commands_->ResourceBarrier(1,&b);
    auto handle=rtv_->GetCPUDescriptorHandleForHeapStart();handle.ptr+=index*rtvSize_;float clear[]={.035f,.04f,.055f,1};commands_->ClearRenderTargetView(handle,clear,0,nullptr);commands_->OMSetRenderTargets(1,&handle,FALSE,nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),commands_.Get());std::swap(b.Transition.StateBefore,b.Transition.StateAfter);commands_->ResourceBarrier(1,&b);verify(commands_->Close());ID3D12CommandList* lists[]={commands_.Get()};queue_->ExecuteCommandLists(1,lists);verify(swap_->Present(1,0));
}
Image Renderer::readback(uint64_t handle){
    const auto first=gpu(1).ptr;
    if(handle<first||(handle-first)%(descriptorSize_*2)!=0)throw std::runtime_error("Invalid readback handle");
    size_t index=(handle-first)/(descriptorSize_*2);if(index>=textures_.size())throw std::runtime_error("Missing readback texture");
    begin();auto& texture=textures_[index];auto desc=texture.resource->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT64 bytes;
    device_->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> staging;verify(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&staging)));
    auto prior=texture.state;transition(texture,D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION from{};from.pResource=texture.resource.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION to{};to.pResource=staging.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;
    commands_->CopyTextureRegion(&to,0,0,0,&from,nullptr);transition(texture,prior);verify(commands_->Close());ID3D12CommandList* lists[]={commands_.Get()};queue_->ExecuteCommandLists(1,lists);wait();
    Image result{width_,height_,std::vector<uint8_t>(size_t(width_)*height_*4)};uint8_t* data;D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};verify(staging->Map(0,&range,reinterpret_cast<void**>(&data)));
    for(UINT row=0;row<height_;++row)memcpy(result.pixels.data()+size_t(row)*width_*4,data+footprint.Offset+size_t(row)*footprint.Footprint.RowPitch,size_t(width_)*4);
    D3D12_RANGE written{0,0};staging->Unmap(0,&written);return result;
}
}
