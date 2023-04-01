#include "VRImguiScene.h"


#include "core/utils/toString.h"
#include "core/gui/VRGuiManager.h"

ImSceneEditor::ImSceneEditor() : ImWidget("SceneEditor") {
    auto mgr = OSG::VRGuiSignals::get();
    //mgr->addCallback("newAppLauncher", [&](OSG::VRGuiSignals::Options o){ newAppLauncher(o["panel"], o["ID"]); return true; } );
}

void ImSceneEditor::begin() {
    if (ImGui::BeginTabBar("AppPanelsTabBar", ImGuiTabBarFlags_None)) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_None;

        if (ImGui::BeginTabItem("Rendering")) {
            ImGui::Spacing();
            ImGui::BeginChild("RenderingPanel", ImGui::GetContentRegionAvail(), false, flags);
            rendering.render();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scenegraph")) {
            ImGui::Spacing();
            ImGui::BeginChild("ScenegraphPanel", ImGui::GetContentRegionAvail(), false, flags);
            scenegraph.render();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Scripting")) {
            scripting.render();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Navigation")) {
            ImGui::Spacing();
            ImGui::BeginChild("NavigationPanel", ImGui::GetContentRegionAvail(), false, flags);
            navigation.render();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Semantics")) {
            ImGui::Spacing();
            ImGui::BeginChild("SemanticsPanel", ImGui::GetContentRegionAvail(), false, flags);
            semantics.render();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Network")) {
            ImGui::Spacing();
            ImGui::BeginChild("NetworkPanel", ImGui::GetContentRegionAvail(), false, flags);
            network.render();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}
