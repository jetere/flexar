// Exercised only by --smoke-test. Validates actual shader pixels against scalar
// blend equations, including partial alpha and a branch with a two-pass blur.
static void validateBlendPixels(Renderer& renderer){
    Graph g;std::string error;auto source=g.add(Effect::Source),a=g.add(Effect::Grade),blur=g.add(Effect::Blur),b=g.add(Effect::Grade),blend=g.add(Effect::Blend),output=g.add(Effect::Output);
    auto connect=[&](Id from,Id to,size_t index=0){if(!g.connect(g.find(from)->outputs[0].id,g.find(to)->inputs[index].id,error))throw std::runtime_error(error);};
    connect(source,a);connect(a,blur);connect(source,b);connect(blur,blend);connect(b,blend,1);connect(blend,output);
    g.find(a)->parameters[0].value=-1;g.find(b)->parameters[0].value=1;
    Image solid{32,32,std::vector<uint8_t>(32*32*4)};
    for(size_t p=0;p<solid.pixels.size();p+=4){solid.pixels[p]=100;solid.pixels[p+1]=140;solid.pixels[p+2]=200;solid.pixels[p+3]=128;}
    auto equation=[](float a,float b,int mode){switch(mode){
        case 1:return a*b;case 2:return 1-(1-a)*(1-b);case 3:return a<.5f?2*a*b:1-2*(1-a)*(1-b);
        case 4:{float d=a<=.25f?((16*a-12)*a+4)*a:std::sqrt(a);return b<=.5f?a-(1-2*b)*a*(1-a):a+(2*b-1)*(d-a);}
        case 5:return b<.5f?2*a*b:1-2*(1-a)*(1-b);case 6:return std::abs(a-b);case 7:return a+b-2*a*b;
        case 8:return std::min(1.f,a+b);case 9:return std::max(0.f,a-b);case 10:return std::min(a,b);case 11:return std::max(a,b);default:return b;
    }};
    for(int mode=0;mode<12;++mode){
        g.find(blend)->parameters[0].value=static_cast<float>(mode);g.find(blend)->parameters[1].value=.65f;
        renderer.begin();if(mode==0)renderer.upload(solid);
        auto handle=renderer.evaluate(g,0,false);
        ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();
        auto image=renderer.readback(handle);float ab=128.f/255,as=ab*.65f,alpha=as+ab*(1-as);
        for(int channel=0;channel<3;++channel){float base=solid.pixels[channel]*.5f/255,layer=std::min(255.f,solid.pixels[channel]*2.f)/255;
            float expected=((1-as)*ab*base+(1-ab)*as*layer+ab*as*equation(base,layer,mode))/alpha;
            if(std::abs(image.pixels[channel]-expected*255)>2)throw std::runtime_error("GPU blend pixel mismatch: mode "+std::to_string(mode));
        }
        if(std::abs(image.pixels[3]-alpha*255)>2)throw std::runtime_error("GPU blend alpha mismatch");
    }
    g.find(blend)->enabled=false;g.disconnect(g.find(blend)->inputs[1].id);
    renderer.begin();auto handle=renderer.evaluate(g,0,false);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();
    auto bypass=renderer.readback(handle);if(std::abs(int(bypass.pixels[0])-50)>1||bypass.pixels[3]!=128)throw std::runtime_error("Blend bypass should return A even if B is disconnected");
    g.find(blend)->enabled=true;renderer.begin();handle=renderer.evaluate(g,0,false);if(handle)throw std::runtime_error("Missing blend B must disconnect output");
    ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();
}
