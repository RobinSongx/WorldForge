#include "ui/StudioPanels.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace wf {
namespace {
const ImVec4 accent{0.87f,0.65f,0.34f,1.0f};
const ImVec4 muted{0.53f,0.60f,0.65f,1.0f};
void label(const char* text){ImGui::TextColored(muted,"%s",text);}
void resetDock(ImGuiID id,const ImVec2& size){
    ImGui::DockBuilderRemoveNode(id);ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id,size);
    auto center=id;ImGuiID left{},right{},bottom{},bottomRight{};
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,.18f,&left,&center);
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.25f,&right,&center);
    ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.25f,&bottom,&center);
    ImGui::DockBuilderSplitNode(bottom,ImGuiDir_Right,.40f,&bottomRight,&bottom);
    ImGui::DockBuilderDockWindow("Scene",left);ImGui::DockBuilderDockWindow("Inspector",right);
    ImGui::DockBuilderDockWindow("Viewport",center);ImGui::DockBuilderDockWindow("Console",bottom);
    ImGui::DockBuilderDockWindow("Assets",bottom);ImGui::DockBuilderDockWindow("Performance",bottomRight);
    ImGui::DockBuilderFinish(id);
}
void header(EditorState& editor,PanelState& panels){
    if(!ImGui::BeginMainMenuBar())return;
    ImGui::TextColored(accent,"W O R L D F O R G E");ImGui::Dummy({14,0});
    if(ImGui::BeginMenu("File")){
        if(ImGui::MenuItem("Exit"))panels.quitRequested=true;
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Scene")){
        if(ImGui::MenuItem("Add cube"))editor.addCube();
        if(ImGui::MenuItem("Delete selected",nullptr,false,editor.selected()!=nullptr))editor.removeSelected();
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("View")){
        ImGui::MenuItem("Scene",nullptr,&panels.hierarchy);ImGui::MenuItem("Inspector",nullptr,&panels.inspector);
        ImGui::MenuItem("Console",nullptr,&panels.console);ImGui::MenuItem("Assets",nullptr,&panels.assets);
        ImGui::MenuItem("Performance",nullptr,&panels.performance);
        if(ImGui::MenuItem("Reset layout"))panels.resetLayout=true;
        ImGui::EndMenu();
    }
    const char* status="WINDOWS / FRAMEWORK PREVIEW";
    const float statusWidth=ImGui::CalcTextSize(status).x;
    if(ImGui::GetWindowWidth()>ImGui::GetCursorPosX()+statusWidth+30){
        ImGui::SameLine(ImGui::GetWindowWidth()-statusWidth-40);label(status);
    }
    ImGui::EndMainMenuBar();
}
}
void applyTheme(float scale){
    ImGui::GetStyle()=ImGuiStyle{};
    ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
    s.FontSizeBase=14.0f;
    s.WindowRounding=4;s.FrameRounding=3;s.GrabRounding=3;s.TabRounding=3;
    s.WindowBorderSize=1;s.FrameBorderSize=0;s.WindowPadding={14,12};s.FramePadding={8,5};s.ItemSpacing={8,8};
    auto* c=s.Colors;
    c[ImGuiCol_WindowBg]={.068f,.079f,.092f,1};c[ImGuiCol_ChildBg]=c[ImGuiCol_WindowBg];
    c[ImGuiCol_PopupBg]={.09f,.105f,.12f,1};c[ImGuiCol_Border]={.16f,.19f,.22f,1};
    c[ImGuiCol_TitleBg]={.075f,.09f,.10f,1};c[ImGuiCol_TitleBgActive]={.12f,.14f,.16f,1};
    c[ImGuiCol_MenuBarBg]={.048f,.059f,.07f,1};
    c[ImGuiCol_Header]={.18f,.22f,.25f,1};c[ImGuiCol_HeaderHovered]={.25f,.29f,.31f,1};c[ImGuiCol_HeaderActive]={.32f,.28f,.21f,1};
    c[ImGuiCol_FrameBg]={.115f,.14f,.16f,1};c[ImGuiCol_FrameBgHovered]={.19f,.23f,.26f,1};
    c[ImGuiCol_Button]={.19f,.23f,.26f,1};c[ImGuiCol_ButtonHovered]={.32f,.29f,.23f,1};c[ImGuiCol_ButtonActive]={.43f,.33f,.20f,1};
    c[ImGuiCol_CheckMark]=accent;c[ImGuiCol_SliderGrab]=accent;c[ImGuiCol_SliderGrabActive]=accent;
    c[ImGuiCol_Tab]={.09f,.11f,.13f,1};c[ImGuiCol_TabSelected]={.17f,.20f,.23f,1};c[ImGuiCol_TabHovered]={.30f,.26f,.20f,1};
    c[ImGuiCol_DockingPreview]={.87f,.65f,.34f,.3f};c[ImGuiCol_Text]={.88f,.91f,.93f,1};c[ImGuiCol_TextDisabled]=muted;
    s.ScaleAllSizes(scale);s.FontScaleDpi=scale;
}
void drawStudio(EditorState& editor,ViewportRenderer& renderer,PanelState& p,const std::string& gpu){
    header(editor,p);
    auto* main=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(main->WorkPos);ImGui::SetNextWindowSize(main->WorkSize);ImGui::SetNextWindowViewport(main->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("StudioDock",nullptr,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoNavFocus);
    ImGui::PopStyleVar(3);const auto dockId=ImGui::GetID("WorldForgeDock");
    if(!ImGui::DockBuilderGetNode(dockId)||p.resetLayout){resetDock(dockId,main->WorkSize);p.resetLayout=false;p.hierarchy=p.inspector=p.console=p.performance=p.assets=true;}
    ImGui::DockSpace(dockId,ImVec2(0,0),ImGuiDockNodeFlags_None);ImGui::End();p.panelsDrawn=0;

    if(p.hierarchy){
        const bool shown=ImGui::Begin("Scene",&p.hierarchy);++p.panelsDrawn;
        if(shown){
            label("UNTITLED WORLD");ImGui::Separator();
            if(ImGui::Button("+ Cube"))editor.addCube();ImGui::SameLine();
            if(ImGui::Button("New"))ImGui::OpenPopup("New scene?");
            if(ImGui::BeginPopupModal("New scene?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                ImGui::TextUnformatted("Discard this in-memory scene?\nProject saving arrives in the Mesh milestone.");
                if(ImGui::Button("Discard and create")){editor.newScene();ImGui::CloseCurrentPopup();}ImGui::SameLine();
                if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
            }
            ImGui::Spacing();
            for(const auto& e:editor.entities()){
                ImGui::PushID(static_cast<int>(e.id));
                if(ImGui::Selectable(e.name.c_str(),editor.selection()==e.id))editor.select(e.id);
                ImGui::PopID();
            }
            ImGui::Spacing();ImGui::Separator();label("PROCEDURAL CONTENT");
            ImGui::TextWrapped("%zu entities\nSelect an object to edit its transform.",editor.entities().size());
        }ImGui::End();
    }
    if(p.inspector){
        const bool shown=ImGui::Begin("Inspector",&p.inspector);++p.panelsDrawn;
        if(shown){
            if(auto* e=editor.selected()){
                label("SELECTED OBJECT");ImGui::TextColored(accent,"%s",e->name.c_str());ImGui::Separator();
                ImGui::Checkbox("Visible",&e->visible);
                if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
                    auto t=e->transform;label("Position");ImGui::SetNextItemWidth(-1);
                    bool changed=ImGui::DragFloat3("##position",glm::value_ptr(t.position),.02f);
                    label("Rotation / degrees");ImGui::SetNextItemWidth(-1);changed|=ImGui::DragFloat3("##rotation",glm::value_ptr(t.rotationDegrees),.5f);
                    label("Scale");ImGui::SetNextItemWidth(-1);changed|=ImGui::DragFloat3("##scale",glm::value_ptr(t.scale),.01f,.01f,100.0f);
                    if(changed&&!editor.setTransform(e->id,t))editor.log("WARN","Rejected invalid transform");
                    if(ImGui::Button("Reset transform")){Transform reset;reset.position.y=.5f;editor.setTransform(e->id,reset);}
                }
                if(ImGui::CollapsingHeader("Appearance",ImGuiTreeNodeFlags_DefaultOpen))ImGui::ColorEdit3("Tint",glm::value_ptr(e->color),ImGuiColorEditFlags_NoInputs);
                if(ImGui::CollapsingHeader("Geometry",ImGuiTreeNodeFlags_DefaultOpen)){ImGui::TextUnformatted("Built-in cube\n12 triangles / 36 vertices\nPreview diffuse shader");}
                ImGui::Spacing();if(ImGui::Button("Frame selected"))editor.camera.frame(*e);
                if(ImGui::Button("Delete selected"))editor.removeSelected();
            }else{label("NO SELECTION");ImGui::TextWrapped("Choose an object in the Scene panel.");}
        }ImGui::End();
    }
    {
        const bool shown=ImGui::Begin("Viewport",nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);++p.panelsDrawn;
        if(shown){
            ImGui::TextColored(accent,"PERSPECTIVE");ImGui::SameLine();ImGui::Checkbox("Grid",&editor.grid);ImGui::SameLine();ImGui::Checkbox("Wireframe",&editor.wireframe);
            ImGui::SameLine();if(ImGui::Button("Frame")&&editor.selected())editor.camera.frame(*editor.selected());
            ImGui::SameLine();if(ImGui::Button("Reset camera"))editor.camera=OrbitCamera{};
            const auto size=ImGui::GetContentRegionAvail();
            if(size.x>1&&size.y>1){
                const auto origin=ImGui::GetCursorScreenPos();p.imageX=origin.x;p.imageY=origin.y;const auto& io=ImGui::GetIO();
                p.viewportWidth=std::max(1,static_cast<int>(size.x*io.DisplayFramebufferScale.x));p.viewportHeight=std::max(1,static_cast<int>(size.y*io.DisplayFramebufferScale.y));
                // Image refers to a stable texture until render() resizes it: draw after render.
                renderer.render(editor,p.viewportWidth,p.viewportHeight);
                ImGui::Image(static_cast<ImTextureID>(renderer.textureToken()),size,ImVec2(0,1),ImVec2(1,0));
                if(ImGui::IsItemHovered()){
                    if(ImGui::IsMouseDragging(ImGuiMouseButton_Right))editor.camera.orbit(io.MouseDelta.x,io.MouseDelta.y);
                    if(ImGui::IsMouseDragging(ImGuiMouseButton_Middle))editor.camera.pan(io.MouseDelta.x,io.MouseDelta.y);
                    if(io.MouseWheel!=0)editor.camera.zoom(io.MouseWheel);
                    if(!io.WantTextInput&&ImGui::IsKeyPressed(ImGuiKey_F)&&editor.selected())editor.camera.frame(*editor.selected());
                }
                auto* draw=ImGui::GetWindowDrawList();
                draw->AddText(ImVec2(origin.x+14,origin.y+14),IM_COL32(155,175,184,255),"WORLD / 01");
                draw->AddText(ImVec2(origin.x+14,origin.y+size.y-26),IM_COL32(155,175,184,255),"RMB orbit   MMB pan   Wheel zoom   F frame");
                draw->AddCircleFilled(ImVec2(origin.x+size.x-20,origin.y+20),4,IM_COL32(114,193,153,255));
            }
        }ImGui::End();
    }
    if(p.console){
        const bool shown=ImGui::Begin("Console",&p.console);++p.panelsDrawn;
        if(shown){label("SESSION LOG");ImGui::Separator();ImGui::BeginChild("LogScroll",ImVec2(0,0));
            for(const auto& log:editor.logs()){ImGui::TextColored(log.level=="WARN"?accent:muted,"[%s]",log.level.c_str());ImGui::SameLine();ImGui::TextWrapped("%s",log.message.c_str());}
            ImGui::EndChild();
        }ImGui::End();
    }
    if(p.assets){
        const bool shown=ImGui::Begin("Assets",&p.assets);++p.panelsDrawn;
        if(shown){label("BUILT-IN PRIMITIVES");if(ImGui::Button("Cube\nAdd to scene",ImVec2(130,65)))editor.addCube();ImGui::SameLine();ImGui::TextWrapped("File import and asset processing\nare planned for the Mesh milestone.");}ImGui::End();
    }
    p.frameTimes[p.frameIndex++%p.frameTimes.size()]=ImGui::GetIO().DeltaTime*1000;
    if(p.performance){
        const bool shown=ImGui::Begin("Performance",&p.performance);++p.panelsDrawn;
        if(shown){
            ImGui::TextColored(accent,"%.1f FPS",ImGui::GetIO().Framerate);label("CPU FRAME INTERVAL");
            ImGui::PlotLines("##frames",p.frameTimes.data(),static_cast<int>(p.frameTimes.size()),static_cast<int>(p.frameIndex%p.frameTimes.size()),nullptr,0,50,ImVec2(-1,45));
            ImGui::Text("Viewport %d x %d",renderer.width(),renderer.height());ImGui::SameLine();ImGui::Text("/ %u draws",renderer.draws());
            ImGui::TextWrapped("%s",gpu.c_str());label("GPU timing is not implemented in this preview.");
        }ImGui::End();
    }
}
}
