    // Layer transform helpers mirror the compositor shader exactly. Path points
    // live in layer source coordinates, so masks travel with the layer.
    ImVec2 layerForward(const CompositeLayer& layer,ImVec2 source)const{
        float x=layer.flipX?1-source.x:source.x,y=layer.flipY?1-source.y:source.y;
        float dx=(x-.5f)*layer.scaleX,dy=(y-.5f)*layer.scaleY;
        float angle=layer.rotation*3.14159265f/180.f,cs=std::cos(angle),sn=std::sin(angle);
        return {.5f+layer.x+cs*dx-sn*dy,.5f+layer.y+sn*dx+cs*dy};
    }
    ImVec2 layerInverse(const CompositeLayer& layer,ImVec2 uv)const{
        float dx=uv.x-.5f-layer.x,dy=uv.y-.5f-layer.y;
        float angle=layer.rotation*3.14159265f/180.f,cs=std::cos(angle),sn=std::sin(angle);
        float x=(cs*dx+sn*dy)/std::max(layer.scaleX,.001f)+.5f,y=(-sn*dx+cs*dy)/std::max(layer.scaleY,.001f)+.5f;
        if(layer.flipX)x=1-x;if(layer.flipY)y=1-y;return {x,y};
    }
    void seekComposition(double seconds){
        position=std::clamp(seconds,0.,double(std::max(.1f,graph.find(compositionNode)?graph.find(compositionNode)->compDuration:10.f)));
        if(video.loaded()){video.seek(std::min(position,std::max(0.,video.duration-1./30.)));uploadPending=video.frame(position,image)||uploadPending;}
    }
    void compositionSettings(Node& node){
        ImGui::TextDisabled("COMPOSITION");
        int presetIndex=0;
        if(node.compWidth==1920&&node.compHeight==1080)presetIndex=1;
        else if(node.compWidth==1080&&node.compHeight==1350)presetIndex=2;
        else if(node.compWidth==1080&&node.compHeight==1080)presetIndex=3;
        else if(node.compWidth==2560&&node.compHeight==1440)presetIndex=4;
        else if(node.compWidth==3840&&node.compHeight==2160)presetIndex=5;
        const char* presets[]={"Custom","1920 x 1080 / 16:9","1080 x 1350 / 4:5","1080 x 1080 / 1:1","2560 x 1440 / 16:9","3840 x 2160 / 16:9"};
        ImGui::SetNextItemWidth(-1);
        if(ImGui::Combo("##CompositionPreset",&presetIndex,presets,6)){
            switch(presetIndex){case 1:node.compWidth=1920;node.compHeight=1080;break;case 2:node.compWidth=1080;node.compHeight=1350;break;case 3:node.compWidth=1080;node.compHeight=1080;break;case 4:node.compWidth=2560;node.compHeight=1440;break;case 5:node.compWidth=3840;node.compHeight=2160;break;default:break;}
        }
        ImGui::TextDisabled("SIZE");
        ImGui::SetNextItemWidth(78);if(ImGui::DragInt("##Width",&node.compWidth,1,16,7680,"%d",ImGuiSliderFlags_AlwaysClamp))node.compWidth=std::clamp(node.compWidth,16,7680);
        ImGui::SameLine();ImGui::TextDisabled("x");ImGui::SameLine();
        ImGui::SetNextItemWidth(78);if(ImGui::DragInt("##Height",&node.compHeight,1,16,7680,"%d",ImGuiSliderFlags_AlwaysClamp))node.compHeight=std::clamp(node.compHeight,16,7680);
        ImGui::SameLine();if(ImGui::Button("Match",{64,24})&&hasMedia){node.compWidth=int(std::max(16u,image.width));node.compHeight=int(std::max(16u,image.height));}
        ImGui::TextDisabled("TIMING");
        ImGui::SetNextItemWidth(96);ImGui::DragFloat("##Duration",&node.compDuration,.1f,.1f,3600,"%.2f s",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();static const int rates[]={12,24,25,30,50,60};int rate=0;for(int i=0;i<6;++i)if(rates[i]==node.compFps)rate=i;
        ImGui::SetNextItemWidth(92);if(ImGui::Combo("##Fps",&rate,"12 FPS\0" "24 FPS\0" "25 FPS\0" "30 FPS\0" "50 FPS\0" "60 FPS\0"))node.compFps=rates[rate];
        ImGui::TextDisabled("BACKGROUND");ImGui::SameLine();ImGui::ColorEdit3("##Background",&node.compBgR,ImGuiColorEditFlags_NoInputs);
    }
    void compositionSetup(Node& node){
        int count=static_cast<int>(node.layers.size());
        if(ImGui::SliderInt("Layer inputs",&count,1,8)){graph.setLayerCount(node.id,count);compositionLayer=std::min(compositionLayer,count-1);}
        Id id=node.id;
        if(ImGui::Button("Create missing source nodes",{-1,28})){
            for(size_t i=0;i<graph.find(id)->inputs.size()&&graph.nodes.size()<32;++i){auto pin=graph.find(id)->inputs[i].id;bool connected=false;for(auto& c:graph.connections)if(c.to==pin)connected=true;if(connected)continue;
                auto source=graph.add(Effect::Source,graph.find(id)->x-330,graph.find(id)->y+float(i)*260);std::string error;graph.connect(graph.find(source)->outputs[0].id,pin,error);
            }
            fitRequested=true;status="Source nodes created. Select each source to import its media.";
        }
    }
    void layerControls(Node& node){
        compositionLayer=std::clamp(compositionLayer,0,int(node.layers.size())-1);
        ImGui::TextDisabled("LAYERS");
        for(int i=int(node.layers.size())-1;i>=0;--i){ImGui::PushID(i);auto& layer=node.layers[size_t(i)];
            ImGui::Checkbox("##Visible",&layer.enabled);ImGui::SameLine();
            std::string name="Layer "+std::to_string(i+1);Id producer=0;for(auto& c:graph.connections)if(c.to==node.inputs[size_t(i)].id)producer=graph.pin(c.from)->node;
            name+=" / "+(producer?graph.find(producer)->name:std::string("empty"));if(!layer.path.empty())name+=" *";if(layer.locked)name+="  [LOCK]";
            if(ImGui::Selectable(name.c_str(),compositionLayer==i,0,{0,23}))compositionLayer=i;
            ImGui::PopID();
        }
        ImGui::Separator();auto& layer=node.layers[size_t(compositionLayer)];
        ImGui::Checkbox("Visible",&layer.enabled);ImGui::SameLine();ImGui::Checkbox("Lock",&layer.locked);
        ImGui::SameLine();if(ImGui::Button("Reset layer"))layer=CompositeLayer{};
        ImGui::SliderFloat("Opacity",&layer.opacity,0,1,"%.3f");ImGui::Combo("Blend mode",&layer.mode,BlendModes,12);
        if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
            ImGui::BeginDisabled(layer.locked);
            ImGui::DragFloat("Position X",&layer.x,.002f,-2,2,"%.3f",ImGuiSliderFlags_AlwaysClamp);ImGui::DragFloat("Position Y",&layer.y,.002f,-2,2,"%.3f",ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Scale X",&layer.scaleX,.01f,.01f,10,"%.3f",ImGuiSliderFlags_AlwaysClamp);ImGui::DragFloat("Scale Y",&layer.scaleY,.01f,.01f,10,"%.3f",ImGuiSliderFlags_AlwaysClamp);
            ImGui::DragFloat("Rotation",&layer.rotation,.5f,-360,360,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);
            ImGui::Checkbox("Flip X",&layer.flipX);ImGui::SameLine();ImGui::Checkbox("Flip Y",&layer.flipY);
            if(ImGui::Button("Center")){layer.x=0;layer.y=0;}ImGui::SameLine();if(ImGui::Button("Original size")){layer.scaleX=1;layer.scaleY=1;}
            ImGui::EndDisabled();
        }
        if(ImGui::CollapsingHeader("Crop")){
            ImGui::SliderFloat("Left",&layer.cropLeft,0,.99f-layer.cropRight);ImGui::SliderFloat("Right",&layer.cropRight,0,.99f-layer.cropLeft);
            ImGui::SliderFloat("Top",&layer.cropTop,0,.99f-layer.cropBottom);ImGui::SliderFloat("Bottom",&layer.cropBottom,0,.99f-layer.cropTop);
        }
        if(ImGui::CollapsingHeader("Mask / path",layer.path.empty()?0:ImGuiTreeNodeFlags_DefaultOpen)){
            ImGui::Checkbox("Enable mask",&layer.maskEnabled);ImGui::SameLine();ImGui::Checkbox("Invert",&layer.maskInvert);
            ImGui::SliderFloat("Feather",&layer.maskFeather,0,.2f,"%.3f");
            ImGui::TextDisabled("%u points / pen tool disabled",unsigned(layer.path.size()));
            if(ImGui::Button("Remove last")){if(!layer.path.empty())layer.path.pop_back();}
            ImGui::SameLine();if(ImGui::Button("Clear path"))layer.path.clear();
            ImGui::SameLine();if(ImGui::Button("Close path")&&layer.path.size()>=3)status="Mask path is a closed polygon.";
        }
        ImGui::BeginDisabled(compositionLayer==0);if(ImGui::Button("Move backward")){std::swap(node.layers[size_t(compositionLayer)],node.layers[size_t(compositionLayer-1)]);std::swap(node.inputs[size_t(compositionLayer)],node.inputs[size_t(compositionLayer-1)]);--compositionLayer;}ImGui::EndDisabled();
        ImGui::SameLine();ImGui::BeginDisabled(compositionLayer+1==int(node.layers.size()));if(ImGui::Button("Move forward")){std::swap(node.layers[size_t(compositionLayer)],node.layers[size_t(compositionLayer+1)]);std::swap(node.inputs[size_t(compositionLayer)],node.inputs[size_t(compositionLayer+1)]);++compositionLayer;}ImGui::EndDisabled();
    }
    void compositionWindow(Renderer& renderer){
        auto node=graph.find(compositionNode);if(!showComposition||!node||node->effect!=Effect::Compositor){focusComposition=false;studioLayerDrag=false;studioHandle=-1;return;}
        if(focusComposition){ImGui::SetNextWindowFocus();focusComposition=false;}
        auto viewport=ImGui::GetMainViewport();float studioWidth=std::min(1280.f,viewport->Size.x-40),studioHeight=std::min(820.f,viewport->Size.y-130);
        ImGui::SetNextWindowSize({studioWidth,studioHeight},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({viewport->Pos.x+(viewport->Size.x-studioWidth)*.5f,viewport->Pos.y+112},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({840,560},{viewport->Size.x-20,viewport->Size.y-120});
        if(ImGui::Begin("Composition studio",&showComposition,ImGuiWindowFlags_NoCollapse)){
            ImGui::BeginDisabled(exportActive);
            node=graph.find(compositionNode);
            // Pen drawing is temporarily disabled. Existing mask paths still render.
            if(studioTool==1)studioTool=0;
            auto toolButton=[&](int value,const char* tooltip,bool disabled,auto icon){
                bool active=!disabled&&studioTool==value;
                if(disabled)ImGui::BeginDisabled();
                if(active){ImGui::PushStyleColor(ImGuiCol_Button,accent);ImGui::PushStyleColor(ImGuiCol_ButtonHovered,accent);}
                ImVec2 pos=ImGui::GetCursorScreenPos();ImGui::PushID(value);
                if(ImGui::Button("##StudioTool",{40,28}))studioTool=value;
                ImGui::PopID();if(active)ImGui::PopStyleColor(2);
                if(disabled)ImGui::EndDisabled();
                ImU32 color=disabled?IM_COL32(92,97,94,255):active?IM_COL32(10,12,11,255):ImGui::GetColorU32(ImGuiCol_Text);
                icon({pos.x+20,pos.y+14},color);
                if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",tooltip);
            };
            toolButton(0,"Select  (V)",false,[&](ImVec2 c,ImU32 col){auto d=ImGui::GetWindowDrawList();
                d->AddTriangleFilled({c.x-7,c.y-9},{c.x+8,c.y+0},{c.x+1,c.y+2},col);
                d->AddTriangleFilled({c.x-7,c.y-9},{c.x+1,c.y+2},{c.x+4,c.y+9},col);});
            ImGui::SameLine(0,4);
            toolButton(1,"Pen  (temporarily disabled)",true,[&](ImVec2 c,ImU32 col){auto d=ImGui::GetWindowDrawList();
                d->AddLine({c.x-8,c.y+8},{c.x+7,c.y-8},col,2.f);
                d->AddTriangleFilled({c.x+7,c.y-8},{c.x+2,c.y-6},{c.x+4,c.y-1},col);
                d->AddLine({c.x-10,c.y+10},{c.x-6,c.y+6},col,2.f);});
            ImGui::SameLine(0,4);
            toolButton(2,"Hand  (H)",false,[&](ImVec2 c,ImU32 col){auto d=ImGui::GetWindowDrawList();
                for(int i=0;i<4;++i)d->AddLine({c.x-6+i*3.f,c.y-1},{c.x-6+i*3.f,c.y-7+(i==0||i==3?3:0)},col,2.f);
                d->AddRectFilled({c.x-7,c.y-1},{c.x+5,c.y+8},col);
                d->AddLine({c.x+5,c.y+4},{c.x+9,c.y+0},col,2.f);});
            ImGui::SameLine(0,12);ImGui::TextDisabled("%d x %d  /  %d FPS",node->compWidth,node->compHeight,node->compFps);
            ImGui::SameLine(std::max(ImGui::GetCursorPosX()+12,ImGui::GetWindowWidth()-260));
            ImGui::TextDisabled("ZOOM %3.0f%%",studioZoom*100.f);
            ImGui::SameLine();if(ImGui::Button("FIT",{48,24})){studioZoom=1;studioPan={0,0};}

            float right=310,timelineHeight=94;
            float contentHeight=std::max(180.f,ImGui::GetContentRegionAvail().y-timelineHeight-10);
            ImGui::BeginChild("Composition image",{std::max(300.f,ImGui::GetContentRegionAvail().x-right-10),contentHeight},true,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
            ImVec2 top=ImGui::GetCursorScreenPos(),room=ImGui::GetContentRegionAvail();auto draw=ImGui::GetWindowDrawList();
            ImGui::InvisibleButton("Transform surface",room,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle|ImGuiButtonFlags_MouseButtonRight);
            bool hovered=ImGui::IsItemHovered();auto& io=ImGui::GetIO();
            draw->AddRectFilled(top,{top.x+room.x,top.y+room.y},IM_COL32(8,9,9,255));
            float compAspect=float(node->compWidth)/std::max(1,node->compHeight);
            float fit=std::min(room.x/float(node->compWidth),room.y/float(node->compHeight))*studioZoom;
            ImVec2 compSize{float(node->compWidth)*fit,float(node->compHeight)*fit};
            ImVec2 origin{top.x+(room.x-compSize.x)*.5f+studioPan.x,top.y+(room.y-compSize.y)*.5f+studioPan.y};
            ImVec2 imageSize;
            float mediaAspect=float(renderer.width())/std::max(1u,renderer.height());
            if(mediaAspect>compAspect)imageSize={compSize.x,compSize.x/mediaAspect};else imageSize={compSize.y*mediaAspect,compSize.y};
            ImVec2 imageA{origin.x+(compSize.x-imageSize.x)*.5f,origin.y+(compSize.y-imageSize.y)*.5f},imageB{imageA.x+imageSize.x,imageA.y+imageSize.y};
            draw->AddRectFilled(origin,{origin.x+compSize.x,origin.y+compSize.y},IM_COL32(int(node->compBgR*255),int(node->compBgG*255),int(node->compBgB*255),255));
            uint64_t texture=renderer.evaluate(graph,float(position),false,compositionNode);
            if(texture)draw->AddImage(texture,imageA,imageB);
            draw->AddRect(origin,{origin.x+compSize.x,origin.y+compSize.y},IM_COL32(80,86,82,255));
            draw->AddRect(imageA,imageB,IM_COL32(112,118,113,180));
            if(hovered&&io.MouseWheel!=0){float next=std::clamp(studioZoom*std::pow(1.12f,io.MouseWheel),.15f,8.f);ImVec2 center{top.x+room.x*.5f,top.y+room.y*.5f};ImVec2 rel{io.MousePos.x-center.x-studioPan.x,io.MousePos.y-center.y-studioPan.y};studioPan.x+=rel.x*(1-next/studioZoom);studioPan.y+=rel.y*(1-next/studioZoom);studioZoom=next;}
            if(hovered&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle)){studioPan.x+=io.MouseDelta.x;studioPan.y+=io.MouseDelta.y;}
            if(hovered&&studioTool==2&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){studioPan.x+=io.MouseDelta.x;studioPan.y+=io.MouseDelta.y;}
            auto screenFromUv=[&](ImVec2 uv){return ImVec2(imageA.x+uv.x*imageSize.x,imageA.y+uv.y*imageSize.y);};
            auto uvFromScreen=[&](ImVec2 screen){return ImVec2((screen.x-imageA.x)/std::max(1.f,imageSize.x),(screen.y-imageA.y)/std::max(1.f,imageSize.y));};
            bool overImage=io.MousePos.x>=imageA.x&&io.MousePos.x<=imageB.x&&io.MousePos.y>=imageA.y&&io.MousePos.y<=imageB.y;
            compositionLayer=std::clamp(compositionLayer,0,int(node->layers.size())-1);auto& layer=node->layers[size_t(compositionLayer)];
            if((ImGui::IsKeyPressed(ImGuiKey_V)||ImGui::IsKeyPressed(ImGuiKey_H))&&!io.WantTextInput&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))studioTool=ImGui::IsKeyPressed(ImGuiKey_V)?0:2;
            ImVec2 corners[4];{float xs[]={0,1,1,0},ys[]={0,0,1,1};for(int i=0;i<4;++i)corners[i]=screenFromUv(layerForward(layer,{xs[i],ys[i]}));}
            ImVec2 edges[4]={screenFromUv(layerForward(layer,{.5f,0})),screenFromUv(layerForward(layer,{1,.5f})),screenFromUv(layerForward(layer,{.5f,1})),screenFromUv(layerForward(layer,{0,.5f}))};
            ImVec2 center{(corners[0].x+corners[1].x+corners[2].x+corners[3].x)*.25f,(corners[0].y+corners[1].y+corners[2].y+corners[3].y)*.25f};
            ImVec2 topMid=edges[0],rotate{topMid.x+(topMid.x-center.x)*.18f,topMid.y+(topMid.y-center.y)*.18f};
            {float dx=topMid.x-center.x,dy=topMid.y-center.y,length=std::sqrt(dx*dx+dy*dy);if(length>.001f){dx/=length;dy/=length;}rotate={topMid.x+dx*26,topMid.y+dy*26};}
            auto nearPoint=[&](ImVec2 point,float radius){float dx=io.MousePos.x-point.x,dy=io.MousePos.y-point.y;return dx*dx+dy*dy<=radius*radius;};
            int hitHandle=-1;
            if(studioTool==0&&!layer.locked){for(int i=0;i<8;++i){if(nearPoint(i<4?corners[i]:edges[i-4],9)){hitHandle=i;break;}}if(hitHandle<0&&nearPoint(rotate,10))hitHandle=8;}
            auto pickLayer=[&](ImVec2 uv){for(int i=int(node->layers.size())-1;i>=0;--i){auto& candidate=node->layers[size_t(i)];if(!candidate.enabled)continue;auto q=layerInverse(candidate,uv);if(q.x>=0&&q.x<=1&&q.y>=0&&q.y<=1)return i;}return -1;};
            if(hovered&&studioTool!=2&&ImGui::IsMouseClicked(0)&&overImage){
                ImVec2 uv=uvFromScreen(io.MousePos);
                if(hitHandle>=0){studioLayerDrag=true;studioHandle=hitHandle;studioDragStart=io.MousePos;studioStartX=layer.x;studioStartY=layer.y;studioStartScaleX=layer.scaleX;studioStartScaleY=layer.scaleY;studioStartRotation=layer.rotation;}
                else{int picked=pickLayer(uv);if(picked>=0){compositionLayer=picked;auto& fresh=node->layers[size_t(compositionLayer)];studioLayerDrag=!fresh.locked;studioHandle=studioLayerDrag?9:-1;studioDragStart=io.MousePos;studioStartX=fresh.x;studioStartY=fresh.y;studioStartScaleX=fresh.scaleX;studioStartScaleY=fresh.scaleY;studioStartRotation=fresh.rotation;}}
            }
            if(studioLayerDrag&&ImGui::IsMouseDown(0)){auto& active=node->layers[size_t(compositionLayer)];
                if(studioHandle==9){active.x=std::clamp(studioStartX+(io.MousePos.x-studioDragStart.x)/std::max(1.f,imageSize.x),-2.f,2.f);active.y=std::clamp(studioStartY+(io.MousePos.y-studioDragStart.y)/std::max(1.f,imageSize.y),-2.f,2.f);}
                else if(studioHandle==8){float dx=io.MousePos.x-center.x,dy=io.MousePos.y-center.y;float degrees=std::atan2(dy,dx)*57.2957795f+90;while(degrees>180)degrees-=360;while(degrees<-180)degrees+=360;active.rotation=degrees;}
                else{ImVec2 uv=uvFromScreen(io.MousePos);float dx=uv.x-.5f-active.x,dy=uv.y-.5f-active.y;float angle=active.rotation*3.14159265f/180.f,cs=std::cos(angle),sn=std::sin(angle);float rx=cs*dx+sn*dy,ry=-sn*dx+cs*dy;
                    float scaleX=std::clamp(std::abs(rx)*2,.01f,10.f),scaleY=std::clamp(std::abs(ry)*2,.01f,10.f);
                    if(studioHandle<4){if(io.KeyShift){float uniform=std::max(scaleX,scaleY);scaleX=scaleY=uniform;}active.scaleX=scaleX;active.scaleY=scaleY;}
                    else if(studioHandle==4||studioHandle==6)active.scaleY=scaleY;else active.scaleX=scaleX;
                }
            }
            if(!ImGui::IsMouseDown(0)){studioLayerDrag=false;studioHandle=-1;}
            // Selection chrome -------------------------------------------------
            if(studioTool==0){
                draw->AddPolyline(corners,4,IM_COL32(236,232,223,235),ImDrawFlags_Closed,2.f);
                draw->AddLine(edges[0],rotate,IM_COL32(236,232,223,180),1.5f);
                for(int i=0;i<4;++i){ImVec2 p=corners[i];draw->AddRectFilled({p.x-5,p.y-5},{p.x+5,p.y+5},IM_COL32(236,232,223,255));draw->AddRect({p.x-5,p.y-5},{p.x+5,p.y+5},accentU32);}
                for(int i=0;i<4;++i){ImVec2 p=edges[i];draw->AddRectFilled({p.x-4,p.y-4},{p.x+4,p.y+4},IM_COL32(236,232,223,255));draw->AddRect({p.x-4,p.y-4},{p.x+4,p.y+4},accentU32);}
                draw->AddCircleFilled(rotate,6,IM_COL32(236,232,223,255));draw->AddCircle(rotate,6,accentU32,0,2.f);
            }
            // Mask path overlay ------------------------------------------------
            auto& active=node->layers[size_t(compositionLayer)];
            if(!active.path.empty()){
                std::vector<ImVec2> screen;screen.reserve(active.path.size());
                for(auto& point:active.path)screen.push_back(screenFromUv(layerForward(active,{point.x,point.y})));
                if(screen.size()>=2)draw->AddPolyline(screen.data(),int(screen.size()),IM_COL32(236,232,223,200),active.path.size()>=3?ImDrawFlags_Closed:0,1.5f);
                for(size_t i=0;i<screen.size();++i){ImVec2 p=screen[i];bool first=i==0;float radius=first?6.f:4.f;draw->AddCircleFilled(p,radius,first?accentU32:IM_COL32(236,232,223,255));draw->AddCircle(p,radius,IM_COL32(12,14,13,255),0,1.5f);}
            }
            // Tool hint --------------------------------------------------------
            const char* hint=studioTool==0?"SELECT / drag to move, corners scale, top handle rotates":"HAND / drag to pan, wheel to zoom";
            draw->AddRectFilled({top.x+10,top.y+10},{top.x+std::min(room.x-20,420.f),top.y+34},IM_COL32(8,9,9,210));
            draw->AddText({top.x+18,top.y+15},IM_COL32(206,203,195,255),hint);
            if(active.locked){draw->AddText({top.x+18,top.y+42},IM_COL32(230,150,60,255),"LAYER LOCKED");}
            if(overImage&&(hitHandle>=0||studioLayerDrag))ImGui::SetMouseCursor(studioHandle==8?ImGuiMouseCursor_Hand:ImGuiMouseCursor_ResizeAll);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("Composition layers",{0,contentHeight},true);
            compositionSettings(*node);ImGui::Separator();compositionSetup(*node);node=graph.find(compositionNode);ImGui::Separator();layerControls(*node);
            ImGui::EndChild();
            // Timeline ---------------------------------------------------------
            node=graph.find(compositionNode);
            ImGui::Spacing();
            if(ImGui::Button(playing?"PAUSE":"PLAY",{84,28}))playing=!playing;ImGui::SameLine();
            if(ImGui::Button("|<",{32,28}))seekComposition(0);ImGui::SameLine();
            if(ImGui::Button("<",{28,28}))seekComposition(position-1.0/std::max(1,node->compFps));ImGui::SameLine();
            if(ImGui::Button(">",{28,28}))seekComposition(position+1.0/std::max(1,node->compFps));ImGui::SameLine();
            position=std::clamp(position,0.,double(std::max(.1f,node->compDuration)));
            float seconds=float(position);ImGui::SetNextItemWidth(230);
            if(ImGui::SliderFloat("##Composition time",&seconds,0,std::max(.1f,node->compDuration),"%.2f s"))seekComposition(seconds);
            ImGui::SameLine();ImGui::TextDisabled("FRAME %d / %d  /  %d FPS",int(std::lround(position*node->compFps)),int(std::lround(node->compDuration*node->compFps)),node->compFps);
            ImGui::NewLine();
            ImVec2 rulerA=ImGui::GetCursorScreenPos();float rulerWidth=ImGui::GetContentRegionAvail().x;ImVec2 rulerB{rulerA.x+rulerWidth,rulerA.y+30};
            draw->AddRectFilled(rulerA,rulerB,IM_COL32(10,12,11,255));draw->AddRect(rulerA,rulerB,IM_COL32(48,53,50,255));
            float duration=std::max(.1f,node->compDuration);int marks=std::min(60,int(std::ceil(duration)));
            for(int i=0;i<=marks;++i){float t=float(i)/std::max(1,marks);float x=rulerA.x+rulerWidth*t;bool major=(i%5==0);draw->AddLine({x,rulerA.y},{x,rulerB.y},major?IM_COL32(92,98,94,255):IM_COL32(52,57,54,255));if(major){char label[16];snprintf(label,sizeof(label),"%.0f",t*duration);draw->AddText({x+3,rulerA.y+8},IM_COL32(138,143,139,255),label);}}
            float playX=rulerA.x+rulerWidth*std::clamp(float(position/duration),0.f,1.f);draw->AddLine({playX,rulerA.y},{playX,rulerB.y},accentU32,2.f);draw->AddTriangleFilled({playX-5,rulerA.y},{playX+5,rulerA.y},{playX,rulerA.y+7},accentU32);
            ImGui::InvisibleButton("##Composition ruler",{rulerWidth,30});
            if(ImGui::IsItemActive())seekComposition(double(std::clamp((io.MousePos.x-rulerA.x)/std::max(1.f,rulerWidth),0.f,1.f))*duration);
            if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape))showComposition=false;
            ImGui::EndDisabled();
        }ImGui::End();
    }
