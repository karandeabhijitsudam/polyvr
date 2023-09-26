#include "VRImguiApps.h"

#include "core/utils/toString.h"
#include "core/gui/VRGuiManager.h"

ImAppLauncher::ImAppLauncher(string ID, string pnl, string ts) : ID(ID), name(ID), panel(pnl), timestamp(ts) {}

void ImAppLauncher::render(string filter) {
    if (filter != "") {
        if (!contains(name, filter, false)) return;
    }

    string label = name;
    if (label.length() > 25) label = ".." + subString(label, label.length()-23, 23);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;

    if (!sensitive) ImGui::BeginDisabled();

    ImGui::BeginGroup();
        ImGui::Spacing();
        ImGui::Indent(5.0);
        ImGui::Columns(2);
            if (ImGui::CollapsingHeader(("advanced##"+ID).c_str(), flags) && sensitive) {
                if (ImGui::Button(("Run without scripts##"+ID).c_str())) uiSignal("on_toggle_app_no_scripts", {{"ID",ID}});
            }
            ImGui::NextColumn();
            ImGui::Text(label.c_str());
            ImGui::SameLine();
            if (!running) {
                if (ImGui::Button(("Run##"+ID).c_str())) uiSignal("on_toggle_app", {{"ID",ID}});
            } else {
                if (ImGui::Button(("Stop##"+ID).c_str())) uiSignal("on_toggle_app", {{"ID",ID}});
            }
        ImGui::Columns(1);
        ImGui::Spacing();
    ImGui::EndGroup();

    ImVec2 p1 = ImGui::GetItemRectMin();
    ImVec2 p2 = ImGui::GetItemRectMax();
    p2.x = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddRect(p1, p2, IM_COL32(255, 255, 255, 255));

    if (!sensitive) ImGui::EndDisabled();
}

ImAppPanel::ImAppPanel(string lbl) : label(lbl) {
    ;
}

void ImAppPanel::render(string filter, map<string, ImAppLauncher>& launcherPool) {
    if (filter != "") {
        bool anyVisible = false;
        for (auto& lID : launchers) {
            auto& l = launcherPool[lID];
            if (contains(l.name, filter, false)) anyVisible = true;
        }
        if (!anyVisible) return;
    }

    if (label.size() > 0) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text(label.c_str());
        //ImGui::SameLine();
        ImGui::Separator();
        ImGui::Spacing();
    }

    for (auto& lID : launchers) {
        auto& l = launcherPool[lID];
        l.render(filter);
    }
}

ImAppManager::ImAppManager() : ImWidget("AppManager"), examples("") {
    auto mgr = OSG::VRGuiSignals::get();
    mgr->addCallback("newAppLauncher", [&](OSG::VRGuiSignals::Options o){ newAppLauncher(o["panel"], o["ID"], o["timestamp"]); return true; } );
    mgr->addCallback("setupAppLauncher", [&](OSG::VRGuiSignals::Options o){ setupAppLauncher(o["ID"], o["name"]); return true; } );
    mgr->addCallback("setAppLauncherState", [&](OSG::VRGuiSignals::Options o){ setAppLauncherState(o["ID"], toBool(o["running"]), toBool(o["sensitive"])); return true; } );
}

void getDurationString(size_t d, string& dLabel, size_t& dLabelI) {
    if (d == 0) { dLabel = "long ago"; dLabelI = 3600*24*356*2; return; }
    if (d < 3600) { dLabel = "last hour"; dLabelI = 3600; return; }
    if (d < 3600*24) { dLabel = "today"; dLabelI = 3600*24; return; }
    if (d < 3600*24*2) { dLabel = "yesterday"; dLabelI = 3600*24*2; return; }
    if (d < 3600*24*7) { dLabel = "last week"; dLabelI = 3600*24*7; return; }
    if (d < 3600*24*30) { dLabel = "last month"; dLabelI = 3600*24*30; return; }
    if (d < 3600*24*356) { dLabel = "last year"; dLabelI = 3600*24*356; return; }
    { dLabel = "long ago"; dLabelI = 3600*24*356*2; return; }
}

void ImAppManager::updatePannels() {
    projects.clear();
    map<string, vector<string>> panelToLaunchers;
    map<size_t, string> panels;

    time_t tnow = time(0);
    auto now = localtime(&tnow);

    for (auto& l : launchers) {
        if (l.second.panel == "examples") continue;

        time_t tl = toValue<size_t>(l.second.timestamp);
        time_t delta = 0;
        if (tl > 0) delta = tnow-tl;
        string dLabel;
        size_t dLabelI;
        getDurationString(delta, dLabel, dLabelI);

        if (!panelToLaunchers.count(dLabel)) {
            panels[dLabelI] = dLabel;
            panelToLaunchers[dLabel] = vector<string>();
        }
        panelToLaunchers[dLabel].push_back(l.first);
    }

    for (auto& p : panels) {
        projects.push_back(ImAppPanel(p.second));
        auto& panel = projects[projects.size()-1];
        for (auto& l : panelToLaunchers[p.second]) {
            panel.launchers.push_back(l);
        }
    }
}

void ImAppManager::newAppLauncher(string panel, string ID, string timestamp) {
    launchers[ID] = ImAppLauncher(ID, panel, timestamp);
    if (panel == "examples") examples.launchers.push_back(ID);
    else updatePannels();
}

void ImAppManager::setAppLauncherState(string ID, bool running, bool sensitive) {
    if (!launchers.count(ID)) return;
    auto& l = launchers[ID];
    l.running = running;
    l.sensitive = sensitive;
}

void ImAppManager::setupAppLauncher(string ID, string name) {
    if (!launchers.count(ID)) return;
    auto& l = launchers[ID];
    l.name = name;
}

void ImAppManager::begin() {
    static char str0[128] = "";
    memcpy(str0, filter.c_str(), filter.size());
    str0[filter.size()] = 0;

    ImGui::Text("filter:");
    ImGui::SameLine();
    if (ImGui::InputText("##AppFilter", str0, 128) ) {
        filter = string(str0);
        uiSignal("on_change_app_filter", {{"filter",filter}});
    }

    if (ImGui::BeginTabBar("AppPanelsTabBar", ImGuiTabBarFlags_None)) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_None;

        if (ImGui::BeginTabItem("Projects")) {
            ImGui::Spacing();
            ImGui::BeginChild("Panel1", ImGui::GetContentRegionAvail(), false, flags);
            for (auto panel : projects) panel.render(filter, launchers);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Examples")) {
            ImGui::Spacing();
            ImGui::BeginChild("Panel2", ImGui::GetContentRegionAvail(), false, flags);
            examples.render(filter, launchers);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}
