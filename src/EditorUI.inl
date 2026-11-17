    void heading(const char* text){
        ImGui::PushFont(gothicHeading);ImGui::TextColored({.86f,.82f,.76f,1},"%s",text);ImGui::PopFont();
    }
    void effectMenu(){
        for(auto& effect:catalog()){
            if(ImGui::Selectable(effect.name,false,0,{230,24}))addEffect(effect.effect);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",effect.description);
        }
    }
    void library(){
        heading("look library");ImGui::TextDisabled("TEN SIGNALS / SELECT TO REPLACE GRAPH");ImGui::Separator();
        const char* descriptions[]={"Clean correction","Violet xerography","Distressed ink print","Liquid outlines","Sepia / film grain","Thermal neon trails","Paper texture","Chromatic CRT","Posterized glow","Solarized ink"};
        for(int i=0;i<PresetCount;++i){
            if(ImGui::Selectable(PresetCodes[i],preset==i,0,{250,30}))applyPreset(i);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",descriptions[i]);
        }
    }
    void inspector(){
        heading("controls");ImGui::Separator();auto n=graph.find(selected);
        if(!n){ImGui::TextWrapped("Select a node to shape its signal.");return;}
        ImGui::TextWrapped("%s",n->name.c_str());ImGui::TextDisabled("NODE / %04llu",n->id);ImGui::Spacing();
        if(n->effect==Effect::Source){
            ImGui::TextWrapped("%s",hasMedia?mediaPath.filename().string().c_str():"No media loaded");
            if(hasMedia)ImGui::Text("%u x %u / %s",image.width,image.height,video.loaded()?"VIDEO":"IMAGE");
            if(ImGui::Button(importVideo?"Import video":"Import image",{-1,30}))safely([&]{load(dialog(hwnd,false,false,importVideo));});return;
        }
        if(n->effect==Effect::Output){ImGui::TextWrapped("Final image / video output. This thumbnail updates as you change the graph.");return;}
        ImGui::Checkbox("Enabled",&n->enabled);ImGui::Spacing();
        for(size_t index=0;index<n->parameters.size();++index){auto& p=n->parameters[index];
            ImGui::TextUnformatted(p.name);ImGui::SetNextItemWidth(-1);std::string label=std::string("##")+p.name;
            if(n->effect==Effect::Blend&&index==0){int mode=static_cast<int>(p.value);if(ImGui::Combo(label.c_str(),&mode,BlendModes,12))p.value=static_cast<float>(mode);}
            else if(n->effect==Effect::Mirror&&index==0){const char* axes[]={"Horizontal","Vertical","Both"};int axis=static_cast<int>(p.value);if(ImGui::Combo(label.c_str(),&axis,axes,3))p.value=static_cast<float>(axis);}
            else if(n->effect==Effect::Duotone&&index==0){const char* palettes[]={"Violet","Sepia","Thermal"};int palette=static_cast<int>(p.value);if(ImGui::Combo(label.c_str(),&palette,palettes,3))p.value=static_cast<float>(palette);}
            else ImGui::SliderFloat(label.c_str(),&p.value,p.min,p.max,"%.3f");
            ImGui::Spacing();
        }
        if(n->effect==Effect::Blend)ImGui::TextWrapped("A = base. B = layer. Connect any two branches. Opacity controls B; bypass returns A.");
        ImGui::Spacing();if(ImGui::Button("Remove node",{-1,28})){graph.remove(selected);selected=0;status="Node removed. Reconnect its neighbors.";}
    }
    float nodeHeight(const Node& n)const{return n.effect==Effect::Source||n.effect==Effect::Output?238.f:n.effect==Effect::Blend?166.f:140.f;}
    ImVec2 pinPosition(const Pin& p,ImVec2 origin){
        auto n=graph.find(p.node);float y=84;
        if(n->effect==Effect::Source||n->effect==Effect::Output)y=211;
        if(n->effect==Effect::Blend)y=p.direction==Direction::Output?104.f:p.id==n->inputs[0].id?84.f:126.f;
        return {origin.x+(n->x+(p.direction==Direction::Output?250:0))*zoom,origin.y+(n->y+y)*zoom};
    }
    void fit(ImVec2 area){
        if(graph.nodes.empty())return;float left=1e9f,top=1e9f,right=-1e9f,bottom=-1e9f;
        for(auto& n:graph.nodes){left=std::min(left,n.x);top=std::min(top,n.y);right=std::max(right,n.x+250);bottom=std::max(bottom,n.y+nodeHeight(n));}
        zoom=std::clamp(std::min((area.x-80)/(right-left),(area.y-100)/(bottom-top)),.3f,1.15f);
        pan={(area.x-(right-left)*zoom)*.5f-left*zoom,(area.y-(bottom-top)*zoom)*.5f-top*zoom};fitRequested=false;
    }
    void thumbnail(ImDrawList* draw,ImVec2 a,ImVec2 b,uint64_t texture,const char* empty){
        draw->AddRectFilled(a,b,IM_COL32(9,9,11,255),2);
        if(texture){
            float scale=std::min((b.x-a.x)/image.width,(b.y-a.y)/image.height);ImVec2 size{image.width*scale,image.height*scale};
            ImVec2 start{a.x+(b.x-a.x-size.x)*.5f,a.y+(b.y-a.y-size.y)*.5f};
            draw->AddImage(texture,start,{start.x+size.x,start.y+size.y});
        }else{ImVec2 size=ImGui::CalcTextSize(empty);draw->AddText({a.x+(b.x-a.x-size.x)*.5f,a.y+(b.y-a.y-size.y)*.5f},IM_COL32(132,124,120,255),empty);}
    }
    void canvas(){
        ImGui::BeginChild("Canvas",{0,0},false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImVec2 base=ImGui::GetCursorScreenPos(),area=ImGui::GetContentRegionAvail();auto draw=ImGui::GetWindowDrawList();
        if(fitRequested)fit(area);
        ImGui::InvisibleButton("Graph surface",area,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle|ImGuiButtonFlags_MouseButtonRight);
        bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
        canvasHovered=hovered;
        bool space=ImGui::IsKeyDown(ImGuiKey_Space)&&!io.WantTextInput;
        if(hovered&&space&&ImGui::IsMouseClicked(0)){spacePanning=true;dragNode=0;dragPin=0;}
        if(spacePanning&&ImGui::IsMouseDown(0)){pan.x+=io.MouseDelta.x;pan.y+=io.MouseDelta.y;}
        if(!ImGui::IsMouseDown(0))spacePanning=false;
        if(hovered&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle)){pan.x+=io.MouseDelta.x;pan.y+=io.MouseDelta.y;}
        if(hovered&&io.MouseWheel!=0){float next=std::clamp(zoom+io.MouseWheel*.08f,.25f,1.75f);ImVec2 mouse{io.MousePos.x-base.x,io.MousePos.y-base.y};pan={mouse.x-(mouse.x-pan.x)*next/zoom,mouse.y-(mouse.y-pan.y)*next/zoom};zoom=next;}
        ImVec2 origin{base.x+pan.x,base.y+pan.y};
        graphBackdrop(draw,base,area);
        auto wire=[&](ImVec2 a,ImVec2 b,ImU32 color){float bend=std::max(65.f,std::abs(b.x-a.x)*.45f);draw->AddBezierCubic(a,{a.x+bend,a.y},{b.x-bend,b.y},b,IM_COL32(45,17,24,255),6*zoom);draw->AddBezierCubic(a,{a.x+bend,a.y},{b.x-bend,b.y},b,color,2*zoom);};
        for(auto& c:graph.connections)wire(pinPosition(*graph.pin(c.from),origin),pinPosition(*graph.pin(c.to),origin),IM_COL32(185,91,109,255));
        bool hitPin=false,hitNode=false;
        for(auto& n:graph.nodes){
            ImVec2 a{origin.x+n.x*zoom,origin.y+n.y*zoom},b{a.x+250*zoom,a.y+nodeHeight(n)*zoom};bool chosen=n.id==selected;
            draw->AddRectFilled({a.x+5,a.y+7},{b.x+5,b.y+7},IM_COL32(0,0,0,90),3);
            draw->AddRectFilled(a,b,IM_COL32(24,21,25,255),3);draw->AddRect(a,b,chosen?IM_COL32(198,115,128,255):IM_COL32(80,62,69,255),3,0,chosen?2.f:1.f);
            draw->AddRectFilled(a,{b.x,a.y+42*zoom},IM_COL32(40,26,33,255),3);
            draw->AddLine({a.x,a.y+42*zoom},{b.x,a.y+42*zoom},IM_COL32(94,56,67,255));
            draw->AddText(gothicHeading,19*zoom,{a.x+14*zoom,a.y+12*zoom},IM_COL32(226,215,198,255),n.name.c_str());
            bool endpoint=n.effect==Effect::Source||n.effect==Effect::Output;
            if(endpoint){thumbnail(draw,{a.x+12*zoom,a.y+53*zoom},{b.x-12*zoom,a.y+188*zoom},n.effect==Effect::Source?sourceTexture:outputTexture,hasMedia?"disconnected":"import media");
                const char* label=hasMedia?(video.loaded()?"VIDEO / RGBA":"IMAGE / RGBA"):"NO SIGNAL";
                draw->AddText(nullptr,12*zoom,{a.x+18*zoom,a.y+206*zoom},IM_COL32(167,154,150,255),label);
            }else if(n.effect==Effect::Blend){
                draw->AddText(nullptr,14*zoom,{a.x+18*zoom,a.y+76*zoom},IM_COL32(202,183,176,255),"A / base");
                draw->AddText(nullptr,14*zoom,{a.x+18*zoom,a.y+118*zoom},IM_COL32(202,183,176,255),"B / layer");
                draw->AddText(nullptr,13*zoom,{a.x+120*zoom,a.y+96*zoom},IM_COL32(180,111,126,255),BlendModes[std::clamp(static_cast<int>(n.parameters[0].value),0,11)]);
            }else{
                draw->AddText(nullptr,14*zoom,{a.x+18*zoom,a.y+76*zoom},IM_COL32(170,151,153,255),n.enabled?"in                       out":"bypassed");
                if(!n.parameters.empty()){char text[80];snprintf(text,sizeof(text),"%s  %.2f",n.parameters[0].name,n.parameters[0].value);draw->AddText(nullptr,12*zoom,{a.x+18*zoom,a.y+111*zoom},IM_COL32(122,109,114,255),text);}
            }
            auto handlePin=[&](const Pin& p){auto pos=pinPosition(p,origin);float dx=io.MousePos.x-pos.x,dy=io.MousePos.y-pos.y;bool hit=hovered&&dx*dx+dy*dy<144;
                draw->AddRectFilled({pos.x-6*zoom,pos.y-6*zoom},{pos.x+6*zoom,pos.y+6*zoom},IM_COL32(23,18,21,255));draw->AddRect({pos.x-6*zoom,pos.y-6*zoom},{pos.x+6*zoom,pos.y+6*zoom},hit?IM_COL32(255,231,216,255):IM_COL32(203,113,131,255),0,0,2);
                if(hit&&!space&&!spacePanning){hitPin=true;if(ImGui::IsMouseClicked(0)&&p.direction==Direction::Output){dragPin=p.id;dragNode=0;}
                    if(ImGui::IsMouseReleased(0)&&dragPin&&p.direction==Direction::Input){std::string error;status=graph.connect(dragPin,p.id,error)?"Connection created":error;dragPin=0;}
                    if(ImGui::IsMouseClicked(1)&&p.direction==Direction::Input){graph.disconnect(p.id);status="Input disconnected";}
                }
            };
            for(auto& p:n.inputs)handlePin(p);for(auto& p:n.outputs)handlePin(p);
            bool inside=hovered&&io.MousePos.x>a.x+12&&io.MousePos.x<b.x-12&&io.MousePos.y>a.y&&io.MousePos.y<b.y;
            if(inside&&!space&&!spacePanning){hitNode=true;if(ImGui::IsMouseClicked(0)&&!hitPin){selected=n.id;dragNode=n.id;showControls=true;}
                if(ImGui::IsMouseDoubleClicked(0)){selected=n.id;dragNode=0;showControls=true;focusControls=true;}}
        }
        if(dragNode&&!dragPin&&!spacePanning&&!space&&ImGui::IsMouseDragging(0)){if(auto n=graph.find(dragNode)){n->x+=io.MouseDelta.x/zoom;n->y+=io.MouseDelta.y/zoom;}}
        if(dragPin)if(auto pin=graph.pin(dragPin))wire(pinPosition(*pin,origin),io.MousePos,IM_COL32(246,208,216,255));
        if(ImGui::IsMouseReleased(0)){dragPin=0;dragNode=0;}
        if(hovered&&!hitPin&&!hitNode&&!space&&!spacePanning&&ImGui::IsMouseClicked(0)){selected=0;showControls=false;}
        if(hovered&&!hitPin&&ImGui::IsMouseClicked(1))ImGui::OpenPopup("Add node at canvas");
        if(ImGui::BeginPopup("Add node at canvas")){heading("add node");effectMenu();ImGui::EndPopup();}
        draw->AddText({base.x+14,base.y+14},IM_COL32(115,94,102,255),"GRAPH / DRAG TO CONNECT");
        draw->AddText({base.x+14,base.y+area.y-26},IM_COL32(115,94,102,255),"SPACE + drag / MMB pan    /    wheel zoom    /    double-click node: controls");
        ImGui::EndChild();
    }
    void controlsWindow(){
        if(!showControls||!graph.find(selected))return;
        auto vp=ImGui::GetMainViewport();
        ImGui::SetNextWindowSize({340,480},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x-365,vp->Pos.y+390},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({280,220},{vp->Size.x,vp->Size.y});
        if(focusControls){ImGui::SetNextWindowFocus();ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x-365,vp->Pos.y+std::min(390.f,std::max(60.f,vp->Size.y-500))});focusControls=false;}
        if(ImGui::Begin("Node Controls",&showControls,ImGuiWindowFlags_NoCollapse)){
            ImGui::BeginDisabled(exportActive);inspector();ImGui::EndDisabled();
        }ImGui::End();
    }
    void previewWindow(){
        if(!largePreview)return;
        auto viewport=ImGui::GetMainViewport();
        if(largeFullscreen){ImGui::SetNextWindowPos(viewport->Pos);ImGui::SetNextWindowSize(viewport->Size);}
        else{ImGui::SetNextWindowSize({std::min(1000.f,viewport->Size.x-60),std::min(700.f,viewport->Size.y-60)},ImGuiCond_Appearing);ImGui::SetNextWindowPos({viewport->Pos.x+viewport->Size.x*.5f,viewport->Pos.y+viewport->Size.y*.5f},ImGuiCond_Appearing,{.5f,.5f});}
        ImGui::SetNextWindowSizeConstraints({480,340},{viewport->Size.x,viewport->Size.y});
        ImGuiWindowFlags flags=ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse;
        if(largeFullscreen)flags|=ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize;
        if(ImGui::Begin("Preview player",&largePreview,flags)){
            ImGui::Checkbox("Original##player",&original);ImGui::SameLine();
            if(ImGui::Button(largeFullscreen?"Windowed":"Full screen"))largeFullscreen=!largeFullscreen;
            ImGui::SameLine();ImGui::TextDisabled("%s",video.loaded()?"VIDEO":"IMAGE");
            ImVec2 a=ImGui::GetCursorScreenPos(),room=ImGui::GetContentRegionAvail();float h=std::max(40.f,room.y-80);
            thumbnail(ImGui::GetWindowDrawList(),a,{a.x+room.x,a.y+h},original?sourceTexture:outputTexture,hasMedia?"output disconnected":"import image or video");ImGui::Dummy({room.x,h});
            ImGui::BeginDisabled(!hasMedia||exportActive);
            if(ImGui::Button(playing?"Pause":"Resume",{90,30}))playing=!playing;ImGui::SameLine();
            if(ImGui::Button("Restart",{90,30}))safely([&]{position=0;video.seek(0);if(video.loaded())uploadPending=video.frame(0,image)||uploadPending;});
            float seconds=static_cast<float>(position),duration=static_cast<float>(video.loaded()?video.duration:10.);ImGui::SetNextItemWidth(-1);
            if(ImGui::SliderFloat("##Player timeline",&seconds,0,std::max(.01f,duration),"%.2f s"))safely([&]{position=seconds;if(video.loaded()){video.seek(position);uploadPending=video.frame(position,image)||uploadPending;}});
            ImGui::EndDisabled();if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape))largePreview=false;
        }ImGui::End();
    }
    void ui(Renderer& renderer){
        auto viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos(viewport->Pos);ImGui::SetNextWindowSize(viewport->Size);
        ImGui::Begin("flexar",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        ImGui::BeginChild("Navbar",{0,62},false,ImGuiWindowFlags_NoScrollbar);
        float rowY=ImGui::GetCursorPosY();
        ImGui::PushFont(gothicTitle);ImGui::TextColored({.91f,.85f,.76f,1},"flexar");ImGui::PopFont();
        ImGui::SetCursorPos({160,rowY+8});ImGui::BeginGroup();
        ImGui::BeginDisabled(exportActive);
        if(ImGui::Button("Looks",{80,34}))ImGui::OpenPopup("Looks");
        if(ImGui::BeginPopup("Looks")){library();ImGui::EndPopup();}
        ImGui::SameLine(0,8);if(ImGui::Button("+ Node",{80,34}))ImGui::OpenPopup("Effects");
        if(ImGui::BeginPopup("Effects")){heading("effects");effectMenu();ImGui::EndPopup();}
        ImGui::SameLine();if(ImGui::Button("Open",{70,34}))safely([&]{restore(dialog(hwnd,false,true));});
        ImGui::SameLine();if(ImGui::Button("Save",{70,34}))safely([&]{save(dialog(hwnd,true,true));});
        ImGui::SameLine();if(ImGui::Button("Fit",{50,34}))fitRequested=true;
        ImGui::SameLine();if(ImGui::Button("Arrange",{80,34}))arrange();
        ImGui::SameLine();if(ImGui::Button(showPreview?"Preview ON":"Preview OFF",{112,34}))showPreview=!showPreview;
        ImGui::SameLine();ImGui::BeginDisabled(!hasMedia||!outputTexture);
        if(ImGui::Button("EXPORT",{90,34}))ImGui::OpenPopup("Export");
        if(ImGui::BeginPopup("Export")){
            heading("export");
            if(ImGui::MenuItem("PNG / current output"))safely([&]{startExport(false);});
            ImGui::BeginDisabled(!video.loaded());
            if(ImGui::MenuItem("MP4 / full clip / 30 FPS"))safely([&]{startExport(true);});
            ImGui::EndDisabled();ImGui::TextDisabled("MP4: silent H.264 / alpha over black");ImGui::EndPopup();
        }
        ImGui::EndDisabled();ImGui::EndDisabled();ImGui::EndGroup();ImGui::EndChild();ImGui::Separator();
        if(exportActive){ImGui::ProgressBar(float(exportFrame)/exportFrames,{ImGui::GetContentRegionAvail().x-100,24},"EXPORTING");ImGui::SameLine();if(ImGui::Button("Cancel",{80,24}))cancelExport=true;}
        ImGui::BeginDisabled(exportActive);
        // Space is reserved for canvas panning. Playback has explicit buttons.
        ImGui::BeginChild("Transport",{0,48},false,ImGuiWindowFlags_NoScrollbar);
        if(ImGui::Selectable("IMAGE",!importVideo,0,{65,28}))importVideo=false;ImGui::SameLine();
        if(ImGui::Selectable("VIDEO",importVideo,0,{65,28}))importVideo=true;ImGui::SameLine();
        if(ImGui::Button(importVideo?"Import video":"Import image",{120,28}))safely([&]{load(dialog(hwnd,false,false,importVideo));});ImGui::SameLine();
        ImGui::BeginDisabled(!hasMedia);
        if(ImGui::Button(playing?"Pause":"Play",{60,28}))playing=!playing;ImGui::SameLine();
        if(ImGui::Button("|<",{32,28}))safely([&]{position=0;video.seek(0);if(video.loaded())uploadPending=video.frame(0,image)||uploadPending;});ImGui::SameLine();
        float seconds=static_cast<float>(position),duration=static_cast<float>(video.loaded()?video.duration:10.);ImGui::SetNextItemWidth(std::max(120.f,ImGui::GetContentRegionAvail().x-135));
        if(ImGui::SliderFloat("##Playhead",&seconds,0,std::max(.01f,duration),"%.2f s"))safely([&]{position=seconds;if(video.loaded()){video.seek(position);uploadPending=video.frame(position,image)||uploadPending;}});
        ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("%3.0f FPS",ImGui::GetIO().Framerate);ImGui::EndChild();
        sourceTexture=hasMedia&&!uploadPending?renderer.sourceHandle():0;
        uint64_t evaluated=renderer.evaluate(graph,static_cast<float>(position),false);outputTexture=hasMedia&&!uploadPending?evaluated:0;
        float width=ImGui::GetContentRegionAvail().x,height=ImGui::GetContentRegionAvail().y-26;
        bool sidebar=showPreview;
        float side=sidebar?320.f:0;
        ImGui::BeginChild("Graph pane",{sidebar?width-side-8:width,height},true);canvas();ImGui::EndChild();
        if(sidebar){
            ImGui::SameLine();ImGui::BeginChild("Side",{0,height},false);
            float previewHeight=310.f;
            if(showPreview){
            ImGui::BeginChild("Preview panel",{0,previewHeight},true);
            heading("preview");ImGui::SameLine();ImGui::Checkbox("Original",&original);ImGui::Separator();
            ImVec2 a=ImGui::GetCursorScreenPos();float w=ImGui::GetContentRegionAvail().x,h=std::max(40.f,ImGui::GetContentRegionAvail().y-42);
            thumbnail(ImGui::GetWindowDrawList(),a,{a.x+w,a.y+h},original?sourceTexture:outputTexture,hasMedia?"output disconnected":"import image or video");
            ImGui::InvisibleButton("Expand preview image",{w,h});if(ImGui::IsItemClicked())largePreview=true;
            if(ImGui::Button("Expand / Play",{-1,28}))largePreview=true;
            ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::EndDisabled();ImGui::TextDisabled("%s",status.c_str());ImGui::End();controlsWindow();previewWindow();crosshair();
    }
