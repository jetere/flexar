#include "Renderer.h"
#include "Media.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <mfapi.h>
#include <commdlg.h>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <cmath>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
using namespace flexar;
static UINT resizeW{},resizeH{};
static ImFont* gothicTitle{};
static ImFont* gothicHeading{};
static LRESULT WINAPI windowProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(ImGui_ImplWin32_WndProcHandler(h,m,w,l))return 1;
    if(m==WM_SIZE&&w!=SIZE_MINIMIZED){resizeW=LOWORD(l);resizeH=HIWORD(l);return 0;}
    if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
    if(m==WM_GETMINMAXINFO){auto p=reinterpret_cast<MINMAXINFO*>(l);p->ptMinTrackSize={1000,720};return 0;}
    return DefWindowProc(h,m,w,l);
}
static std::filesystem::path dialog(HWND hwnd,bool save,bool project=false,bool video=false){
    wchar_t file[32768]{};OPENFILENAME ofn{sizeof(ofn)};ofn.hwndOwner=hwnd;ofn.lpstrFile=file;ofn.nMaxFile=32768;
    ofn.lpstrFilter=project?L"Flexar graph\0*.flexar\0":video?L"Video\0*.mp4;*.mov;*.mkv;*.avi;*.wmv\0All files\0*.*\0":L"Images\0*.jpg;*.jpeg;*.png;*.bmp;*.tif;*.tiff\0All files\0*.*\0";
    ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);ofn.lpstrDefExt=project?L"flexar":nullptr;
    return (save?GetSaveFileName(&ofn):GetOpenFileName(&ofn))?std::filesystem::path(file):std::filesystem::path{};
}
struct Editor {
    Graph graph;Video video;Image image;std::filesystem::path mediaPath;
    Id selected{},dragPin{},dragNode{};ImVec2 pan{24,36};float zoom=1;int preset{};
    bool showPreview{true},showControls{},focusControls{},canvasHovered{},spacePanning{},largePreview{},largeFullscreen{},importVideo{},hasMedia{},fitRequested{};
    uint64_t sourceTexture{},outputTexture{};
    std::filesystem::path exportPath;Video exportDecoder;VideoWriter exportWriter;
    bool exportActive{},exportStill{},cancelExport{},exportPrepared{};int exportFrame{},exportFrames{};double savedPosition{};Image savedImage;
    void startExport(bool movie){
        if(!hasMedia||!outputTexture)throw std::runtime_error("Load media and connect Output before exporting");
        wchar_t file[32768]{};OPENFILENAME ofn{sizeof(ofn)};ofn.hwndOwner=hwnd;ofn.lpstrFile=file;ofn.nMaxFile=32768;ofn.lpstrFilter=movie?L"MP4 video\0*.mp4\0":L"PNG image\0*.png\0";ofn.lpstrDefExt=movie?L"mp4":L"png";ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT;
        if(!GetSaveFileName(&ofn))return;std::filesystem::path path(file);
        if(!mediaPath.empty()&&std::filesystem::weakly_canonical(path)==std::filesystem::weakly_canonical(mediaPath))throw std::runtime_error("Choose a path different from the source");
        if(movie&&(!video.loaded()||video.duration<=0))throw std::runtime_error("MP4 requires a video with known duration");
        if(movie){exportDecoder.open(mediaPath);exportWriter.open(path,image.width,image.height);}
        exportPath=path;exportStill=!movie;exportFrame=0;exportFrames=movie?std::max(1,static_cast<int>(std::ceil(video.duration*30))):1;savedPosition=position;savedImage=image;playing=false;exportActive=true;cancelExport=false;exportPrepared=false;
    }
    void finishExport(bool canceled=false){
        exportActive=false;exportDecoder.close();image=std::move(savedImage);position=savedPosition;uploadPending=true;
        exportWriter.finish();status=canceled?"Export canceled. Partial MP4 saved.":"Exported "+exportPath.filename().string();
    }
    void prepareExport(){
        if(!exportActive)return;if(cancelExport){finishExport(true);return;}
        if(!exportStill){position=exportFrame/30.;if(exportFrame==0)image={};exportDecoder.frame(position,image);uploadPending=true;}exportPrepared=true;
    }
    void writeExport(Renderer& renderer){
        if(!exportActive||!exportPrepared)return;if(!outputTexture)throw std::runtime_error("Export Output is disconnected");auto result=renderer.readback(outputTexture);exportPrepared=false;
        if(exportStill){savePng(exportPath,result);finishExport();}else{exportWriter.write(result);++exportFrame;if(exportFrame>=exportFrames)finishExport();}
    }
    bool playing{},original{},uploadPending{true};double position{};std::string status="Ready. Open a clip or image to begin.";
    std::filesystem::path root;HWND hwnd{};
    template<class Action> void safely(Action action){try{action();}catch(const std::exception& e){status=e.what();playing=false;}}
    void load(const std::filesystem::path& path){
        if(path.empty())return;
        auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        if(ext==L".jpg"||ext==L".jpeg"||ext==L".png"||ext==L".bmp"||ext==L".tif"||ext==L".tiff"){auto next=loadImage(path);video.close();image=std::move(next);}else{
            Video next;next.open(path);Image first;if(!next.frame(0,first))throw std::runtime_error("Clip contains no decodable video frame");video=std::move(next);image=std::move(first);
        }
        mediaPath=path;position=0;playing=false;uploadPending=true;hasMedia=true;importVideo=video.loaded();status="Loaded "+path.filename().string();
    }
    void arrange(){for(size_t i=0;i<graph.nodes.size();++i){size_t row=i/3,col=row%2?2-i%3:i%3;graph.nodes[i].x=30.f+310.f*col;graph.nodes[i].y=50.f+290.f*row;}pan={24,36};zoom=1;fitRequested=true;}
    void applyPreset(int index){preset=index;graph.preset(index);selected=graph.nodes.size()>2?graph.nodes[1].id:0;dragPin=0;arrange();status=std::string(PresetCodes[index])+" loaded";}
    void addEffect(Effect effect){
        if(graph.nodes.size()>=32){status="This preview build supports up to 32 nodes.";return;}
        Id output=0,previousPin=0;for(auto& n:graph.nodes)if(n.effect==Effect::Output)output=n.id;
        if(output)for(auto& c:graph.connections)if(c.to==graph.find(output)->inputs[0].id)previousPin=c.from;
        selected=graph.add(effect,80.f+float(graph.nodes.size()%3)*310,340);auto node=graph.find(selected);
        std::string error;
        if(previousPin)graph.connect(previousPin,node->inputs[0].id,error);
        if(output)graph.connect(node->outputs[0].id,graph.find(output)->inputs[0].id,error);
        if(effect==Effect::Blend)for(auto& source:graph.nodes)if(source.effect==Effect::Source)graph.connect(source.outputs[0].id,node->inputs[1].id,error);
        showControls=true;status="Added "+node->name;
    }
    void save(const std::filesystem::path& path){
        if(path.empty())return;std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write project");
        out<<"FLEXAR 2\n"<<std::quoted(mediaPath.string())<<'\n'<<graph.nodes.size()<<'\n';
        for(auto& n:graph.nodes){out<<n.id<<' '<<static_cast<int>(n.effect)<<' '<<n.x<<' '<<n.y<<' '<<n.enabled<<' '<<n.parameters.size();for(auto& p:n.parameters)out<<' '<<p.value;out<<'\n';}
        out<<graph.connections.size()<<'\n';for(auto& c:graph.connections){auto target=graph.find(graph.pin(c.to)->node);size_t inputIndex=0;while(target->inputs[inputIndex].id!=c.to)++inputIndex;out<<graph.pin(c.from)->node<<' '<<target->id<<' '<<inputIndex<<'\n';}
        if(!out)throw std::runtime_error("Project write failed");status="Saved "+path.filename().string();
    }
    void restore(const std::filesystem::path& path){
        if(path.empty())return;std::ifstream in(path);std::string tag,media;int version;size_t count;
        if(!(in>>tag>>version)||tag!="FLEXAR"||(version!=1&&version!=2))throw std::runtime_error("Invalid Flexar project");
        if(!(in>>std::quoted(media)>>count)||count<2||count>32)throw std::runtime_error("Invalid node count");
        Graph next;std::unordered_map<Id,Id> ids;int sources=0,outputs=0;
        for(size_t i=0;i<count;++i){Id old;int effect;float x,y;bool enabled;size_t params;
            if(!(in>>old>>effect>>x>>y>>enabled>>params)||effect<0||effect>static_cast<int>(Effect::Flicker)||!std::isfinite(x)||!std::isfinite(y)||ids.contains(old))throw std::runtime_error("Invalid project node");
            auto id=next.add(static_cast<Effect>(effect),x,y);ids[old]=id;auto n=next.find(id);n->enabled=enabled;
            sources+=n->effect==Effect::Source;outputs+=n->effect==Effect::Output;
            if(params!=n->parameters.size())throw std::runtime_error("Invalid parameter count");
            for(auto& p:n->parameters){float value;if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Invalid parameter");p.value=std::clamp(value,p.min,p.max);}
        }
        if(sources!=1||outputs!=1)throw std::runtime_error("Project must have one source and one output");
        if(!(in>>count)||count>64)throw std::runtime_error("Invalid connection count");
        for(size_t i=0;i<count;++i){Id a,b;size_t pinIndex=0;std::string error;if(!(in>>a>>b)||!ids.contains(a)||!ids.contains(b)||(version==2&&!(in>>pinIndex)))throw std::runtime_error("Invalid connection");auto na=next.find(ids[a]),nb=next.find(ids[b]);if(na->outputs.empty()||pinIndex>=nb->inputs.size()||!next.connect(na->outputs[0].id,nb->inputs[pinIndex].id,error))throw std::runtime_error("Invalid graph connection: "+error);}
        if(!media.empty())load(std::filesystem::path(media));else{video.close();image={16,9,std::vector<uint8_t>(16*9*4,0)};mediaPath.clear();position=0;playing=false;uploadPending=true;hasMedia=false;}graph=std::move(next);selected=0;dragPin=0;pan={24,36};fitRequested=true;status="Opened "+path.filename().string();
    }
#include "GraphBackdrop.inl"
#include "EditorUI.inl"
};
static void style(){
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();s.WindowPadding={14,12};s.FramePadding={10,6};s.ItemSpacing={8,8};s.WindowRounding=0;s.ChildRounding=0;s.FrameRounding=0;s.GrabRounding=0;s.PopupRounding=0;s.ScrollbarRounding=0;s.FrameBorderSize=1;s.ChildBorderSize=2;s.AntiAliasedLines=false;s.AntiAliasedFill=false;
    s.Colors[ImGuiCol_WindowBg]={.047f,.039f,.050f,1};s.Colors[ImGuiCol_ChildBg]={.066f,.055f,.068f,1};s.Colors[ImGuiCol_PopupBg]={.085f,.065f,.08f,1};
    s.Colors[ImGuiCol_Text]={.86f,.82f,.77f,1};s.Colors[ImGuiCol_TextDisabled]={.47f,.40f,.43f,1};s.Colors[ImGuiCol_Border]={.23f,.16f,.19f,1};s.Colors[ImGuiCol_Separator]={.26f,.16f,.20f,1};
    s.Colors[ImGuiCol_FrameBg]={.13f,.09f,.115f,1};s.Colors[ImGuiCol_FrameBgHovered]={.22f,.12f,.16f,1};s.Colors[ImGuiCol_FrameBgActive]={.30f,.14f,.19f,1};
    s.Colors[ImGuiCol_Button]={.20f,.105f,.14f,1};s.Colors[ImGuiCol_ButtonHovered]={.34f,.16f,.21f,1};s.Colors[ImGuiCol_ButtonActive]={.43f,.20f,.25f,1};
    s.Colors[ImGuiCol_Header]={.30f,.13f,.19f,1};s.Colors[ImGuiCol_HeaderHovered]={.37f,.18f,.23f,1};s.Colors[ImGuiCol_HeaderActive]={.43f,.21f,.27f,1};
    s.Colors[ImGuiCol_SliderGrab]={.67f,.34f,.42f,1};s.Colors[ImGuiCol_SliderGrabActive]={.87f,.52f,.59f,1};s.Colors[ImGuiCol_CheckMark]={.83f,.49f,.55f,1};
}
#include "GpuValidation.inl"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int){
    bool smoke=std::wstring(command).find(L"--smoke-test")!=std::wstring::npos;
    Renderer renderer;bool context=false,winBackend=false,mf=false,com=false;HWND window=nullptr;int result=0;
    try{
        verify(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));com=true;verify(MFStartup(MF_VERSION));mf=true;
        ImGui_ImplWin32_EnableDpiAwareness();WNDCLASSEX wc{sizeof(wc)};wc.style=CS_CLASSDC;wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"FlexarWindow";RegisterClassEx(&wc);
        window=CreateWindow(wc.lpszClassName,L"flexar",WS_OVERLAPPEDWINDOW,80,60,1480,940,nullptr,nullptr,instance,nullptr);if(!window)throw std::runtime_error("Cannot create editor window");
        IMGUI_CHECKVERSION();ImGui::CreateContext();context=true;ImGui::GetIO().IniFilename=nullptr;style();
        ImGui_ImplWin32_Init(window);winBackend=true;wchar_t module[32768];GetModuleFileName(nullptr,module,32768);auto executable=std::filesystem::path(module).parent_path();
        auto font=executable/L"assets/fonts/GothicByte.ttf";
        if(!std::filesystem::exists(font))throw std::runtime_error("Missing assets/fonts/GothicByte.ttf beside flexar");
        ImFontConfig fontConfig;fontConfig.OversampleH=1;fontConfig.OversampleV=1;fontConfig.PixelSnapH=true;
        ImFontConfig bodyConfig;bodyConfig.SizePixels=13;bodyConfig.OversampleH=1;bodyConfig.OversampleV=1;bodyConfig.PixelSnapH=true;
        auto bodyFont=ImGui::GetIO().Fonts->AddFontDefault(&bodyConfig);bodyFont->Scale=1.25f;
        gothicTitle=ImGui::GetIO().Fonts->AddFontFromFileTTF(font.string().c_str(),46,&fontConfig);
        gothicHeading=ImGui::GetIO().Fonts->AddFontFromFileTTF(font.string().c_str(),24,&fontConfig);
        renderer.init(window,executable/L"shaders/Effects.hlsl");
        if(smoke)validateBlendPixels(renderer);
        Editor editor;editor.hwnd=window;editor.image={16,9,std::vector<uint8_t>(16*9*4,0)};editor.applyPreset(0);editor.status="Choose IMAGE or VIDEO, then import your own media.";
        if(smoke){editor.addEffect(Effect::Blend);editor.save(executable/L"smoke-roundtrip.flexar");editor.restore(executable/L"smoke-roundtrip.flexar");if(editor.graph.nodes.size()!=4||editor.graph.connections.size()!=4)throw std::runtime_error("Blend project roundtrip failed");editor.image=demoImage();editor.hasMedia=true;}
        if(!smoke){ShowWindow(window,SW_SHOWDEFAULT);UpdateWindow(window);}
        auto last=std::chrono::steady_clock::now();bool quit=false;int frames=0;
        while(!quit){MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);if(msg.message==WM_QUIT)quit=true;}if(quit)break;
            if(IsIconic(window)){WaitMessage();last=std::chrono::steady_clock::now();continue;}
            if(resizeW&&resizeH){renderer.resize(resizeW,resizeH);resizeW=resizeH=0;}
            auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-last).count();last=now;
            if(editor.playing)editor.safely([&]{editor.position+=dt;double duration=editor.video.loaded()?editor.video.duration:10.;if(duration>0&&editor.position>duration){editor.position=std::fmod(editor.position,duration);editor.video.seek(editor.position);}if(editor.video.loaded())editor.uploadPending=editor.video.frame(editor.position,editor.image)||editor.uploadPending;});
            if(editor.exportActive){try{editor.prepareExport();}catch(const std::exception& e){auto error=std::string(e.what());try{editor.finishExport(true);}catch(...){}editor.status="Export failed: "+error;}}
            renderer.begin();if(editor.uploadPending){renderer.upload(editor.image);editor.uploadPending=false;}
            ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
            try{editor.ui(renderer);}catch(const std::exception& e){editor.status=e.what();/* Preserve ImGui stack by treating UI errors as fatal below. */throw;}
            ImGui::Render();renderer.present();
            if(editor.exportActive){try{editor.writeExport(renderer);}catch(const std::exception& e){auto error=std::string(e.what());try{editor.finishExport(true);}catch(...){}editor.status="Export failed: "+error;}}
            if(smoke){++frames;if(frames<=PresetCount)editor.applyPreset(frames-1);else if(frames<=PresetCount+static_cast<int>(catalog().size())){editor.applyPreset(0);editor.addEffect(catalog()[frames-PresetCount-1].effect);editor.largePreview=true;editor.largeFullscreen=frames%2==0;}else if(frames<=PresetCount+static_cast<int>(catalog().size())+12){editor.applyPreset(0);editor.addEffect(Effect::Blend);for(auto& node:editor.graph.nodes)if(node.effect==Effect::Blend)node.parameters[0].value=static_cast<float>(frames-PresetCount-catalog().size()-1);}else quit=true;}
        }
        renderer.wait();
    }catch(const std::exception& e){result=1;if(smoke){std::ofstream log("smoke-error.txt");log<<e.what();}else MessageBoxA(window,e.what(),"Flexar",MB_OK|MB_ICONERROR);}
    try{renderer.shutdown();}catch(...){}if(winBackend)ImGui_ImplWin32_Shutdown();if(context)ImGui::DestroyContext();if(window)DestroyWindow(window);if(mf)MFShutdown();if(com)CoUninitialize();return result;
}
