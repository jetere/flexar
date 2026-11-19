    const wchar_t* backdropFiles[4]={L"perfect_hue_80.jpg",L"perfect_hue_91.jpg",L"perfect_hue_93.jpg",L"perfect_hue_117.jpg"};
    const char* backdropNames[4]={"01 / TEAL GLASS","02 / VIOLET STRUCTURE","03 / BLUE FACETS","04 / AMBER FIELD"};
    int backdropStyle{};
    uint64_t backdropHandles[4]{};
    ImVec2 backdropSizes[4]{};
    bool backdropAttempted[4]{};
    void ensureBackdrop(int index){
        int i=std::clamp(index,0,3);
        if(backdropAttempted[i]||!runtime)return;
        backdropAttempted[i]=true;
        safely([&]{
            auto path=findAsset(root,backdropFiles[i],{L"assets/backgrounds",L"Music"});
            if(path.empty())throw std::runtime_error("Wallpaper not found");
            auto image=loadImage(path);
            backdropSizes[i]={float(image.width),float(image.height)};
            backdropHandles[i]=runtime->uploadBackdrop(image,static_cast<size_t>(i));
        });
    }
    void drawWallpaper(ImDrawList* draw,ImVec2 pos,ImVec2 size){
        if(size.x<=0||size.y<=0)return;
        int i=std::clamp(backdropStyle,0,3);ensureBackdrop(i);
        ImVec2 end{pos.x+size.x,pos.y+size.y};
        draw->AddRectFilled(pos,end,IM_COL32(9,11,10,255));
        if(backdropHandles[i]){
            float imageAspect=backdropSizes[i].x/std::max(1.f,backdropSizes[i].y);
            float canvasAspect=size.x/size.y;ImVec2 uv0{0,0},uv1{1,1};
            if(canvasAspect<imageAspect){float visible=canvasAspect/imageAspect;uv0.x=(1-visible)*.5f;uv1.x=1-uv0.x;}
            else{float visible=imageAspect/canvasAspect;uv0.y=(1-visible)*.5f;uv1.y=1-uv0.y;}
            draw->AddImage(reinterpret_cast<ImTextureID>(backdropHandles[i]),pos,end,uv0,uv1);
            draw->AddRectFilled(pos,end,IM_COL32(7,9,8,150));
        }
    }
    void graphBackdrop(ImDrawList*,ImVec2,ImVec2){}
    void crosshair(){}
    void musicSection(){
        ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(paper.x,paper.y,paper.z,.96f));
        ImGui::BeginChild("Music section",{0,152},true,ImGuiWindowFlags_NoScrollbar);
        ImVec2 mark=ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(mark,{mark.x+30,mark.y+4},accentU32);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY()+8);
        heading("MUSIC");ImGui::Separator();
        bool ready=music&&music->available();
        bool musicPlaying=ready&&music->playing();
        if(music&&!music->errorText.empty())ImGui::TextWrapped("%s",music->errorText.c_str());
        ImGui::BeginDisabled(!ready);
        float width=(ImGui::GetContentRegionAvail().x-8)*.5f;
        // Active transport is filled with the accent; the other stays neutral.
        if(musicPlaying)ImGui::PushStyleColor(ImGuiCol_Button,accent);
        if(ImGui::Button("PLAY",{width,34}))music->resume();
        if(musicPlaying)ImGui::PopStyleColor();
        ImGui::SameLine(0,8);
        if(!musicPlaying)ImGui::PushStyleColor(ImGuiCol_Button,accent);
        if(ImGui::Button("PAUSE",{width,34}))music->pause();
        if(!musicPlaying)ImGui::PopStyleColor();
        ImGui::Spacing();
        int volume=ready?music->volume():0;
        ImGui::SetNextItemWidth(-1);
        if(ImGui::SliderInt("##MusicVolume",&volume,0,100,"VOLUME  %d%%"))music->setVolume(volume);
        ImGui::EndDisabled();
        ImGui::EndChild();ImGui::PopStyleColor();
    }
    void appearanceMenu(){
        if(ImGui::Button("SURFACE",{96,30}))ImGui::OpenPopup("Surface");
        if(ImGui::BeginPopup("Surface")){
            heading("SURFACE");
            ImGui::TextDisabled("STATIC WALLPAPER / 04 SOURCES");
            ImGui::Spacing();
            const ImVec4 swatches[4]={{.10f,.52f,.50f,1},{.45f,.28f,.74f,1},{.08f,.42f,.70f,1},{.86f,.56f,.10f,1}};
            for(int i=0;i<4;++i){
                ImGui::PushID(i);
                bool active=backdropStyle==i;
                ImGui::PushStyleColor(ImGuiCol_Button,active?accent:swatches[i]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,active?accent:ImVec4(swatches[i].x,swatches[i].y,swatches[i].z,.78f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,ink);
                if(ImGui::Button(backdropNames[i],{260,28})){backdropStyle=i;applyTheme();ImGui::CloseCurrentPopup();}
                ImGui::PopStyleColor(3);ImGui::PopID();
            }
            ImGui::EndPopup();
        }
    }
