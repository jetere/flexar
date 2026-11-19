// Regression for returning to the graph while floating editors are already open.
#include <imgui_internal.h>
static void validateEditorWindowFocus(Renderer& renderer,HWND hwnd,const std::filesystem::path& root){
    Editor editor;editor.runtime=&renderer;editor.hwnd=hwnd;editor.root=root;editor.image=demoImage();editor.hasMedia=true;editor.applyPreset(0);editor.addEffect(Effect::Compositor);Id compositor=editor.selected;Id output=0;for(auto& n:editor.graph.nodes)if(n.effect==Effect::Output)output=n.id;
    auto frame=[&]{renderer.begin();if(editor.uploadPending){renderer.upload(editor.image);editor.uploadPending=false;}editor.syncSources(renderer);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();editor.beginHistoryFrame();editor.ui(renderer);editor.commitHistoryFrame();if(ImGui::GetCurrentContext()->DisabledStackSize!=0)throw std::runtime_error("Floating editor leaked disabled state");ImGui::Render();renderer.present();};
    auto mutate=[&](auto action){renderer.begin();if(editor.uploadPending){renderer.upload(editor.image);editor.uploadPending=false;}editor.syncSources(renderer);ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();editor.beginHistoryFrame();action();editor.commitHistoryFrame();ImGui::Render();renderer.present();};
    auto window=[](const char* name){auto w=ImGui::FindWindowByName(name);if(!w)throw std::runtime_error(std::string("Missing editor window: ")+name);return w;};
    auto focused=[&](const char* name){auto nav=ImGui::GetCurrentContext()->NavWindow;if(!nav||nav->RootWindow!=window(name))throw std::runtime_error(std::string("Editor did not regain focus: ")+name);};
    auto above=[&](const char* name){int workspace=-1,floating=-1;auto& windows=ImGui::GetCurrentContext()->Windows;for(int i=0;i<windows.Size;++i){if(windows[i]==window("flexar"))workspace=i;if(windows[i]==window(name))floating=i;}if(floating<=workspace)throw std::runtime_error(std::string("Workspace covered floating editor: ")+name);};
    editor.openPreview();editor.openComposition(compositor);frame();
    for(int i=0;i<3;++i){
        ImGui::FocusWindow(window("flexar"));above("Preview player");above("Composition studio");frame();
        editor.openPreview();frame();focused("Preview player");
        editor.openComposition(compositor);frame();focused("Composition studio");
        editor.openControls(output);frame();focused("Node Controls");
        auto controls=window("Node Controls");ImGui::SetWindowPos(controls,{90,260},ImGuiCond_Always);editor.openControls(output);frame();if(controls->Pos.x!=90||controls->Pos.y!=260)throw std::runtime_error("Refocusing controls unexpectedly moved them");
        editor.largeFullscreen=true;editor.largePreview=false;editor.showComposition=false;editor.showControls=false;frame();
        editor.openPreview();frame();focused("Preview player");if(editor.largeFullscreen)throw std::runtime_error("Reopened preview retained fullscreen lock");
        editor.openComposition(compositor);frame();focused("Composition studio");
    }
    editor.largePreview=false;editor.showComposition=false;editor.showControls=false;frame();
    auto key=[&](ImGuiKey key,bool down){ImGui::GetIO().AddKeyEvent(key,down);frame();};
    editor.openComposition(compositor);editor.studioTool=1;frame();if(editor.studioTool!=0)throw std::runtime_error("Disabled pen tool became active");
    key(ImGuiKey_P,true);key(ImGuiKey_P,false);if(editor.studioTool!=0)throw std::runtime_error("P shortcut activated the disabled pen tool");
    editor.openControls(compositor);frame();key(ImGuiKey_Delete,true);if(editor.graph.find(compositor)||editor.selected||editor.showComposition)throw std::runtime_error("Delete did not remove compositor and clear selection");key(ImGuiKey_Delete,false);
    auto extra=editor.graph.add(Effect::Source);editor.clips[extra].image=demoImage();editor.openControls(extra);frame();key(ImGuiKey_Backspace,true);if(editor.graph.find(extra)||editor.clips.contains(extra))throw std::runtime_error("Backspace did not remove source media state");key(ImGuiKey_Backspace,false);
    editor.openControls(output);frame();key(ImGuiKey_Delete,true);if(!editor.graph.find(output))throw std::runtime_error("Output must remain protected");key(ImGuiKey_Delete,false);
    auto primary=editor.primarySource();editor.openControls(primary);frame();key(ImGuiKey_Backspace,true);if(!editor.graph.find(primary))throw std::runtime_error("Primary source must remain protected");key(ImGuiKey_Backspace,false);
    editor.addEffect(Effect::Grade);auto effect=editor.selected;editor.openPreview();frame();key(ImGuiKey_Delete,true);if(!editor.graph.find(effect))throw std::runtime_error("Preview focus must not delete graph nodes");key(ImGuiKey_Delete,false);
    editor.openControls(effect);frame();ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,true);key(ImGuiKey_Backspace,true);if(!editor.graph.find(effect))throw std::runtime_error("Modified editing shortcut deleted a node");key(ImGuiKey_Backspace,false);ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,false);frame();
    size_t nodeCount=editor.graph.nodes.size();mutate([&]{editor.addEffect(Effect::Grade);});
    if(!editor.canUndo()||editor.graph.nodes.size()!=nodeCount+1)throw std::runtime_error("Adding a node did not create an undo snapshot");
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,true);key(ImGuiKey_Z,true);key(ImGuiKey_Z,false);ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,false);frame();
    if(editor.graph.nodes.size()!=nodeCount||!editor.canRedo())throw std::runtime_error("Ctrl+Z did not undo the added node");
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,true);key(ImGuiKey_Y,true);key(ImGuiKey_Y,false);ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,false);frame();
    if(editor.graph.nodes.size()!=nodeCount+1)throw std::runtime_error("Ctrl+Y did not redo the added node");
    for(auto& n:editor.graph.nodes)if(n.effect==Effect::Grade){effect=n.id;break;}
    // Exercise the guard with ImGui's live text-edit state and an actual key press.
    renderer.begin();ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();ImGui::GetIO().WantTextInput=true;editor.nodeDeletionFocus=true;ImGui::GetIO().KeysData[ImGuiKey_Delete-ImGuiKey_NamedKey_BEGIN].Down=true;ImGui::GetIO().KeysData[ImGuiKey_Delete-ImGuiKey_NamedKey_BEGIN].DownDuration=0;editor.deleteNodeShortcut();if(!editor.graph.find(effect))throw std::runtime_error("Typing guard deleted a node");ImGui::GetIO().KeysData[ImGuiKey_Delete-ImGuiKey_NamedKey_BEGIN].Down=false;ImGui::GetIO().WantTextInput=false;ImGui::Render();renderer.present();
    editor.largePreview=false;editor.showControls=false;frame();
}
