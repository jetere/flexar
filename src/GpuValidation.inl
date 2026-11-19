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

static void validateCustomAndSources(Renderer& renderer,const std::filesystem::path& root){
    Graph g;auto a=g.add(Effect::Source),b=g.add(Effect::Source),blend=g.add(Effect::Blend),out=g.add(Effect::Output);std::string error;
    g.connect(g.find(a)->outputs[0].id,g.find(blend)->inputs[0].id,error);g.connect(g.find(b)->outputs[0].id,g.find(blend)->inputs[1].id,error);g.connect(g.find(blend)->outputs[0].id,g.find(out)->inputs[0].id,error);
    Image red{8,8,std::vector<uint8_t>(8*8*4)},blue{4,4,std::vector<uint8_t>(4*4*4)};
    for(size_t i=0;i<red.pixels.size();i+=4){red.pixels[i]=255;red.pixels[i+3]=255;}for(size_t i=0;i<blue.pixels.size();i+=4){blue.pixels[i+2]=255;blue.pixels[i+3]=255;}
    g.find(blend)->parameters[0].value=0;g.find(blend)->parameters[1].value=.5f;
    auto present=[&]{ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();};
    renderer.begin();renderer.upload(red);renderer.clearSources();renderer.bindPrimary(a);renderer.uploadSource(b,blue);auto handle=renderer.evaluate(g,0,false);present();auto result=renderer.readback(handle);
    if(std::abs(int(result.pixels[0])-128)>1||std::abs(int(result.pixels[2])-128)>1||result.pixels[3]!=255)throw std::runtime_error("Independent source compositing failed");
    for(auto& file:std::filesystem::directory_iterator(root/"shaders/godot"))if(file.path().extension()==".gdshader"){
        Graph custom;auto src=custom.add(Effect::Source),fx=custom.add(Effect::Custom),dst=custom.add(Effect::Output);std::ifstream in(file.path());custom.find(fx)->shaderCode=std::string(std::istreambuf_iterator<char>(in),{});renderer.compileShader(*custom.find(fx));custom.connect(custom.find(src)->outputs[0].id,custom.find(fx)->inputs[0].id,error);custom.connect(custom.find(fx)->outputs[0].id,custom.find(dst)->inputs[0].id,error);
        renderer.begin();renderer.clearSources();renderer.upload(red);auto output=renderer.evaluate(custom,1,false);present();auto pixels=renderer.readback(output);if(pixels.pixels[3]!=255||(file.path().stem().string().starts_with("x0")&&(pixels.pixels[0]<220||pixels.pixels[2]!=0)))throw std::runtime_error("Bundled shader pixel validation failed");
    }
    Node invalid;invalid.shaderCode="shader_type spatial; void fragment() {}";bool rejected=false;try{renderer.compileShader(invalid);}catch(...){rejected=true;}if(!rejected)throw std::runtime_error("Unsupported shader accepted");renderer.clearSources();
}

static void validateCompositor(Renderer& renderer){
    Graph graph;auto red=graph.add(Effect::Source),blue=graph.add(Effect::Source),comp=graph.add(Effect::Compositor),output=graph.add(Effect::Output);std::string error;
    graph.connect(graph.find(red)->outputs[0].id,graph.find(comp)->inputs[0].id,error);graph.connect(graph.find(blue)->outputs[0].id,graph.find(comp)->inputs[1].id,error);graph.connect(graph.find(comp)->outputs[0].id,graph.find(output)->inputs[0].id,error);
    Image a{32,32,std::vector<uint8_t>(32*32*4)},b=a;for(size_t i=0;i<a.pixels.size();i+=4){a.pixels[i]=255;a.pixels[i+3]=255;b.pixels[i+2]=255;b.pixels[i+3]=255;}
    auto& layers=graph.find(comp)->layers;layers[1].scaleX=.5;layers[1].scaleY=.5;layers[1].opacity=.5;
    auto frame=[&](bool upload){renderer.begin();if(upload){renderer.upload(a);renderer.clearSources();renderer.bindPrimary(red);renderer.uploadSource(blue,b);}auto handle=renderer.evaluate(graph,0,false);renderer.evaluate(graph,0,false,comp);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();return renderer.readback(handle);};
    auto result=frame(true);auto center=(16*32+16)*4;
    if(std::abs(int(result.pixels[center])-128)>1||std::abs(int(result.pixels[center+2])-128)>1||result.pixels[0]!=255||result.pixels[2]!=0)throw std::runtime_error("Compositor scale/opacity or retained preview failed");
    layers[0].enabled=false;result=frame(false);if(result.pixels[3]!=0||std::abs(int(result.pixels[center+3])-128)>1||result.pixels[center+2]!=255)throw std::runtime_error("Compositor transparent background failed");
    layers[1].x=.5f;result=frame(false);if(result.pixels[center+3]!=0)throw std::runtime_error("Compositor translation failed");
    layers[1].x=0;layers[1].cropLeft=.6f;result=frame(false);if(result.pixels[center+3]!=0)throw std::runtime_error("Compositor crop failed");
    // Vector mask pass: a left-half alpha mask must remove the right half.
    layers[1].cropLeft=0;layers[1].scaleX=1;layers[1].scaleY=1;layers[1].opacity=1;layers[1].x=0;layers[1].y=0;layers[0].enabled=false;
    layers[1].path={{0,0},{.5f,0},{.5f,1},{0,1}};layers[1].maskFeather=0;layers[1].maskEnabled=true;layers[1].maskInvert=false;
    Image mask{32,32,std::vector<uint8_t>(32*32*4,255)};for(uint32_t y=0;y<32;++y)for(uint32_t x=16;x<32;++x){auto* p=mask.pixels.data()+size_t(y*32+x)*4;p[0]=p[1]=p[2]=0;}
    renderer.begin();renderer.uploadMask(compositionMaskKey(comp,1),mask);auto masked=renderer.evaluate(graph,0,false);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();auto maskedPixels=renderer.readback(masked);
    if(maskedPixels.pixels[(16*32+8)*4+3]!=255||maskedPixels.pixels[(16*32+24)*4+3]!=0)throw std::runtime_error("Compositor vector mask failed");
    layers[1].maskInvert=true;renderer.begin();renderer.uploadMask(compositionMaskKey(comp,1),[&]{Image inverted=mask;for(size_t i=0;i<inverted.pixels.size();i+=4){inverted.pixels[i]=inverted.pixels[i+1]=inverted.pixels[i+2]=255-inverted.pixels[i];}return inverted;}());masked=renderer.evaluate(graph,0,false);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();maskedPixels=renderer.readback(masked);
    if(maskedPixels.pixels[(16*32+8)*4+3]!=0||maskedPixels.pixels[(16*32+24)*4+3]!=255)throw std::runtime_error("Compositor inverted mask failed");
    // A source registered after a mask must receive its own persistent slot.
    renderer.begin();renderer.uploadSource(999,b);masked=renderer.evaluate(graph,1.f,false);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::Render();renderer.present();maskedPixels=renderer.readback(masked);
    if(maskedPixels.pixels[(16*32+8)*4+3]!=0||maskedPixels.pixels[(16*32+24)*4+3]!=255)throw std::runtime_error("Late source upload overwrote a mask texture");
    layers[1].enabled=false;result=frame(false);for(auto value:result.pixels)if(value!=0)throw std::runtime_error("Empty composition must be transparent");renderer.clearSources();
}
