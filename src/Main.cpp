#include "Renderer.h"
#include "Media.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <mfapi.h>
#include <commdlg.h>
#include <chrono>
#include <algorithm>
#include <bit>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <deque>
#include <unordered_map>
#include <initializer_list>
#include <cmath>
#include "AmbientMusic.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
using namespace flexar;
static UINT resizeW{},resizeH{};
static ImFont* uiTitle{};
static ImFont* uiHeading{};
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
// Assets live beside the executable or in the project's Music folder (one level up).
static std::filesystem::path findAsset(const std::filesystem::path& root,const std::wstring& filename,std::initializer_list<const wchar_t*> folders){
    std::error_code ec;
    for(auto base:{root,root.parent_path()}){
        if(base.empty())continue;
        for(auto folder:folders){
            auto path=base/std::filesystem::path(folder)/filename;
            if(std::filesystem::exists(path,ec))return path;
        }
    }
    return {};
}
struct Editor {
    struct Clip {Video video;Image image;std::filesystem::path path;bool dirty{true};};
    std::string sourceSignature;
    std::unordered_map<Id,Clip> clips;std::unordered_map<Id,uint64_t> thumbnails;Renderer* runtime{};char shaderBuffer[65537]{};Id editingShader{};
    AmbientMusic* music{};
    // Dark Swiss palette: graphite panels + one accent sampled from the active wallpaper.
    ImVec4 accent{.16f,.62f,.60f,1},ink{.92f,.91f,.88f,1},paper{.072f,.078f,.076f,1};
    ImU32 accentU32{IM_COL32(41,158,153,255)};
    void applyTheme(){
        static const ImVec4 palette[4]={
            {.16f,.62f,.60f,1}, // 80 / teal glass
            {.55f,.36f,.86f,1}, // 91 / violet structure
            {.16f,.52f,.86f,1}, // 93 / blue facets
            {.95f,.66f,.16f,1}  // 117 / amber field
        };
        accent=palette[std::clamp(backdropStyle,0,3)];
        accentU32=IM_COL32(static_cast<int>(accent.x*255),static_cast<int>(accent.y*255),static_cast<int>(accent.z*255),255);
        auto& s=ImGui::GetStyle();
        s.Colors[ImGuiCol_Header]={accent.x,accent.y,accent.z,.18f};
        s.Colors[ImGuiCol_HeaderHovered]={accent.x,accent.y,accent.z,.34f};
        s.Colors[ImGuiCol_HeaderActive]=accent;
        s.Colors[ImGuiCol_Button]={.105f,.115f,.112f,1};
        s.Colors[ImGuiCol_ButtonHovered]={accent.x,accent.y,accent.z,.45f};
        s.Colors[ImGuiCol_ButtonActive]=accent;
        s.Colors[ImGuiCol_TitleBgActive]=accent;
        s.Colors[ImGuiCol_ResizeGripHovered]=accent;
        s.Colors[ImGuiCol_ResizeGripActive]=accent;
        s.Colors[ImGuiCol_SliderGrab]=accent;
        s.Colors[ImGuiCol_SliderGrabActive]=ink;
        s.Colors[ImGuiCol_CheckMark]=accent;
        s.Colors[ImGuiCol_SeparatorHovered]=accent;
        s.Colors[ImGuiCol_SeparatorActive]=accent;
        s.Colors[ImGuiCol_NavHighlight]=accent;
        s.Colors[ImGuiCol_TextSelectedBg]={accent.x,accent.y,accent.z,.35f};
    }
    bool nodeDeletionFocus{};
    // Snapshot history. Snapshots reuse the project format so they capture graph,
    // composition, mask, custom shader, source clip, and primary media state.
    std::deque<std::string> undoStack_,redoStack_;
    std::string historyBaseline_;std::unordered_map<Id,Id> lastIdMap_;bool historyArmed_{};
    void removeSelectedNode(){
        if(exportActive)return;auto node=graph.find(selected);if(!node)return;
        if(node->effect==Effect::Output||selected==primarySource()){status="The primary Source and Output are required. Other nodes can be removed with Delete or Backspace.";return;}
        Id removed=selected;std::string name=node->name;graph.remove(removed);clips.erase(removed);thumbnails.erase(removed);
        if(compositionNode==removed){showComposition=false;focusComposition=false;compositionNode=0;}
        if(editingShader==removed){editingShader=0;shaderBuffer[0]=0;}
        selected=dragPin=dragNode=0;spacePanning=false;showControls=false;focusControls=false;recoverControls=false;
        status="Removed "+name+". Reconnect its neighbors.";
    }
    void deleteNodeShortcut(){
        auto& io=ImGui::GetIO();
        if(!nodeDeletionFocus||exportActive||io.WantTextInput||ImGui::IsAnyItemActive()||io.KeyCtrl||io.KeyAlt||io.KeyShift||io.KeySuper||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))return;
        if(ImGui::IsKeyPressed(ImGuiKey_Delete,false)||ImGui::IsKeyPressed(ImGuiKey_Backspace,false))removeSelectedNode();
    }
    Id primarySource()const{for(auto& n:graph.nodes)if(n.effect==Effect::Source)return n.id;return 0;}
    void loadSource(const std::filesystem::path& path){
        if(path.empty())return;if(!graph.find(selected)||graph.find(selected)->effect!=Effect::Source||selected==primarySource()){load(path);return;}
        Clip clip;clip.path=path;auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        if(ext==L".png"||ext==L".jpg"||ext==L".jpeg"||ext==L".bmp"||ext==L".tif"||ext==L".tiff")clip.image=loadImage(path);else{clip.video.open(path);if(!clip.video.frame(0,clip.image))throw std::runtime_error("No decodable frame");}
        clips[selected]=std::move(clip);status="Source loaded";
    }
    void syncSources(Renderer& renderer){
        std::string signature=std::to_string(renderer.width())+":"+std::to_string(renderer.height())+":"+std::to_string(primarySource());
        for(auto& [id,clip]:clips)if(graph.find(id))signature+=":"+std::to_string(id)+clip.path.string();
        if(signature!=sourceSignature){renderer.clearSources();maskSignatures.clear();renderer.bindPrimary(primarySource());for(auto& [id,clip]:clips)clip.dirty=true;sourceSignature=signature;}
        thumbnails.clear();thumbnails[primarySource()]=hasMedia?renderer.sourceHandle():0;
        for(auto& [id,clip]:clips)if(graph.find(id)){
            if(clip.video.loaded())clip.dirty=clip.video.frame(std::min(position,std::max(0.,clip.video.duration-1./30.)),clip.image)||clip.dirty;
            if(clip.dirty){renderer.uploadSource(id,clip.image);clip.dirty=false;}thumbnails[id]=renderer.sourceHandle(id);
        }
        syncMasks(renderer);
    }
    void addShader(const std::string& code,const std::string& name){if(graph.nodes.size()>=32)throw std::runtime_error("Maximum 32 nodes");Node candidate;candidate.effect=Effect::Custom;candidate.shaderCode=code;runtime->compileShader(candidate);addEffect(Effect::Custom);auto n=graph.find(selected);n->shaderCode=code;n->parameters=std::move(candidate.parameters);n->name=name;strncpy_s(shaderBuffer,code.c_str(),_TRUNCATE);}
    Graph graph;Video video;Image image;std::filesystem::path mediaPath;
    bool focusPreview{},focusComposition{},recoverControls{};
    void openPreview(){if(!largePreview)largeFullscreen=false;largePreview=true;focusPreview=true;dragPin=dragNode=0;spacePanning=false;}
    void openComposition(Id id){if(!graph.find(id)||graph.find(id)->effect!=Effect::Compositor)return;if(compositionNode!=id)compositionLayer=0;compositionNode=id;showComposition=true;focusComposition=true;dragPin=dragNode=0;spacePanning=false;}
    void openControls(Id id,bool recover=false){selected=id;showControls=true;focusControls=true;recoverControls=recover;}
    Id compositionNode{};int compositionLayer{};bool showComposition{};
    // Per-layer vector masks uploaded as small alpha textures.
    std::unordered_map<Id,size_t> maskSignatures;
    // Composition studio interaction state.
    int studioTool{};ImVec2 studioPan{0,0};float studioZoom=1;
    int studioHandle=-1;bool studioLayerDrag{};ImVec2 studioDragStart{};float studioStartX{},studioStartY{},studioStartScaleX{1},studioStartScaleY{1},studioStartRotation{};
    // Upload masks only when their path or settings change.
    static Image rasterizeMask(const CompositeLayer& layer,uint32_t width,uint32_t height){
        Image image{width,height,std::vector<uint8_t>(size_t(width)*height*4,0)};
        std::vector<ImVec2> points;for(auto& p:layer.path)points.push_back({p.x*width,p.y*height});
        std::vector<uint8_t> coverage(size_t(width)*height,0);
        if(points.size()>=3){
            for(uint32_t y=0;y<height;++y){float py=y+.5f;std::vector<float> crossings;
                for(size_t i=0;i<points.size();++i){auto& a=points[i];auto& b=points[(i+1)%points.size()];if((a.y<=py)!=(b.y<=py)){float t=(py-a.y)/(b.y-a.y);crossings.push_back(a.x+t*(b.x-a.x));}}
                std::sort(crossings.begin(),crossings.end());
                for(size_t k=0;k+1<crossings.size();k+=2){int x0=std::max(0,int(std::ceil(crossings[k]-.5f)));int x1=std::min(int(width)-1,int(std::floor(crossings[k+1]-.5f)));for(int x=x0;x<=x1;++x)coverage[size_t(y)*width+x]=255;}
            }
        }
        int radius=std::clamp(int(std::lround(layer.maskFeather*float(std::min(width,height)))),1,std::max(1,int(std::min(width,height)/4)));
        std::vector<uint8_t> temporary(size_t(width)*height,0);
        auto blurRows=[&](const std::vector<uint8_t>& from,std::vector<uint8_t>& to){int window=radius*2+1;for(uint32_t y=0;y<height;++y){const uint8_t* row=from.data()+size_t(y)*width;uint8_t* out=to.data()+size_t(y)*width;int sum=0;for(int k=-radius;k<=radius;++k)sum+=row[std::clamp(k,0,int(width)-1)];for(uint32_t x=0;x<width;++x){out[x]=uint8_t(sum/window);sum+=row[std::clamp(int(x)+radius+1,0,int(width)-1)]-row[std::clamp(int(x)-radius,0,int(width)-1)];}}};
        auto blurColumns=[&](const std::vector<uint8_t>& from,std::vector<uint8_t>& to){int window=radius*2+1;for(uint32_t x=0;x<width;++x){int sum=0;for(int k=-radius;k<=radius;++k)sum+=from[size_t(std::clamp(k,0,int(height)-1))*width+x];for(uint32_t y=0;y<height;++y){to[size_t(y)*width+x]=uint8_t(sum/window);sum+=from[size_t(std::clamp(int(y)+radius+1,0,int(height)-1))*width+x]-from[size_t(std::clamp(int(y)-radius,0,int(height)-1))*width+x];}}};
        blurRows(coverage,temporary);blurColumns(temporary,coverage);
        for(size_t i=0;i<coverage.size();++i){uint8_t value=layer.maskInvert?uint8_t(255-coverage[i]):coverage[i];auto* pixel=image.pixels.data()+i*4;pixel[0]=pixel[1]=pixel[2]=value;pixel[3]=255;}
        return image;
    }
    void syncMasks(Renderer& renderer){
        uint32_t width=renderer.width(),height=renderer.height();if(!width||!height)return;
        for(auto& n:graph.nodes){if(n.effect!=Effect::Compositor)continue;for(size_t i=0;i<n.layers.size();++i){auto& layer=n.layers[i];Id key=compositionMaskKey(n.id,i);
            if(!layer.maskEnabled||layer.path.size()<3)continue;
            size_t signature=1469598103934665603ull;auto mix=[&](size_t v){signature^=v+0x9e3779b9+(signature<<6)+(signature>>2);};
            mix(width);mix(height);mix(std::bit_cast<uint32_t>(layer.maskFeather));mix(layer.maskInvert);mix(layer.path.size());
            for(auto& p:layer.path){mix(std::bit_cast<uint32_t>(p.x));mix(std::bit_cast<uint32_t>(p.y));}
            if(!maskSignatures.contains(key)||maskSignatures.at(key)!=signature){renderer.uploadMask(key,rasterizeMask(layer,width,height));maskSignatures[key]=signature;}
        }}
    }
    double playbackDuration()const{if(showComposition)if(auto node=graph.find(compositionNode))return std::max(.1,double(node->compDuration));return video.loaded()?video.duration:10.;}
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
        for(auto& [id,clip]:clips)if(std::filesystem::weakly_canonical(path)==std::filesystem::weakly_canonical(clip.path))throw std::runtime_error("Choose a path different from every source");
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
    void arrange(){for(size_t i=0;i<graph.nodes.size();++i){size_t row=i/3,col=row%2?2-i%3:i%3;graph.nodes[i].x=30.f+270.f*col;graph.nodes[i].y=50.f+230.f*row;}pan={24,36};zoom=1;fitRequested=true;}
    void applyPreset(int index){Graph next;next.preset(index);for(auto& node:next.nodes)if(node.effect==Effect::Custom){std::ifstream in(root/"shaders/godot"/(node.name+".gdshader"));if(!in)throw std::runtime_error("Missing look shader: "+node.name);node.shaderCode=std::string(std::istreambuf_iterator<char>(in),{});runtime->compileShader(node);}preset=index;showComposition=false;clips.clear();graph=std::move(next);selected=graph.nodes.size()>2?graph.nodes[1].id:0;dragPin=0;arrange();status=std::string(PresetCodes[index])+" loaded";}
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
    void writeState(std::ostream& out)const{
        out<<"FLEXAR 5\n"<<std::quoted(mediaPath.string())<<'\n'<<graph.nodes.size()<<'\n';
        for(auto& n:graph.nodes){out<<n.id<<' '<<static_cast<int>(n.effect)<<' '<<n.x<<' '<<n.y<<' '<<n.enabled<<' '<<std::quoted(n.shaderCode)<<' '<<std::quoted(clips.contains(n.id)?clips.at(n.id).path.string():std::string{})<<' '<<std::quoted(n.name)<<' '<<n.compWidth<<' '<<n.compHeight<<' '<<n.compDuration<<' '<<n.compFps<<' '<<n.compBgR<<' '<<n.compBgG<<' '<<n.compBgB<<' '<<n.layers.size();for(auto& l:n.layers){out<<' '<<l.x<<' '<<l.y<<' '<<l.scaleX<<' '<<l.scaleY<<' '<<l.rotation<<' '<<l.opacity<<' '<<l.mode<<' '<<l.enabled<<' '<<l.flipX<<' '<<l.flipY<<' '<<l.cropLeft<<' '<<l.cropTop<<' '<<l.cropRight<<' '<<l.cropBottom<<' '<<l.locked<<' '<<l.maskEnabled<<' '<<l.maskInvert<<' '<<l.maskFeather<<' '<<l.path.size();for(auto& p:l.path)out<<' '<<p.x<<' '<<p.y;}out<<' '<<n.parameters.size();for(auto& p:n.parameters)out<<' '<<p.value;out<<'\n';}
        out<<graph.connections.size()<<'\n';for(auto& c:graph.connections){auto target=graph.find(graph.pin(c.to)->node);size_t inputIndex=0;while(target->inputs[inputIndex].id!=c.to)++inputIndex;out<<graph.pin(c.from)->node<<' '<<target->id<<' '<<inputIndex<<'\n';}
        if(!out)throw std::runtime_error("Project write failed");
    }
    void save(const std::filesystem::path& path){
        if(path.empty())return;std::ofstream out(path);if(!out)throw std::runtime_error("Cannot write project");
        writeState(out);status="Saved "+path.filename().string();
    }
    std::string captureState()const{std::ostringstream out;writeState(out);return out.str();}
    void readState(std::istream& in,bool preserveView=false){
        Id previousSelected=selected,previousComposition=compositionNode;
        std::string tag,media;int version;size_t count;
        if(!(in>>tag>>version)||tag!="FLEXAR"||(version!=1&&version!=2&&version!=3&&version!=4&&version!=5))throw std::runtime_error("Invalid Flexar project");
        if(!(in>>std::quoted(media)>>count)||count<2||count>32)throw std::runtime_error("Invalid node count");
        Graph next;std::unordered_map<Id,Id> ids;int sources=0,outputs=0;std::unordered_map<Id,std::filesystem::path> extraPaths;
        for(size_t i=0;i<count;++i){Id old;int effect;float x,y;bool enabled;size_t params;std::string code,clipPath,nodeName;std::vector<CompositeLayer> layers;
            if(!(in>>old>>effect>>x>>y>>enabled)||effect<0||effect>static_cast<int>(Effect::DropShadow)||!std::isfinite(x)||!std::isfinite(y)||ids.contains(old))throw std::runtime_error("Invalid project node");
            if(version>=3&&!(in>>std::quoted(code)>>std::quoted(clipPath)))throw std::runtime_error("Invalid node media/shader");
            int compWidth=1920,compHeight=1080,compFps=30;float compDuration=10,compBgR=.055f,compBgG=.062f,compBgB=.06f;
            if(version>=4&&!(in>>std::quoted(nodeName)))throw std::runtime_error("Invalid node name");
            if(version>=5){if(!(in>>compWidth>>compHeight>>compDuration>>compFps>>compBgR>>compBgG>>compBgB))throw std::runtime_error("Invalid composition settings");
                if(compWidth<16||compWidth>7680||compHeight<16||compHeight>7680||compFps<1||compFps>240||!std::isfinite(compDuration)||compDuration<.1f||compDuration>3600||!std::isfinite(compBgR)||!std::isfinite(compBgG)||!std::isfinite(compBgB)||compBgR<0||compBgR>1||compBgG<0||compBgG>1||compBgB<0||compBgB>1)throw std::runtime_error("Invalid composition settings");}
            if(version>=4){size_t layerCount;if(!(in>>layerCount)||layerCount>8||(effect==int(Effect::Compositor)?layerCount<1:layerCount!=0))throw std::runtime_error("Invalid layer count");layers.resize(layerCount);for(auto& l:layers){if(!(in>>l.x>>l.y>>l.scaleX>>l.scaleY>>l.rotation>>l.opacity>>l.mode>>l.enabled>>l.flipX>>l.flipY>>l.cropLeft>>l.cropTop>>l.cropRight>>l.cropBottom))throw std::runtime_error("Invalid layer");for(float v:{l.x,l.y,l.scaleX,l.scaleY,l.rotation,l.opacity,l.cropLeft,l.cropTop,l.cropRight,l.cropBottom})if(!std::isfinite(v))throw std::runtime_error("Non-finite transform");if(l.scaleX<.01f||l.scaleY<.01f||l.scaleX>10||l.scaleY>10||std::abs(l.x)>2||std::abs(l.y)>2||std::abs(l.rotation)>360||l.opacity<0||l.opacity>1||l.mode<0||l.mode>11||l.cropLeft<0||l.cropTop<0||l.cropRight<0||l.cropBottom<0||l.cropLeft+l.cropRight>=1||l.cropTop+l.cropBottom>=1)throw std::runtime_error("Invalid transform range");
                if(version>=5){size_t pathCount=0;if(!(in>>l.locked>>l.maskEnabled>>l.maskInvert>>l.maskFeather>>pathCount)||pathCount>64||!std::isfinite(l.maskFeather)||l.maskFeather<0||l.maskFeather>.5f)throw std::runtime_error("Invalid mask settings");l.path.resize(pathCount);for(auto& p:l.path)if(!(in>>p.x>>p.y)||!std::isfinite(p.x)||!std::isfinite(p.y)||std::abs(p.x)>4||std::abs(p.y)>4)throw std::runtime_error("Invalid mask point");}
            }}
            if(!(in>>params))throw std::runtime_error("Invalid parameters");
            auto id=next.add(static_cast<Effect>(effect),x,y);ids[old]=id;auto n=next.find(id);n->enabled=enabled;n->shaderCode=code;n->compWidth=compWidth;n->compHeight=compHeight;n->compFps=compFps;n->compDuration=compDuration;n->compBgR=compBgR;n->compBgG=compBgG;n->compBgB=compBgB;if(n->effect==Effect::Compositor&&version>=4){next.setLayerCount(id,layers.size());n->layers=layers;}if(n->effect==Effect::Custom){runtime->compileShader(*n);n->name="Godot Shader";}if(version>=4&&!nodeName.empty())n->name=nodeName;if(!clipPath.empty())extraPaths[id]=clipPath;
            sources+=n->effect==Effect::Source;outputs+=n->effect==Effect::Output;
            if(params!=n->parameters.size())throw std::runtime_error("Invalid parameter count");
            for(auto& p:n->parameters){float value;if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Invalid parameter");p.value=std::clamp(value,p.min,p.max);}
        }
        if(sources<1||outputs!=1)throw std::runtime_error("Project must have sources and one output");
        if(!(in>>count)||count>256)throw std::runtime_error("Invalid connection count");
        for(size_t i=0;i<count;++i){Id a,b;size_t pinIndex=0;std::string error;if(!(in>>a>>b)||!ids.contains(a)||!ids.contains(b)||(version>=2&&!(in>>pinIndex)))throw std::runtime_error("Invalid connection");auto na=next.find(ids[a]),nb=next.find(ids[b]);if(na->outputs.empty()||pinIndex>=nb->inputs.size()||!next.connect(na->outputs[0].id,nb->inputs[pinIndex].id,error))throw std::runtime_error("Invalid graph connection: "+error);}
        lastIdMap_=ids;
        if(!media.empty())load(std::filesystem::path(media));else{video.close();image={16,9,std::vector<uint8_t>(16*9*4,0)};mediaPath.clear();position=0;playing=false;uploadPending=true;hasMedia=false;}
        graph=std::move(next);showComposition=false;focusComposition=false;clips.clear();
        for(auto& [id,clipPath]:extraPaths){selected=id;loadSource(clipPath);}
        selected=0;dragPin=dragNode=0;sourceSignature.clear();maskSignatures.clear();
        auto remap=[&](Id id){auto it=lastIdMap_.find(id);return it==lastIdMap_.end()?Id{}:it->second;};
        if(preserveView){
            selected=remap(previousSelected);
            if(!selected||!graph.find(selected))selected=graph.nodes.size()>1?graph.nodes[1].id:0;
            Id composition=remap(previousComposition);
            if(composition&&graph.find(composition)&&graph.find(composition)->effect==Effect::Compositor)compositionNode=composition;
            else{compositionNode=0;showComposition=false;focusComposition=false;}
        }else{selected=0;compositionNode=0;showComposition=false;focusComposition=false;pan={24,36};zoom=1;fitRequested=true;}
    }
    void restore(const std::filesystem::path& path){
        if(path.empty())return;std::ifstream in(path);if(!in)throw std::runtime_error("Cannot open project");
        historyArmed_=false;readState(in,false);status="Opened "+path.filename().string();
    }
    void pushHistory(std::string snapshot){
        undoStack_.push_back(std::move(snapshot));if(undoStack_.size()>64)undoStack_.pop_front();redoStack_.clear();
    }
    void applySnapshot(const std::string& snapshot){
        std::istringstream in(snapshot);historyArmed_=false;readState(in,true);
    }
    bool canUndo()const{return !undoStack_.empty();}
    bool canRedo()const{return !redoStack_.empty();}
    void undo(){
        if(exportActive||undoStack_.empty())return;
        redoStack_.push_back(captureState());std::string snapshot=std::move(undoStack_.back());undoStack_.pop_back();
        applySnapshot(snapshot);status="Undo";
    }
    void redo(){
        if(exportActive||redoStack_.empty())return;
        undoStack_.push_back(captureState());std::string snapshot=std::move(redoStack_.back());redoStack_.pop_back();
        applySnapshot(snapshot);status="Redo";
    }
    void beginHistoryFrame(){
        if(!historyArmed_||!ImGui::IsMouseDown(0)){historyBaseline_=captureState();historyArmed_=true;}
    }
    void commitHistoryFrame(){
        if(!historyArmed_||ImGui::IsMouseDown(0))return;
        if(captureState()!=historyBaseline_)pushHistory(historyBaseline_);
        historyArmed_=false;
    }
    void historyShortcut(){
        auto& io=ImGui::GetIO();
        if(exportActive||io.WantTextInput||!io.KeyCtrl||io.KeyAlt||io.KeySuper)return;
        if(ImGui::IsKeyPressed(ImGuiKey_Z,false)){if(io.KeyShift)redo();else undo();}
        else if(ImGui::IsKeyPressed(ImGuiKey_Y,false))redo();
    }
#include "GraphBackdrop.inl"
#include "CompositorUI.inl"
#include "EditorUI.inl"
};
static void style(){
    // Dark Swiss surface: graphite, hard edges, off-white type, one active accent.
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
    s.WindowPadding={14,14};s.FramePadding={10,6};s.ItemSpacing={8,8};
    s.WindowRounding=s.ChildRounding=s.FrameRounding=s.PopupRounding=s.GrabRounding=s.ScrollbarRounding=0;
    s.FrameBorderSize=s.ChildBorderSize=s.WindowBorderSize=1;s.AntiAliasedLines=true;s.AntiAliasedFill=true;
    const ImVec4 ink{.92f,.91f,.88f,1},panel{.072f,.078f,.076f,1};
    s.Colors[ImGuiCol_WindowBg]={.048f,.054f,.052f,.96f};
    s.Colors[ImGuiCol_ChildBg]={panel.x,panel.y,panel.z,.92f};
    s.Colors[ImGuiCol_PopupBg]={.062f,.068f,.066f,.99f};
    s.Colors[ImGuiCol_Text]=ink;s.Colors[ImGuiCol_TextDisabled]={.49f,.52f,.50f,1};
    s.Colors[ImGuiCol_Border]={.20f,.22f,.21f,1};s.Colors[ImGuiCol_BorderShadow]={0,0,0,0};
    s.Colors[ImGuiCol_Separator]={.22f,.24f,.23f,1};
    s.Colors[ImGuiCol_FrameBg]={.105f,.115f,.112f,1};s.Colors[ImGuiCol_FrameBgHovered]={.14f,.16f,.155f,1};s.Colors[ImGuiCol_FrameBgActive]={.18f,.20f,.195f,1};
    s.Colors[ImGuiCol_Button]={.105f,.115f,.112f,1};s.Colors[ImGuiCol_ButtonHovered]={.17f,.19f,.185f,1};s.Colors[ImGuiCol_ButtonActive]={.24f,.27f,.26f,1};
    s.Colors[ImGuiCol_Header]={.13f,.15f,.145f,1};s.Colors[ImGuiCol_HeaderHovered]={.17f,.19f,.185f,1};s.Colors[ImGuiCol_HeaderActive]={.24f,.27f,.26f,1};
    s.Colors[ImGuiCol_TitleBg]={.035f,.040f,.039f,1};s.Colors[ImGuiCol_TitleBgActive]={.10f,.115f,.11f,1};s.Colors[ImGuiCol_TitleBgCollapsed]={.035f,.040f,.039f,1};
    s.Colors[ImGuiCol_ResizeGrip]={.25f,.27f,.26f,.5f};
    s.Colors[ImGuiCol_ScrollbarBg]={.055f,.062f,.060f,1};s.Colors[ImGuiCol_ScrollbarGrab]={.30f,.33f,.32f,1};
    s.Colors[ImGuiCol_TabActive]={.20f,.22f,.21f,1};s.Colors[ImGuiCol_TabHovered]={.17f,.19f,.185f,1};
}
#include "GpuValidation.inl"
#include "WindowValidation.inl"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int){
    bool smoke=std::wstring(command).find(L"--smoke-test")!=std::wstring::npos;
    Renderer renderer;bool context=false,winBackend=false,mf=false,com=false;HWND window=nullptr;int result=0;
    try{
        verify(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));com=true;verify(MFStartup(MF_VERSION));mf=true;
        ImGui_ImplWin32_EnableDpiAwareness();WNDCLASSEX wc{sizeof(wc)};wc.style=CS_CLASSDC;wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"FlexarWindow";RegisterClassEx(&wc);
        window=CreateWindow(wc.lpszClassName,L"flexar",WS_OVERLAPPEDWINDOW,80,60,1480,940,nullptr,nullptr,instance,nullptr);if(!window)throw std::runtime_error("Cannot create editor window");
        IMGUI_CHECKVERSION();ImGui::CreateContext();context=true;ImGui::GetIO().IniFilename=nullptr;style();
        ImGui_ImplWin32_Init(window);winBackend=true;wchar_t module[32768];GetModuleFileName(nullptr,module,32768);auto executable=std::filesystem::path(module).parent_path();
        wchar_t windowsDirectory[MAX_PATH];GetWindowsDirectoryW(windowsDirectory,MAX_PATH);
        auto fonts=std::filesystem::path(windowsDirectory)/L"Fonts";
        auto regular=fonts/L"arial.ttf",bold=fonts/L"impact.ttf";
        ImFontConfig fontConfig;fontConfig.OversampleH=3;fontConfig.OversampleV=2;
        auto& atlas=*ImGui::GetIO().Fonts;
        if(std::filesystem::exists(regular))atlas.AddFontFromFileTTF(regular.string().c_str(),15,&fontConfig);
        else atlas.AddFontDefault();
        uiTitle=std::filesystem::exists(bold)?atlas.AddFontFromFileTTF(bold.string().c_str(),34,&fontConfig):atlas.Fonts[0];
        uiHeading=std::filesystem::exists(bold)?atlas.AddFontFromFileTTF(bold.string().c_str(),20,&fontConfig):atlas.Fonts[0];
        renderer.init(window,executable/L"shaders/Effects.hlsl");
        if(smoke){validateBlendPixels(renderer);validateCustomAndSources(renderer,executable);validateCompositor(renderer);validateEditorWindowFocus(renderer,window,executable);}
        AmbientMusic music;if(!smoke){
            auto track=findAsset(executable,L"you met me at a very chinese time in my life (outsider house).mp3",{L"assets/music",L"Music"});
            if(track.empty())track=findAsset(executable,L"ambient.mp3",{L"assets/music",L"Music"});
            music.open(track);music.startRandom();
        }
        Editor editor;editor.music=&music;editor.hwnd=window;editor.root=executable;editor.runtime=&renderer;editor.image={16,9,std::vector<uint8_t>(16*9*4,0)};editor.applyPreset(0);editor.status="READY / IMPORT MEDIA";
        if(smoke){
            Editor project;project.runtime=&renderer;project.root=executable;project.applyPreset(0);auto fixture=executable/L"smoke-source.png";savePng(fixture,demoImage());project.load(fixture);project.selected=project.graph.add(Effect::Source);project.loadSource(fixture);std::ifstream shader(executable/L"shaders/godot/x010gp.gdshader");project.addShader(std::string(std::istreambuf_iterator<char>(shader),{}),"Pixelate");project.graph.find(project.selected)->parameters[0].value=75;project.addEffect(Effect::Compositor);auto comp=project.selected;project.graph.setLayerCount(comp,4);auto compNode=project.graph.find(comp);compNode->compWidth=1080;compNode->compHeight=1350;compNode->compDuration=7.5f;compNode->compFps=24;compNode->compBgR=.1f;compNode->compBgG=.2f;compNode->compBgB=.3f;compNode->layers[3].x=.25f;compNode->layers[3].rotation=45;compNode->layers[3].path={{0,0},{1,0},{1,1},{0,1}};compNode->layers[3].maskInvert=true;compNode->layers[3].maskFeather=.05f;compNode->layers[3].locked=true;std::string compError;project.graph.connect(project.graph.find(project.clips.begin()->first)->outputs[0].id,project.graph.find(comp)->inputs[3].id,compError);project.save(executable/L"smoke-multisource.flexar");project.restore(executable/L"smoke-multisource.flexar");bool custom=false;for(auto& n:project.graph.nodes)if(n.effect==Effect::Custom)custom=n.parameters[0].value==75&&!n.shaderCode.empty();bool composition=false;for(auto& n:project.graph.nodes)if(n.effect==Effect::Compositor){composition=n.layers.size()==4&&n.layers[3].x==.25f&&n.layers[3].rotation==45&&n.compWidth==1080&&n.compHeight==1350&&n.compDuration==7.5f&&n.compFps==24&&std::abs(n.compBgB-.3f)<.001f&&n.layers[3].path.size()==4&&n.layers[3].maskInvert&&n.layers[3].locked;bool linked=false;for(auto& c:project.graph.connections)if(c.to==n.inputs[3].id)linked=true;composition=composition&&linked;}if(project.clips.size()!=1||!custom||!composition)throw std::runtime_error("Multi-source shader project roundtrip failed");
            editor.addEffect(Effect::Blend);editor.save(executable/L"smoke-roundtrip.flexar");editor.restore(executable/L"smoke-roundtrip.flexar");if(editor.graph.nodes.size()!=4||editor.graph.connections.size()!=4)throw std::runtime_error("Blend project roundtrip failed");editor.image=demoImage();editor.hasMedia=true;}
        if(!smoke){ShowWindow(window,SW_SHOWDEFAULT);UpdateWindow(window);}
        auto last=std::chrono::steady_clock::now();bool quit=false;int frames=0;
        while(!quit){MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);if(msg.message==WM_QUIT)quit=true;}if(quit)break;
            music.tick();
            if(IsIconic(window)){MsgWaitForMultipleObjects(0,nullptr,FALSE,250,QS_ALLINPUT);last=std::chrono::steady_clock::now();continue;}
            if(resizeW&&resizeH){renderer.resize(resizeW,resizeH);resizeW=resizeH=0;}
            auto now=std::chrono::steady_clock::now();double dt=std::chrono::duration<double>(now-last).count();last=now;
            if(editor.playing)editor.safely([&]{editor.position+=dt;double duration=editor.playbackDuration();if(duration>0&&editor.position>duration){editor.position=std::fmod(editor.position,duration);}if(editor.video.loaded()){editor.video.seek(std::min(editor.position,std::max(0.,editor.video.duration-1./30.)));editor.uploadPending=editor.video.frame(editor.position,editor.image)||editor.uploadPending;}});
            if(editor.exportActive){try{editor.prepareExport();}catch(const std::exception& e){auto error=std::string(e.what());try{editor.finishExport(true);}catch(...){}editor.status="Export failed: "+error;}}
            renderer.begin();if(editor.uploadPending){renderer.upload(editor.image);editor.uploadPending=false;}
            editor.runtime=&renderer;editor.safely([&]{editor.syncSources(renderer);});
            ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
            editor.beginHistoryFrame();
            try{editor.ui(renderer);}catch(const std::exception& e){editor.status=e.what();/* Preserve ImGui stack by treating UI errors as fatal below. */throw;}
            editor.commitHistoryFrame();
            ImGui::Render();renderer.present();
            if(editor.exportActive){try{editor.writeExport(renderer);}catch(const std::exception& e){auto error=std::string(e.what());try{editor.finishExport(true);}catch(...){}editor.status="Export failed: "+error;}}
            if(smoke){++frames;if(frames<=PresetCount)editor.applyPreset(frames-1);else if(frames<=PresetCount+static_cast<int>(catalog().size())){editor.applyPreset(0);editor.addEffect(catalog()[frames-PresetCount-1].effect);if(editor.graph.find(editor.selected)->effect==Effect::Compositor){editor.showComposition=true;editor.compositionNode=editor.selected;}editor.largePreview=true;editor.largeFullscreen=frames%2==0;}else if(frames<=PresetCount+static_cast<int>(catalog().size())+12){editor.applyPreset(0);editor.addEffect(Effect::Blend);for(auto& node:editor.graph.nodes)if(node.effect==Effect::Blend)node.parameters[0].value=static_cast<float>(frames-PresetCount-catalog().size()-1);}else quit=true;}
        }
        renderer.wait();
    }catch(const std::exception& e){result=1;if(smoke){std::ofstream log("smoke-error.txt");log<<e.what();}else MessageBoxA(window,e.what(),"Flexar",MB_OK|MB_ICONERROR);}
    try{renderer.shutdown();}catch(...){}if(winBackend)ImGui_ImplWin32_Shutdown();if(context)ImGui::DestroyContext();if(window)DestroyWindow(window);if(mf)MFShutdown();if(com)CoUninitialize();return result;
}
