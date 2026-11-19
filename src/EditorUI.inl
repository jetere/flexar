    void heading(const char* text){
        ImVec2 p=ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p,{p.x+4,p.y+18},accentU32);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+12);
        ImGui::PushFont(uiHeading);ImGui::TextColored(ink,"%s",text);ImGui::PopFont();
    }
    const char* effectGroup(Effect e)const{
        switch(e){
        case Effect::Source:case Effect::Output:case Effect::Blend:case Effect::Compositor:case Effect::LumaKey:return "Compositing";
        case Effect::Bloom:case Effect::GlowRays:case Effect::Streaks:case Effect::Prism:case Effect::LensWarp:case Effect::RadialChromatic:case Effect::EdgeGlow:case Effect::DropShadow:return "Optics & light";
        case Effect::Liquid:case Effect::Neon:case Effect::Shake:case Effect::Ripple:case Effect::RadialBlur:case Effect::Mirror:case Effect::Chromatic:return "Distortion & motion";
        case Effect::Halftone:case Effect::Xerox:case Effect::Grain:case Effect::Paper:case Effect::Scanlines:case Effect::Pixelate:case Effect::GridPixel:case Effect::ColorSteps:case Effect::Flicker:return "Print & texture";
        default:return "Color & detail";
        }
    }
    void effectMenu(){
        ImGui::PushStyleColor(ImGuiCol_Header,accent);
        if(ImGui::Selectable("+ Custom Shader",true,0,{280,26})){safely([&]{addShader("shader_type canvas_item;\nuniform float strength : hint_range(0, 1) = 0.5;\nvoid fragment(){ COLOR = texture(TEXTURE, UV); COLOR.rgb = mix(COLOR.rgb, vec3(1.0)-COLOR.rgb, strength); }","Custom Shader");});ImGui::CloseCurrentPopup();}
        ImGui::PopStyleColor();static char search[80]{};ImGui::SetNextItemWidth(280);ImGui::InputTextWithHint("##NodeSearch","Search nodes",search,sizeof(search));
        auto matches=[&](const char* name){std::string text=name,query=search;std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return text.find(query)!=std::string::npos;};
        ImGui::BeginChild("Node list",{290,380},false);
        for(const char* group:{"Compositing","Artistic","Optics & light","Distortion & motion","Print & texture","Color & detail"}){
            bool header=false;auto section=[&]{if(!header){ImGui::SeparatorText(group);header=true;}};
            if(std::string(group)=="Compositing"&&matches("Source Clip")){section();if(ImGui::Selectable("+ Source Clip",false,0,{0,22})){if(graph.nodes.size()<32){selected=graph.add(Effect::Source,100,250);openControls(selected);}ImGui::CloseCurrentPopup();}}
            if(std::string(group)=="Artistic"||std::string(group)=="Optics & light"){
                for(const char* name:{"Liquid Chrome","Acid Print","Neon Melt","Glitch Collage","Lens Ghosts","Anamorphic Flare","Diffraction Star","Prismatic Glass","Flash Bloom"}){
                    bool artistic=std::string(name)=="Liquid Chrome"||std::string(name)=="Acid Print"||std::string(name)=="Neon Melt"||std::string(name)=="Glitch Collage";
                    if(artistic!=(std::string(group)=="Artistic")||!matches(name))continue;section();
                    if(ImGui::Selectable(name,false,0,{0,22})){safely([&]{std::ifstream in(root/"shaders/godot"/(std::string(name)+".gdshader"));if(!in)throw std::runtime_error("Missing effect shader");addShader(std::string(std::istreambuf_iterator<char>(in),{}),name);});ImGui::CloseCurrentPopup();}
                }
            }
            for(auto& effect:catalog())if(std::string(effectGroup(effect.effect))==group&&matches(effect.name)){section();if(ImGui::Selectable(effect.name,false,0,{0,22})){addEffect(effect.effect);ImGui::CloseCurrentPopup();}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",effect.description);}
        }ImGui::EndChild();
    }
    void library(){
        heading("LOOKS");ImGui::BeginChild("Look list",{270,360},false);
        for(int i=0;i<PresetCount;++i){if(ImGui::Selectable(PresetCodes[i],preset==i,0,{0,24})){safely([&]{applyPreset(i);});ImGui::CloseCurrentPopup();}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",PresetNames[i]);}
        ImGui::EndChild();
    }
    void inspector(){
        heading("CONTROLS");ImGui::Separator();auto n=graph.find(selected);
        if(!n){ImGui::TextWrapped("Select a node to shape its signal.");return;}
        ImGui::TextWrapped("%s",n->name.c_str());ImGui::Spacing();
        if(n->effect==Effect::Source){
            auto extra=clips.find(n->id);bool primary=n->id==primarySource();ImGui::TextWrapped("%s",primary?(hasMedia?mediaPath.filename().string().c_str():"No media loaded"):(extra!=clips.end()?extra->second.path.filename().string().c_str():"No media loaded"));
            if(!primary&&ImGui::Button("Remove source")){removeSelectedNode();return;}
            if(primary&&hasMedia)ImGui::Text("%u x %u / %s",image.width,image.height,video.loaded()?"VIDEO":"IMAGE");
            if(!primary&&extra!=clips.end())ImGui::Text("%u x %u / %s",extra->second.image.width,extra->second.image.height,extra->second.video.loaded()?"VIDEO":"IMAGE");
            if(ImGui::Button(importVideo?"Import video":"Import image",{-1,30}))safely([&]{loadSource(dialog(hwnd,false,false,importVideo));});return;
        }
        if(n->effect==Effect::Output){if(ImGui::Button("Open preview",{-1,30}))openPreview();if(ImGui::Button("Export PNG",{-1,30}))safely([&]{startExport(false);});ImGui::BeginDisabled(!video.loaded());if(ImGui::Button("Export MP4",{-1,30}))safely([&]{startExport(true);});ImGui::EndDisabled();return;}
        if(n->effect==Effect::Compositor){ImGui::Checkbox("Enabled",&n->enabled);Id id=n->id;compositionSetup(*n);n=graph.find(id);if(ImGui::Button("Open composition studio",{-1,32})){openComposition(id);}layerControls(*n);return;}
        if(n->effect==Effect::Custom&&(n->name=="Custom Shader"||n->name=="Godot Shader")){if(editingShader!=n->id){editingShader=n->id;strncpy_s(shaderBuffer,n->shaderCode.c_str(),_TRUNCATE);}if(ImGui::Button("Edit shader"))strncpy_s(shaderBuffer,n->shaderCode.c_str(),_TRUNCATE);ImGui::InputTextMultiline("##Shader",shaderBuffer,sizeof(shaderBuffer),{-1,180});if(ImGui::Button("Compile / Apply"))safely([&]{Node candidate=*n;candidate.shaderCode=shaderBuffer;runtime->compileShader(candidate);*n=std::move(candidate);status="Shader compiled";});}
        ImGui::Checkbox("Enabled",&n->enabled);ImGui::SameLine();if(ImGui::Button("Reset all"))safely([&]{if(n->effect==Effect::Custom)runtime->compileShader(*n);else{Graph defaults;auto id=defaults.add(n->effect);n->parameters=defaults.find(id)->parameters;}});
        Graph defaultGraph;auto defaultId=defaultGraph.add(n->effect);auto defaults=defaultGraph.find(defaultId);ImGui::SeparatorText("Parameters");ImGui::Spacing();
        for(size_t index=0;index<n->parameters.size();++index){auto& p=n->parameters[index];
            ImGui::TextUnformatted(p.name.c_str());ImGui::SetNextItemWidth(-1);std::string label=std::string("##")+p.name;
            if(n->effect==Effect::Blend&&index==0){int mode=static_cast<int>(p.value);if(ImGui::Combo(label.c_str(),&mode,BlendModes,12))p.value=static_cast<float>(mode);}
            else if(n->effect==Effect::Mirror&&index==0){const char* axes[]={"Horizontal","Vertical","Both"};int axis=static_cast<int>(p.value);if(ImGui::Combo(label.c_str(),&axis,axes,3))p.value=static_cast<float>(axis);}
            else if(n->effect==Effect::Duotone&&index==0){const char* palettes[]={"Violet","Sepia","Thermal"};int palette=static_cast<int>(p.value);if(ImGui::Combo(label.c_str(),&palette,palettes,3))p.value=static_cast<float>(palette);}
            else {ImGui::SliderFloat(label.c_str(),&p.value,p.min,p.max,"%.3f",ImGuiSliderFlags_AlwaysClamp);ImGui::SetNextItemWidth(-55);ImGui::DragFloat((label+"Exact").c_str(),&p.value,.001f,p.min,p.max,"%.5f",ImGuiSliderFlags_AlwaysClamp);if(index<defaults->parameters.size()){ImGui::SameLine();if(ImGui::SmallButton(("R"+label).c_str()))p.value=defaults->parameters[index].value;}}
            ImGui::Spacing();
        }
        if(ImGui::CollapsingHeader("Connections")){for(size_t i=0;i<n->inputs.size();++i){Id upstream=0;for(auto& c:graph.connections)if(c.to==n->inputs[i].id)upstream=graph.pin(c.from)->node;ImGui::Text("Input %u: %s",unsigned(i+1),upstream?graph.find(upstream)->name.c_str():"Disconnected");}ImGui::Text("Outputs: %u",unsigned(n->outputs.size()));}
        ImGui::Spacing();if(ImGui::Button("Remove node",{-1,28})){removeSelectedNode();}
    }
    float nodeHeight(const Node& n)const{return n.effect==Effect::Source||n.effect==Effect::Output?190.f:n.effect==Effect::Compositor?94.f+30.f*float(n.inputs.size()):n.effect==Effect::Blend?146.f:116.f;}
    ImVec2 pinPosition(const Pin& p,ImVec2 origin){
        auto n=graph.find(p.node);float y=68;
        if(n->effect==Effect::Source||n->effect==Effect::Output)y=174;
        if(n->effect==Effect::Compositor){y=64;for(size_t i=0;i<n->inputs.size();++i)if(n->inputs[i].id==p.id)y=64+30*float(i);}
        if(n->effect==Effect::Blend)y=p.direction==Direction::Output?90.f:p.id==n->inputs[0].id?68.f:110.f;
        return {origin.x+(n->x+(p.direction==Direction::Output?220:0))*zoom,origin.y+(n->y+y)*zoom};
    }
    // Pin-aligned node labels. Local Y values match pinPosition so labels sit on
    // the wire rows instead of being spaced by hand with literal spaces.
    void nodeLabel(ImDrawList* draw,ImVec2 a,ImVec2 b,const char* text,float localY,int align,float viewScale,ImU32 color)const{
        float size=14*viewScale,width=220*viewScale;
        auto measured=ImGui::GetFont()->CalcTextSizeA(size,100000.f,0.f,text);
        float x=align==1?b.x-16*viewScale-measured.x:align==2?a.x+(width-measured.x)*.5f:a.x+16*viewScale;
        draw->AddText(ImGui::GetFont(),size,{x,a.y+(localY-measured.y*.5f)*viewScale},color,text);
    }
    void fit(ImVec2 area){
        if(graph.nodes.empty())return;float left=1e9f,top=1e9f,right=-1e9f,bottom=-1e9f;
        for(auto& n:graph.nodes){left=std::min(left,n.x);top=std::min(top,n.y);right=std::max(right,n.x+220);bottom=std::max(bottom,n.y+nodeHeight(n));}
        zoom=std::clamp(std::min((area.x-80)/(right-left),(area.y-100)/(bottom-top)),.3f,1.15f);
        pan={(area.x-(right-left)*zoom)*.5f-left*zoom,(area.y-(bottom-top)*zoom)*.5f-top*zoom};fitRequested=false;
    }
    void thumbnail(ImDrawList* draw,ImVec2 a,ImVec2 b,uint64_t texture,const char*){
        draw->PushClipRect(a,b,true);draw->AddRectFilled(a,b,IM_COL32(8,9,9,255));
        if(texture){
            float scale=std::min((b.x-a.x)/image.width,(b.y-a.y)/image.height);ImVec2 size{image.width*scale,image.height*scale};
            ImVec2 start{a.x+(b.x-a.x-size.x)*.5f,a.y+(b.y-a.y-size.y)*.5f};
            draw->AddImage(texture,start,{start.x+size.x,start.y+size.y});
        }draw->PopClipRect();
    }
    void canvas(){
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,0.f));
        ImGui::BeginChild("Canvas",{0,0},false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImVec2 base=ImGui::GetCursorScreenPos(),area=ImGui::GetContentRegionAvail();auto draw=ImGui::GetWindowDrawList();
        if(fitRequested)fit(area);
        ImGui::InvisibleButton("Graph surface",area,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle|ImGuiButtonFlags_MouseButtonRight);
        bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
        canvasHovered=hovered;nodeDeletionFocus|=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        bool space=ImGui::IsKeyDown(ImGuiKey_Space)&&!io.WantTextInput;
        if(hovered&&space&&ImGui::IsMouseClicked(0)){spacePanning=true;dragNode=0;dragPin=0;}
        if(spacePanning&&ImGui::IsMouseDown(0)){pan.x+=io.MouseDelta.x;pan.y+=io.MouseDelta.y;}
        if(!ImGui::IsMouseDown(0))spacePanning=false;
        if(hovered&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle)){pan.x+=io.MouseDelta.x;pan.y+=io.MouseDelta.y;}
        if(hovered&&io.MouseWheel!=0){float next=std::clamp(zoom+io.MouseWheel*.08f,.25f,1.75f);ImVec2 mouse{io.MousePos.x-base.x,io.MousePos.y-base.y};pan={mouse.x-(mouse.x-pan.x)*next/zoom,mouse.y-(mouse.y-pan.y)*next/zoom};zoom=next;}
        ImVec2 origin{base.x+pan.x,base.y+pan.y};
        graphBackdrop(draw,base,area);
        auto wire=[&](ImVec2 a,ImVec2 b,ImU32 color){float bend=std::max(65.f,std::abs(b.x-a.x)*.45f);draw->AddBezierCubic(a,{a.x+bend,a.y},{b.x-bend,b.y},b,IM_COL32(5,6,6,230),4*zoom);draw->AddBezierCubic(a,{a.x+bend,a.y},{b.x-bend,b.y},b,color,2*zoom);};
        for(auto& c:graph.connections)wire(pinPosition(*graph.pin(c.from),origin),pinPosition(*graph.pin(c.to),origin),accentU32);
        bool hitPin=false,hitNode=false;
        for(auto& n:graph.nodes){
            ImVec2 a{origin.x+n.x*zoom,origin.y+n.y*zoom},b{a.x+220*zoom,a.y+nodeHeight(n)*zoom};bool chosen=n.id==selected;
            draw->AddRectFilled({a.x+5,a.y+6},{b.x+5,b.y+6},IM_COL32(0,0,0,80));
            draw->AddRectFilled(a,b,IM_COL32(17,19,18,255));draw->AddRect(a,b,chosen?accentU32:IM_COL32(58,64,61,255),0,0,chosen?3.f:1.f);
            draw->AddRectFilled(a,{b.x,a.y+32*zoom},IM_COL32(10,12,11,255));
            draw->AddLine({a.x,a.y+32*zoom},{b.x,a.y+32*zoom},IM_COL32(40,44,42,255));
            draw->PushClipRect(a,{b.x-8*zoom,a.y+32*zoom},true);
            draw->AddText(uiHeading,15*zoom,{a.x+12*zoom,a.y+9*zoom},IM_COL32(236,232,223,255),n.name.c_str());draw->PopClipRect();
            draw->AddRectFilled(a,{b.x,a.y+3*zoom},chosen?accentU32:IM_COL32(90,96,92,255));
            draw->PushClipRect({a.x+8*zoom,a.y+33*zoom},{b.x-8*zoom,b.y-4*zoom},true);
            bool endpoint=n.effect==Effect::Source||n.effect==Effect::Output;
            if(endpoint){thumbnail(draw,{a.x+12*zoom,a.y+41*zoom},{b.x-12*zoom,a.y+152*zoom},n.effect==Effect::Source?(thumbnails.contains(n.id)?thumbnails.at(n.id):0):outputTexture,hasMedia?"disconnected":"import media");
            }else if(n.effect==Effect::Compositor){
                for(size_t i=0;i<n.inputs.size();++i){std::string label="Layer "+std::to_string(i+1);nodeLabel(draw,a,b,label.c_str(),64+30.f*float(i),0,zoom,IM_COL32(218,215,206,255));}
                nodeLabel(draw,a,b,"out",64,1,zoom,IM_COL32(218,215,206,255));
            }else if(n.effect==Effect::Blend){
                nodeLabel(draw,a,b,"A / base",68,0,zoom,IM_COL32(218,215,206,255));
                nodeLabel(draw,a,b,"B / layer",110,0,zoom,IM_COL32(218,215,206,255));
                nodeLabel(draw,a,b,"out",90,1,zoom,IM_COL32(218,215,206,255));
                nodeLabel(draw,a,b,BlendModes[std::clamp(static_cast<int>(n.parameters[0].value),0,11)],130,2,zoom,IM_COL32(138,143,139,255));
            }else{
                nodeLabel(draw,a,b,n.enabled?"in":"off",68,0,zoom,IM_COL32(218,215,206,255));
                nodeLabel(draw,a,b,"out",68,1,zoom,IM_COL32(218,215,206,255));
                if(!n.parameters.empty()){char text[80];snprintf(text,sizeof(text),"%s  %.2f",n.parameters[0].name.c_str(),n.parameters[0].value);draw->AddText(nullptr,12*zoom,{a.x+16*zoom,a.y+90*zoom},IM_COL32(138,143,139,255),text);}
                if(!n.enabled)nodeLabel(draw,a,b,"bypassed",113,2,zoom,IM_COL32(138,143,139,255));
            }
            draw->PopClipRect();
            auto handlePin=[&](const Pin& p){auto pos=pinPosition(p,origin);float dx=io.MousePos.x-pos.x,dy=io.MousePos.y-pos.y;bool hit=hovered&&dx*dx+dy*dy<144;
                draw->AddRectFilled({pos.x-6*zoom,pos.y-6*zoom},{pos.x+6*zoom,pos.y+6*zoom},IM_COL32(16,18,17,255));draw->AddRect({pos.x-6*zoom,pos.y-6*zoom},{pos.x+6*zoom,pos.y+6*zoom},hit?accentU32:IM_COL32(160,166,161,255),0,0,2);
                if(hit&&!space&&!spacePanning){hitPin=true;if(ImGui::IsMouseClicked(0)&&p.direction==Direction::Output){dragPin=p.id;dragNode=0;}
                    if(ImGui::IsMouseReleased(0)&&dragPin&&p.direction==Direction::Input){std::string error;status=graph.connect(dragPin,p.id,error)?"Connection created":error;dragPin=0;}
                    if(ImGui::IsMouseClicked(1)&&p.direction==Direction::Input){graph.disconnect(p.id);status="Input disconnected";}
                }
            };
            for(auto& p:n.inputs)handlePin(p);for(auto& p:n.outputs)handlePin(p);
            bool inside=hovered&&io.MousePos.x>a.x+12&&io.MousePos.x<b.x-12&&io.MousePos.y>a.y&&io.MousePos.y<b.y;
            if(inside&&!space&&!spacePanning){hitNode=true;
                if(ImGui::IsMouseDoubleClicked(0)){
                    dragNode=0;
                    if(n.effect==Effect::Source){selected=n.id;safely([&]{auto path=dialog(hwnd,false);if(!path.empty())loadSource(path);});}
                    else{openControls(n.id,true);if(n.effect==Effect::Compositor)openComposition(n.id);}
                }else if(ImGui::IsMouseClicked(0)&&!hitPin){openControls(n.id);dragNode=n.id;}}
        }
        if(dragNode&&!dragPin&&!spacePanning&&!space&&ImGui::IsMouseDragging(0)){if(auto n=graph.find(dragNode)){n->x+=io.MouseDelta.x/zoom;n->y+=io.MouseDelta.y/zoom;}}
        if(dragPin)if(auto pin=graph.pin(dragPin))wire(pinPosition(*pin,origin),io.MousePos,accentU32);
        if(ImGui::IsMouseReleased(0)){dragPin=0;dragNode=0;}
        if(hovered&&!hitPin&&!hitNode&&!space&&!spacePanning&&ImGui::IsMouseClicked(0)){selected=0;showControls=false;}
        if(hovered&&!hitPin&&ImGui::IsMouseClicked(1))ImGui::OpenPopup("Add node at canvas");
        if(ImGui::BeginPopup("Add node at canvas")){heading("ADD NODE");effectMenu();ImGui::EndPopup();}
        ImGui::EndChild();
    }
    void controlSizeLimits(const Node& node,ImVec2& minimum,ImVec2& maximum)const{
        float height=std::clamp(190.f+float(node.parameters.size())*72.f,290.f,600.f);
        minimum={290,std::min(300.f,height-30)};maximum={370,height};
        if(node.effect==Effect::Source){minimum={290,210};maximum={350,330};}
        else if(node.effect==Effect::Output){minimum={280,190};maximum={340,260};}
        else if(node.effect==Effect::Compositor){minimum={310,380};maximum={400,650};}
        else if(node.effect==Effect::Custom&&(node.name=="Custom Shader"||node.name=="Godot Shader")){minimum={340,400};maximum={520,680};}
        auto viewport=ImGui::GetMainViewport();maximum.x=std::min(maximum.x,viewport->Size.x-24);maximum.y=std::min(maximum.y,viewport->Size.y-32);minimum.x=std::min(minimum.x,maximum.x);minimum.y=std::min(minimum.y,maximum.y);
    }
    void controlsWindow(){
        if(!showControls||!graph.find(selected))return;
        auto vp=ImGui::GetMainViewport();
        ImVec2 minimum,maximum;controlSizeLimits(*graph.find(selected),minimum,maximum);
        ImGui::SetNextWindowSize({std::clamp(310.f,minimum.x,maximum.x),maximum.y},ImGuiCond_Appearing);
        ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x-330,vp->Pos.y+390},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(minimum,maximum);
        if(focusControls){ImGui::SetNextWindowFocus();if(recoverControls)ImGui::SetNextWindowPos({vp->Pos.x+vp->Size.x-330,vp->Pos.y+std::min(390.f,std::max(60.f,vp->Size.y-500))});focusControls=false;recoverControls=false;}
        if(ImGui::Begin("Node Controls",&showControls,ImGuiWindowFlags_NoCollapse)){
            nodeDeletionFocus|=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            ImGui::BeginDisabled(exportActive);inspector();ImGui::EndDisabled();
        }ImGui::End();
    }
    void previewWindow(){
        if(!largePreview){focusPreview=false;return;}
        if(focusPreview){ImGui::SetNextWindowFocus();focusPreview=false;}
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
            if(ImGui::Button(playing?"PAUSE":"PLAY",{90,30}))playing=!playing;ImGui::SameLine();
            if(ImGui::Button("RESTART",{90,30}))safely([&]{position=0;video.seek(0);if(video.loaded())uploadPending=video.frame(0,image)||uploadPending;});
            float seconds=static_cast<float>(position),duration=static_cast<float>(video.loaded()?video.duration:10.);ImGui::SetNextItemWidth(-1);
            if(ImGui::SliderFloat("##Player timeline",&seconds,0,std::max(.01f,duration),"%.2f s"))safely([&]{position=seconds;if(video.loaded()){video.seek(position);uploadPending=video.frame(position,image)||uploadPending;}});
            ImGui::EndDisabled();if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape))largePreview=false;
        }ImGui::End();
    }
    void ui(Renderer& renderer){
        nodeDeletionFocus=false;applyTheme();historyShortcut();
        auto viewport=ImGui::GetMainViewport();ImGui::SetNextWindowPos(viewport->Pos);ImGui::SetNextWindowSize(viewport->Size);
        ImGui::Begin("flexar",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
        auto windowDraw=ImGui::GetWindowDrawList();
        drawWallpaper(windowDraw,viewport->Pos,viewport->Size);
        // 01 / masthead -------------------------------------------------------
        ImGui::SetCursorPos({0,0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,0});
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,.94f));
        ImGui::BeginChild("Navbar",{viewport->Size.x,60},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPosY(12);
        ImGui::PushFont(uiTitle);ImGui::TextColored(ink,"flexar");ImGui::PopFont();
        ImGui::SetCursorPos({212,15});ImGui::BeginGroup();
        ImGui::BeginDisabled(exportActive);
        if(ImGui::Button("01 LOOKS",{88,30}))ImGui::OpenPopup("Looks");
        if(ImGui::BeginPopup("Looks")){library();ImGui::EndPopup();}
        ImGui::SameLine(0,6);if(ImGui::Button("02 NODES",{88,30}))ImGui::OpenPopup("Effects");
        if(ImGui::BeginPopup("Effects")){heading("EFFECTS");effectMenu();ImGui::EndPopup();}
        ImGui::SameLine(0,6);if(ImGui::Button("03 OPEN",{80,30}))safely([&]{restore(dialog(hwnd,false,true));});
        ImGui::SameLine(0,6);if(ImGui::Button("04 SAVE",{80,30}))safely([&]{save(dialog(hwnd,true,true));});
        ImGui::SameLine(0,6);if(ImGui::Button("05 FIT",{70,30}))fitRequested=true;
        ImGui::SameLine(0,6);if(ImGui::Button("ARRANGE",{84,30}))arrange();
        ImGui::SameLine(0,6);ImGui::BeginDisabled(!canUndo());
        if(ImGui::Button("UNDO",{58,30}))safely([&]{undo();});
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Undo  Ctrl+Z");ImGui::EndDisabled();
        ImGui::SameLine(0,6);ImGui::BeginDisabled(!canRedo());
        if(ImGui::Button("REDO",{58,30}))safely([&]{redo();});
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Redo  Ctrl+Y / Ctrl+Shift+Z");ImGui::EndDisabled();
        ImGui::SameLine(0,6);ImGui::BeginDisabled(!hasMedia||!outputTexture);
        if(ImGui::Button("EXPORT",{84,30}))ImGui::OpenPopup("Export");
        if(ImGui::BeginPopup("Export")){
            heading("EXPORT");
            if(ImGui::MenuItem("PNG / current output"))safely([&]{startExport(false);});
            ImGui::BeginDisabled(!video.loaded());if(ImGui::MenuItem("MP4 / full clip / 30 FPS"))safely([&]{startExport(true);});ImGui::EndDisabled();
            ImGui::TextDisabled("MP4: silent H.264 / alpha over black");ImGui::EndPopup();
        }
        ImGui::EndDisabled();ImGui::EndDisabled();ImGui::SameLine(0,6);
        appearanceMenu();
        ImGui::EndGroup();
        // FPS is pinned to the far right of the masthead.
        ImGui::SameLine(std::max(ImGui::GetCursorPosX()+8,viewport->Size.x-82));
        ImGui::TextDisabled("%3.0f FPS",ImGui::GetIO().Framerate);
        // Flush rule at the true bottom edge of the masthead.
        ImVec2 navPos=ImGui::GetWindowPos(),navSize=ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddRectFilled({navPos.x,navPos.y+navSize.y-3},{navPos.x+navSize.x,navPos.y+navSize.y},accentU32);
        ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar();
        // 02 / transport ------------------------------------------------------
        ImGui::SetCursorPos({0,60});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,8});
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,.90f));
        ImGui::BeginChild("Transport",{viewport->Size.x,44},ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar);
        if(ImGui::Selectable("IMAGE",!importVideo,0,{65,28}))importVideo=false;ImGui::SameLine();
        if(ImGui::Selectable("VIDEO",importVideo,0,{65,28}))importVideo=true;ImGui::SameLine();
        if(ImGui::Button("IMPORT",{92,28}))safely([&]{loadSource(dialog(hwnd,false,false,importVideo));});ImGui::SameLine();
        ImGui::BeginDisabled(!hasMedia);
        if(ImGui::Button(playing?"PAUSE":"PLAY",{70,28}))playing=!playing;ImGui::SameLine();
        if(ImGui::Button("|<",{32,28}))safely([&]{position=0;video.seek(0);if(video.loaded())uploadPending=video.frame(0,image)||uploadPending;});ImGui::SameLine();
        float seconds=static_cast<float>(position),duration=static_cast<float>(video.loaded()?video.duration:10.);ImGui::SetNextItemWidth(std::max(120.f,ImGui::GetContentRegionAvail().x-150));
        if(ImGui::SliderFloat("##Playhead",&seconds,0,std::max(.01f,duration),"%.2f s"))safely([&]{position=seconds;if(video.loaded()){video.seek(position);uploadPending=video.frame(position,image)||uploadPending;}});
        ImGui::EndDisabled();ImGui::EndChild();ImGui::PopStyleColor();ImGui::PopStyleVar();
        ImGui::SetCursorPos({14,110});
        if(exportActive){ImGui::ProgressBar(float(exportFrame)/exportFrames,{ImGui::GetContentRegionAvail().x-100,22},"EXPORTING");ImGui::SameLine();if(ImGui::Button("CANCEL",{80,24}))cancelExport=true;}
        // 03 / canvas + rail --------------------------------------------------
        ImGui::BeginDisabled(exportActive);
        sourceTexture=hasMedia&&!uploadPending?renderer.sourceHandle():0;
        uint64_t evaluated=renderer.evaluate(graph,static_cast<float>(position),false);outputTexture=hasMedia&&!uploadPending?evaluated:0;
        float height=std::max(160.f,ImGui::GetContentRegionAvail().y-46),rail=300.f;
        float graphWidth=std::max(240.f,ImGui::GetContentRegionAvail().x-rail-10);
        ImGui::PushStyleColor(ImGuiCol_Border,IM_COL32(70,76,72,255));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,0.f));
        ImGui::BeginChild("Graph pane",{graphWidth,height},true,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();ImGui::PopStyleColor();canvas();ImGui::EndChild();
        ImGui::SameLine();ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,0.f));
        ImGui::BeginChild("Rail",{rail,height},false,ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,.94f));
        ImGui::BeginChild("Preview panel",{0,250},true,ImGuiWindowFlags_NoScrollbar);
        heading("01 / PREVIEW");ImGui::SameLine();ImGui::Checkbox("ORIGINAL",&original);ImGui::Separator();
        ImVec2 a=ImGui::GetCursorScreenPos();float w=ImGui::GetContentRegionAvail().x,h=std::max(40.f,ImGui::GetContentRegionAvail().y-38);
        thumbnail(ImGui::GetWindowDrawList(),a,{a.x+w,a.y+h},original?sourceTexture:outputTexture,hasMedia?"output disconnected":"import image or video");
        ImGui::InvisibleButton("Expand preview image",{w,h});if(ImGui::IsItemClicked())openPreview();
        if(ImGui::Button("EXPAND / PLAY",{-1,28}))openPreview();
        ImGui::EndChild();ImGui::PopStyleColor();
        ImGui::Spacing();musicSection();
        ImGui::EndChild();
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::TextUnformatted(status.c_str());
        ImGui::End();controlsWindow();previewWindow();compositionWindow(renderer);deleteNodeShortcut();crosshair();
    }
