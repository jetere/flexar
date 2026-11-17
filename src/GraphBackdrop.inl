    void graphBackdrop(ImDrawList* draw,ImVec2 base,ImVec2 area){
        // Port of Poison's 320x180 pixel-canvas sky, seeded stars and RIDGES.
        // Original: poison fixed/src/game/parts/11-canvas-renderer.js.
        auto hash=[](int a,int b){uint32_t h=uint32_t(a+1013)*374761393u ^ uint32_t(b+7919)*668265263u;h=(h^(h>>13))*1274126177u;return float(h^(h>>16))/4294967296.f;};
        auto mod=[](float a,float b){return std::fmod(std::fmod(a,b)+b,b);};
        float scale=std::max(2.f,std::floor(std::min(area.x/320,area.y/180)));
        float w=std::ceil(area.x/scale),h=std::ceil(area.y/scale);
        auto rect=[&](float x,float y,float width,float height,ImU32 color){draw->AddRectFilled({base.x+std::floor(x)*scale,base.y+std::floor(y)*scale},{base.x+std::floor(x+width)*scale,base.y+std::floor(y+height)*scale},color);};
        for(int band=0;band<14;++band){float t=band/13.f;rect(0,band*h/14,w,h/14+1,IM_COL32(int(3+11*t),int(2+7*t),int(10+24*t),255));}
        float cameraX=-pan.x/scale,cameraY=-pan.y/scale;
        for(int i=0;i<160;++i){float x=mod(hash(i,21)*w-cameraX*.05f,w),y=mod(hash(i,22)*h*.75f-cameraY*.03f,h);int light=55+(i%4)*20;rect(x,y,1,1,IM_COL32(light,light,light+20,255));}
        const float bases[]={112,138,162},amps[]={46,34,22},steps[]={36,28,20},par[]={.12f,.25f,.4f},parY[]={.1f,.18f,.3f};
        const int seeds[]={5,9,13};const ImU32 colors[]={IM_COL32(30,23,61,255),IM_COL32(19,15,41,255),IM_COL32(10,9,24,255)};
        for(int layer=0;layer<3;++layer){
            if(layer==2)for(int i=0;i<12;++i){float x=mod(i*90+hash(i,1)*60-cameraX*.32f,w+90)-45;float spire=26+hash(i,2)*50,sw=4+hash(i,3)*7,top=h*.8f-spire;
                for(float y=top;y<h;++y){float half=std::min(sw,1+std::floor((y-top)*sw/spire));rect(x-half,y,half*2,1,IM_COL32(18,13,38,255));}rect(x-1,top-2,2,2,IM_COL32(81,56,105,255));}
            for(float x=0;x<w;x+=2){float u=(x+cameraX*par[layer])/steps[layer];int i=int(std::floor(u));float f=u-i,sm=f*f*(3-2*f);float a=hash(int(mod(float(i),48)),seeds[layer]),b=hash(int(mod(float(i+1),48)),seeds[layer]);float y=(bases[layer]-(a+(b-a)*sm)*amps[layer])*h/180-cameraY*parY[layer];y=std::floor(y/2)*2;rect(x,y,2,h-y+2,colors[layer]);}
        }
    }
    void crosshair(){
        // App-local cursor; no system cursor settings are changed.
        auto& io=ImGui::GetIO();bool inside=ImGui::IsMousePosValid()&&ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
        if(!inside)return;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);auto draw=ImGui::GetForegroundDrawList();ImVec2 p{std::floor(io.MousePos.x),std::floor(io.MousePos.y)};
        auto stroke=[&](float x,float y,float xx,float yy){draw->AddLine({p.x+x,p.y+y},{p.x+xx,p.y+yy},IM_COL32(0,0,0,255),4);draw->AddLine({p.x+x,p.y+y},{p.x+xx,p.y+yy},io.MouseDown[0]?IM_COL32(214,118,148,255):IM_COL32(239,234,220,255),2);};
        stroke(-12,0,-5,0);stroke(5,0,12,0);stroke(0,-12,0,-5);stroke(0,5,0,12);
        draw->AddRectFilled({p.x-1,p.y-1},{p.x+2,p.y+2},IM_COL32(239,234,220,255));
        if(dragPin){stroke(-8,-8,-5,-8);stroke(5,8,8,8);}
    }
