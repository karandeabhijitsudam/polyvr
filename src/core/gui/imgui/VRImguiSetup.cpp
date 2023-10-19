#include "VRImguiSetup.h"

#include "core/utils/toString.h"
#include "core/gui/VRGuiManager.h"
#include "core/gui/imgui/imWidgets/VRImguiInput.h"

ImSetupManager::ImSetupManager() : ImWidget("SetupManager"), tree("setup"), NxNy("NxNy", "NxNy") {
    auto mgr = OSG::VRGuiSignals::get();
    mgr->addCallback("updateSetupsList", [&](OSG::VRGuiSignals::Options o){ updateSetupsList(o["setups"]); return true; } );
    mgr->addCallback("setCurrentSetup", [&](OSG::VRGuiSignals::Options o){ current_setup = toInt(o["setup"]); return true; } );
    mgr->addCallback("on_setup_tree_clear", [&](OSG::VRGuiSignals::Options o){ tree.clear(); return true; } );
    mgr->addCallback("on_setup_tree_append", [&](OSG::VRGuiSignals::Options o) { treeAppend(o["ID"], o["label"], o["type"], o["parent"]); return true; } );
    mgr->addCallback("on_setup_tree_expand", [&](OSG::VRGuiSignals::Options o) { tree.expandAll(); return true; } );

    mgr->addCallback("on_setup_multiwindow", [&](OSG::VRGuiSignals::Options o) { selectWindow(o); return true; } );

    tree.setNodeFlags( ImGuiTreeNodeFlags_DefaultOpen );
}

void ImSetupManager::selectWindow(OSG::VRGuiSignals::Options o) {
    hideAll();
    showWindow = true;
    showRemoteWindow = true;

    selected = o["name"];
    remoteWinState = o["state"];
    string ct = o["connType"];
    winConnType = 0;
    if (ct == "SockPipeline") winConnType = 1;
    if (ct == "StreamSock") winConnType = 2;
    Nx = toInt(o["nx"]);
    Ny = toInt(o["ny"]);
    NxNy.set2(Nx, Ny);
    toValue(o["serverIDs"], serverIDs);
}

void ImSetupManager::hideAll() {
    selected = "";
    showDisplay = false;
    showWindow = false;
    showEditorWindow = false;
    showLocalWindow = false;
    showRemoteWindow = false;
    showViewport = false;
    showVRPN = false;
    showVRPNTracker = false;
    showART = false;
    showARTDevice = false;
    showDevice = false;
    showMultitouch = false;
    showLeap = false;
    showHaptics = false;
    showNode = false;
    showSlave = false;
    showScript = false;
}

void ImSetupManager::treeAppend(string ID, string label, string type, string parent) {
    tree.add(ID, label, IM_TV_NODE_EDITABLE, parent);
}

void ImSetupManager::updateSetupsList(string s) {
    toValue(s, setups);
}

void ImSetupManager::begin() {
    vector<const char*> tmpSetups(setups.size(), 0);
    for (int i=0; i<setups.size(); i++) tmpSetups[i] = setups[i].c_str();
    ImGui::Text("Setup:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("##Setups", &current_setup, &tmpSetups[0], tmpSetups.size())) {
        uiSignal("setup_switch_setup", {{"setup",setups[current_setup]}});
    }

    ImGui::SameLine(); if (ImGui::Button("New")) uiSignal("setup_new");
    ImGui::SameLine(); if (ImGui::Button("Delete")) uiSignal("setup_delete");
    ImGui::SameLine(); if (ImGui::Button("Save")) uiSignal("setup_save");
    ImGui::SameLine(); if (ImGui::Button("Save as..")) uiSignal("setup_saveas");

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();
    float fs = io.FontGlobalScale;
    float width = 120*fs; // TODO: add a width compute to TreeView

    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetContentRegionAvail().y;
    float w1 = min(float(w*0.5), width);
    float w2 = w - w1;

    ImGui::BeginChild("setupTree", ImVec2(w1, h), true, flags);
    tree.render();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("setupProps", ImVec2(w2, h), false, flags);
        if (showDisplay) {
            ImGui::Text(("Displays: " + selected).c_str());
            ImGui::Indent(10);
            // Global Offset Vec3
            ImGui::Unindent(10);
        }

        if (showWindow) {
            ImGui::Text(("Window: " + selected).c_str());
            ImGui::Indent(10);
            // bool active
            // X Display dropdown, probably deprecated
            // fullscreen checkbutton
            // Vec3 Position
            // Vec2 Resolution
            // Combo Mouse device (None / mouse)
            // Combo MSAA ( x0 x2 x4 x8 x16 )
            ImGui::Unindent(10);
        }

        if (showEditorWindow) {
            ImGui::Separator();
            ImGui::Text(("Editor Window: " + selected).c_str());
            ImGui::Indent(10);
            // nothing yet
            ImGui::Unindent(10);
        }

        if (showLocalWindow) {
            ImGui::Separator();
            ImGui::Text(("Local Window: " + selected).c_str());
            ImGui::Indent(10);
            ImGui::Unindent(10);
        }

        if (showRemoteWindow) {
            ImGui::Separator();
            ImGui::Text(("Remote Window: " + selected).c_str());
            ImGui::Indent(10);
            ImGui::Text("State:");
            ImGui::SameLine();
            ImGui::Text(remoteWinState.c_str());
            ImGui::SameLine();
            if (ImGui::Button("connect##win")) uiSignal("win_click_connect", {{}});

            ImGui::Text("Connection type:");
            ImGui::Indent(10);
            if (ImGui::RadioButton("Multicast##win", &winConnType, 0)) { uiSignal("win_set_conn_type", {{"type", "multicast"}}); }
            if (ImGui::RadioButton("SockPipeline##win", &winConnType, 1)) { uiSignal("win_set_conn_type", {{"type", "sockpipeline"}}); }
            if (ImGui::RadioButton("StreamSock##win", &winConnType, 2)) { uiSignal("win_set_conn_type", {{"type", "streamsock"}}); }
            ImGui::Unindent(10);

            if (NxNy.render(160) && NxNy.vX > 0 && NxNy.vY > 0) uiSignal("win_set_NxNy", {{"Nx", toString(int(NxNy.vX))}, {"Ny", toString(int(NxNy.vY))}});
            ImGui::Indent(10);
            int k = 0;
            for (int x=0; x<Nx; x++) {
                for (int y=0; y<Ny; y++) {
                    string sID = "";
                    if (k < serverIDs.size()) sID = serverIDs[k];
                    string xy = toString(x) + " " + toString(y);
                    ImGui::Text(xy.c_str());
                    ImInput entry("##nxy_"+xy, "", sID);
                    ImGui::SameLine();
                    if (entry.render(240)) uiSignal("win_set_serverID", {{"x", toString(x)}, {"y", toString(y)}, {"sID", entry.value}});
                    k++;
                }
            }
            ImGui::Unindent(10);
            ImGui::Unindent(10);
        }

        if (showViewport) {
            ImGui::Text(("Viewport: " + selected).c_str());
            ImGui::Indent(10);
            // Area
            //  Vec2 x
            //  Vec2 y
            // Vec2 Size (resolution)
            // checkbutton statistics
            // checkbutton stereo
            // Eye Separation: entry 0.06 [m]
            // checkbutton invert
            // checkbutton active stereo
            // checkbutton projection
            // checkbutton user  combo trackers
            // Vec3 center
            // Vec3 user
            // Vec3 normal
            // Vec3 up
            // Vec2 size
            // Vec2 shear
            // Vec2 warp
            // checkbutton mirror:
            // Vec3 mirror origin
            // Vec3 mirror normal
            ImGui::Unindent(10);
        }

        if (showVRPN) {
            ImGui::Text(("VRPN: " + selected).c_str());
            ImGui::Indent(10);
            ImGui::Checkbox("active##VRPN", &vrpnActive);
            ImGui::SameLine();
            //ImInput("##vrpnPort", "Port:")
            ImGui::Checkbox("test server##VRPN", &vrpnTestServer);
            ImGui::Checkbox("verbose##VRPN", &vrpnVerbose);
            ImGui::Unindent(10);
        }

        if (showVRPNTracker) {
            ImGui::Text(("VRPN Tracker: " + selected).c_str());
            ImGui::Indent(10);
            // address: entry ADDR
            // IF selected_type == "vrpn_device"
            //   translation: Vec3 TRANS_AXIS
            //   rotation: Vec3 TROT_AXIS
            ImGui::Unindent(10);
        }

        if (showART) {
            ImGui::Text(("ART: " + selected).c_str());
            ImGui::Indent(10);
            // checkbutton active    Port: entry PORT
            // Vec3 Offset
            // Vec3 Axis
            ImGui::Unindent(10);
        }

        if (showARTDevice) {
            ImGui::Text(("ART Device: " + selected).c_str());
            ImGui::Indent(10);
            // ID: ID
            ImGui::Unindent(10);
        }

        if (showDevice) {
            ImGui::Text(("Device: " + selected).c_str());
            ImGui::Indent(10);
            // name: DEVICENAME
            // type: Combo devicetype
            // Intersection: INT_OBJ
            // int. point: INT_PNT
            // int. texel: INT_UV
            // checkbutton show intersection point
            ImGui::Unindent(10);
        }

        if (showMultitouch) {
            ImGui::Text(("Multitouch Device: " + selected).c_str());
            ImGui::Indent(10);
            // device: combo devicelist
            ImGui::Unindent(10);
        }

        if (showLeap) {
            ImGui::Text(("Leap Device: " + selected).c_str());
            ImGui::Indent(10);
            // address: entry ADDR
            // status: STATUS
            // serial: SERIAL
            // button start calibration    button stop calibration
            // transformation:
            //   position: Vec3 pos
            //   direction: Vec3 dir
            //   up-vector: Vec3 up
            ImGui::Unindent(10);
        }

        if (showHaptics) {
            ImGui::Text(("Haptic Device: " + selected).c_str());
            ImGui::Indent(10);
            // IP: entry IP
            // combo type
            // deamon state: STATE
            // device state: STATE
            ImGui::Unindent(10);
        }

        if (showNode) {
            ImGui::Text(("Network Node: " + selected).c_str());
            ImGui::Indent(10);
            //ImInput("##nodeAddr", "address:")
            ImGui::SameLine();
            ImGui::Text("STATE");

            //ImInput("##sshUsr", "ssh user:")
            ImGui::SameLine();
            ImGui::Text("STATE");

            if (ImGui::Button("distribute key##node")) uiSignal("node_clicked_distribkey", {{}});
            ImGui::SameLine();
            ImGui::Text("STATE");

            if (ImGui::Button("stop slaves##node")) uiSignal("node_clicked_stopslaves", {{}});
            ImGui::SameLine();
            ImGui::Text("STATE");

            //ImInput("##pvrPath", "root path:")
            ImGui::SameLine();
            ImGui::Text("STATE");
            ImGui::Unindent(10);
        }

        if (showSlave) {
            ImGui::Text(("Network Slave: " + selected).c_str());
            ImGui::Indent(10);
            // connection identifier:  CONN_ID_STR
            if (ImGui::Checkbox("autostart##slave", &slaveAutostart)) uiSignal("slave_toggle_autostart", {{"state",toString(slaveAutostart)}});
            ImGui::SameLine();
            if (ImGui::Button("start##slave")) uiSignal("slave_clicked_start", {{}});
            ImGui::SameLine();
            ImGui::Text("STATE");

            if (ImGui::Checkbox("active stereo##slave", &slaveActStereo)) uiSignal("slave_toggle_activestereo", {{"state",toString(slaveActStereo)}});
            ImGui::SameLine();
            if (ImGui::Checkbox("fullscreen##slave", &slaveFullscreen)) uiSignal("slave_toggle_fullscreen", {{"state",toString(slaveFullscreen)}});
            ImGui::SameLine();
            //ImInput("##slavePort", "port:")

            ImGui::Text("Connection type:");
            ImGui::SameLine();
            if (ImGui::RadioButton("Multicast##slave", &slaveConnType, 0)) { uiSignal("slave_set_conn_type", {{"type", "multicast"}}); }
            ImGui::SameLine();
            if (ImGui::RadioButton("SockPipeline##slave", &slaveConnType, 1)) { uiSignal("slave_set_conn_type", {{"type", "sockpipeline"}}); }
            ImGui::SameLine();
            if (ImGui::RadioButton("StreamSock##slave", &slaveConnType, 2)) { uiSignal("slave_set_conn_type", {{"type", "streamsock"}}); }

            //ImInput("##screenDisplay", "local display:")
            ImGui::SameLine();
            //ImInput("##startupDelay", "startup delay:")

            //ImInput("##winGeometry", "geometry ('512x512+0+0'):")
            ImGui::Unindent(10);
        }

        if (showScript) {
            ImGui::Text(("Script: " + selected).c_str());
            ImGui::Indent(10);
            // nothing, TODO or deprecate?
            ImGui::Unindent(10);
        }

    ImGui::EndChild();
}
