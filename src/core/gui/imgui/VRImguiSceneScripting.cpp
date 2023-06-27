#include "VRImguiSceneScripting.h"
#include "core/gui/VRGuiManager.h"

#include <iostream>
#include "core/utils/toString.h"

using namespace std;

void renderInput(string& label, string ID, string signal, string idKey) {
    static char str0[128] = "Script0";
    memcpy(str0, label.c_str(), label.size());
    str0[label.size()] = 0;
    ID = "##"+ID;

    if (ImGui::InputText(ID.c_str(), str0, 128, ImGuiInputTextFlags_EnterReturnsTrue) ) {
        uiSignal(signal, {{"idKey",idKey}, {"inputOld",label}, {"inputNew",string(str0)}});
        label = string(str0);
    }
}

void renderCombo(string& opt, vector<string>& options, string ID, string signal, string idKey) {
    ID = "##"+ID;
    int labelI = 0;
    const char* optionsCstr[options.size()];
    for (int i=0; i<options.size(); i++) {
        optionsCstr[i] = options[i].c_str();
        if (options[i] == opt) labelI = i;
    }

    if (ImGui::Combo(ID.c_str(), &labelI, optionsCstr, options.size())) {
        opt = options[labelI];
        uiSignal(signal, {{"idKey",idKey}, {"newValue",opt}});
    }
}

ImScriptGroup::ImScriptGroup(string name) : name(name) {}

ImScriptList::ImScriptList() {
    auto mgr = OSG::VRGuiSignals::get();
    mgr->addCallback("scripts_list_clear", [&](OSG::VRGuiSignals::Options o){ clear(); return true; } );
    mgr->addCallback("scripts_list_add_group", [&](OSG::VRGuiSignals::Options o){ addGroup(o["name"], o["ID"]); return true; } );
    mgr->addCallback("scripts_list_add_script", [&](OSG::VRGuiSignals::Options o){ addScript(o["name"], o["group"]); return true; } );
}

void ImScriptList::clear() {
    groups.clear();
    groupsList.clear();
    addGroup("__default__", "__default__");
}

void ImScriptList::addGroup(string name, string ID) {
    cout << " ----- addGroup " << name << ", " << ID << endl;
    if (groups.count(ID)) return;
    groups[ID] = ImScriptGroup(name);
    groupsList.push_back(ID);
}

void ImScriptList::addScript(string name, string groupID) {
    if (groupID == "") groupID = "__default__";
    cout << " ----- addScript " << name << ", " << groupID << endl;
    groups[groupID].scripts.push_back(name);
}

void ImScriptList::renderScriptEntry(string& script) {
    //ImVec4 colorSelected(0.3f, 0.5f, 1.0f, 1.0f);
    bool isSelected = bool(selected == script);
    //if (isSelected) ImGui::PushStyleColor(ImGuiCol_Button, colorSelected);
    if (!isSelected) {
        if (ImGui::Button(script.c_str())) {
            selected = script;
            uiSignal("select_script", {{"script",script}});
        }
    } else {
        static char str0[128] = "Script0";
        memcpy(str0, script.c_str(), script.size());
        str0[script.size()] = 0;
        if (ImGui::InputText("##renameScript", str0, 128, ImGuiInputTextFlags_EnterReturnsTrue) ) {
            script = string(str0);
            selected = script;
            uiSignal("rename_script", {{"name",string(str0)}});
            uiSignal("select_script", {{"script",script}});
        }
    }
    //if (isSelected) ImGui::PopStyleColor();
}

void ImScriptList::renderGroupEntry(string& group) {
    bool isSelected = bool(selected == group);
    if (!isSelected) {
        if (ImGui::Button(group.c_str())) {
            selected = group;
            uiSignal("select_group", {{"group",group}});
        }
    } else {
        static char str0[128] = "Group0";
        memcpy(str0, group.c_str(), group.size());
        str0[group.size()] = 0;
        if (ImGui::InputText("##renameGroup", str0, 128, ImGuiInputTextFlags_EnterReturnsTrue) ) {
            group = string(str0);
            selected = group;
            uiSignal("rename_group", {{"name",string(str0)}});
        }
    }
}

void ImScriptList::render() {
    ImVec4 colorGroup(0.3f, 0.3f, 0.3f, 1.0f);
    ImVec4 colorScript(0.5f, 0.5f, 0.5f, 1.0f);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_OpenOnArrow;
    ImGui::PushStyleColor(ImGuiCol_Header, colorGroup);
    ImGui::PushStyleColor(ImGuiCol_Button, colorScript);

    for (auto groupID : groupsList) {
        if (groupID == "__default__") continue;
        string group = groups[groupID].name;
        renderGroupEntry(groups[groupID].name);
        ImGui::SameLine();

        //if (ImGui::CollapsingHeader((group+"##"+groupID).c_str(), flags)) {
        if (ImGui::CollapsingHeader(("##"+groupID).c_str(), flags)) {
            ImGui::Indent();
            for (auto& script : groups[groupID].scripts) renderScriptEntry(script);
            ImGui::Unindent();
        }
    }

    for (auto& script : groups["__default__"].scripts) renderScriptEntry(script);

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
}

ImScriptEditor::ImScriptEditor() {
    auto mgr = OSG::VRGuiSignals::get();
    mgr->addCallback("script_editor_set_buffer", [&](OSG::VRGuiSignals::Options o){ setBuffer(o["data"]); return true; } );
    mgr->addCallback("script_editor_set_parameters", [&](OSG::VRGuiSignals::Options o){ setParameters(o["type"], o["group"]); return true; } );
    mgr->addCallback("script_editor_request_buffer", [&](OSG::VRGuiSignals::Options o){ getBuffer(toInt(o["skipLines"])); return true; } );
    mgr->addCallback("scripts_list_clear", [&](OSG::VRGuiSignals::Options o){ clearGroups(); return true; } );
    mgr->addCallback("scripts_list_add_group", [&](OSG::VRGuiSignals::Options o){ addGroup(o["name"], o["ID"]); return true; } );
    mgr->addCallback("script_editor_clear_trigs_and_args", [&](OSG::VRGuiSignals::Options o){ clearTrigsAndArgs(); return true; } );
    mgr->addCallback("script_editor_add_trigger", [&](OSG::VRGuiSignals::Options o){ addTrigger(o["name"], o["trigger"], o["parameter"], o["device"], o["key"], o["state"]); return true; } );
    mgr->addCallback("script_editor_add_argument", [&](OSG::VRGuiSignals::Options o){ addArgument(o["name"], o["type"], o["value"]); return true; } );
    mgr->addCallback("editor_cmd", [&](OSG::VRGuiSignals::Options o){ editorCommand(o["cmd"]); return true; } );
    imEditor.SetShowWhitespaces(false); // TODO: add as feature!
    imEditor.SetLanguageDefinition(TextEditor::LanguageDefinition::Python());

    typeList = {"Logic (Python)", "Shader (GLSL)", "Web (HTML/JS/CSS)"};
    argumentTypes = {"None", "int", "float", "str", "VRPyObjectType", "VRPyTransformType", "VRPyGeometryType", "VRPyLightType", "VRPyLodType", "VRPyDeviceType", "VRPyMouseType", "VRPyHapticType", "VRPySocketType", "VRPyLeapFrameType"};
    triggerTypes = {"None", "on_scene_load", "on_scene_close", "on_scene_import", "on_timeout", "on_device", "on_socket"};
    device_types = {"None", "mouse", "multitouch", "keyboard", "flystick", "haptic", "server1", "leap", "vrpn_device"};
    trigger_states = {"Pressed", "Released", "Drag", "Drop", "To edge", "From edge"};
}

void ImScriptEditor::editorCommand(string cmd) {
    if (cmd == "toggleLine") {
        auto p = imEditor.GetCursorPosition();
        if (p.mLine <= 1) return;

        // select current line
        p.mColumn = 0;
        imEditor.SetSelectionStart(p);
        p.mLine += 1;
        imEditor.SetSelectionEnd(p);

        // copy current line and delete
        auto ct = ImGui::GetClipboardText();
        string cts = ct?ct:"";
        imEditor.Copy();
        imEditor.Delete();

        // paste above
        p.mLine -= 2;
        imEditor.SetSelectionStart(p);
        imEditor.SetSelectionEnd(p);
        imEditor.SetCursorPosition(p);
        imEditor.Paste();
        ImGui::SetClipboardText(cts.c_str());
    }

    if (cmd == "duplicateLine") {
        if (!imEditor.HasSelection()) {
            auto p = imEditor.GetCursorPosition();
            p.mColumn = 0;
            imEditor.SetSelectionStart(p);
            p.mLine += 1;
            imEditor.SetSelectionEnd(p);
        }

        auto ct = ImGui::GetClipboardText();
        string cts = ct?ct:"";
        imEditor.Copy();
        imEditor.Paste();// instead of two pastes do unselect
        imEditor.Paste();
        ImGui::SetClipboardText(cts.c_str());
    }

    uiSignal("script_editor_text_changed");
}

void ImScriptEditor::getBuffer(int skipLines) {
    string core = imEditor.GetText();
    int begin = 0;
    for (int i=0; i<skipLines; i++) begin = core.find('\n', begin) + 1;
    core = subString(core, begin);
    uiSignal("script_editor_transmit_core", {{"core",core}});
}

void ImScriptEditor::setBuffer(string data) {
    imEditor.SetText(data);
    sensitive = true;
    if (data == "") sensitive = false;
}

void ImScriptEditor::setParameters(string type, string group) {
    current_group = 0;
    for (int i=0; i<groupList.size(); i++) if (groupList[i] == group) current_group = i;

    current_type = 0; // Python
    if (type == "GLSL") current_type = 1;
    if (type == "HTML") current_type = 2;
}

void ImScriptEditor::addTrigger(string name, string trigger, string parameter, string device, string key, string state) {
    triggers.push_back({name, trigger, parameter, device, key, state});
}

void ImScriptEditor::addArgument(string name, string type, string value) {
    arguments.push_back({name, type, value});
}

void ImScriptEditor::clearTrigsAndArgs() {
    triggers.clear();
    arguments.clear();
}

void ImScriptEditor::clearGroups() {
    groups.clear();
    groups["no group"] = "";
    groupList.clear();
    groupList.push_back("no group");
}

void ImScriptEditor::addGroup(string name, string ID) {
    string nameID = name + "##" + ID;
    groups[nameID] = name;
    groupList.clear();
    for (auto& g : groups) groupList.push_back(g.first);
}


void ImScriptEditor::render() {
    if (!sensitive) ImGui::BeginDisabled();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
    if (ImGui::CollapsingHeader("Options", flags)) {
        ImGui::Text("Type: ");
        ImGui::SameLine();
        const char* types[typeList.size()];
        for (int i=0; i<typeList.size(); i++) types[i] = typeList[i].c_str();
        if (ImGui::Combo("##scriptTypesCombo", &current_type, types, typeList.size())) {
            string type = "Python";
            if (current_type == 1) type = "GLSL";
            if (current_type == 2) type = "HTML";
            uiSignal("script_editor_change_type", {{"type",type}});
        }

        const char* groupsCstr[groupList.size()];
        for (int i=0; i<groupList.size(); i++) groupsCstr[i] = groupList[i].c_str();
        ImGui::Text("Group:");
        ImGui::SameLine();
        if (ImGui::Combo("##groupsCombo", &current_group, groupsCstr, groupList.size())) {
            string group = groups[groupList[current_group]];
            uiSignal("script_editor_change_group", {{"group",group}});
        }
    }

    if (ImGui::CollapsingHeader("Triggers", flags)) {
        ImGui::Indent();
        if (ImGui::Button("+##newTrig")) { uiSignal("script_editor_new_trigger"); };
        for (auto& t : triggers) {
            if (ImGui::Button(("-##remTrig-"+t.name).c_str())) { uiSignal("script_editor_rem_trigger", {{"trigger", t.name}}); }; ImGui::SameLine();
            float w = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(w*0.26 - 16);
            renderCombo(t.trigger, triggerTypes, "trigType-"+t.name, "script_editor_change_trigger_type", t.name); ImGui::SameLine();
            renderInput(t.parameter, "trigParam-"+t.name, "script_editor_change_trigger_param", t.name); ImGui::SameLine();
            renderCombo(t.device, device_types, "trigDevice-"+t.name, "script_editor_change_trigger_device", t.name); ImGui::SameLine();
            ImGui::PushItemWidth(16);
            renderInput(t.key, "trigKey-"+t.name, "script_editor_change_trigger_key", t.name); ImGui::SameLine();
            ImGui::PopItemWidth();
            renderCombo(t.state, trigger_states, "trigState-"+t.name, "script_editor_change_trigger_state", t.name);
            ImGui::PopItemWidth();
        }
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Arguments", flags)) {
        ImGui::Indent();
        if (ImGui::Button("+##newArg")) { uiSignal("script_editor_new_argument"); };
        for (auto& a : arguments) {
            if (ImGui::Button(("-##remArg-"+a.name).c_str())) { uiSignal("script_editor_rem_argument", {{"argument", a.name}}); }; ImGui::SameLine();
            float w = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(w*0.33);
            renderInput(a.name, "argName-"+a.name, "script_editor_rename_argument", a.name); ImGui::SameLine();
            renderCombo(a.type, argumentTypes, "argType-"+a.name, "script_editor_change_argument_type", a.name); ImGui::SameLine();
            renderInput(a.value, "argValue-"+a.name, "script_editor_change_argument", a.name);
            ImGui::PopItemWidth();
        }
        ImGui::Unindent();
    }

    if (sensitive) {
        imEditor.Render("Editor");
        if (imEditor.IsTextChanged()) uiSignal("script_editor_text_changed");
    }

    if (!sensitive) ImGui::EndDisabled();
}



ImScripting::ImScripting() {}

void ImScripting::render() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && io.KeysDown['s']) { io.KeysDown['s'] = false; uiSignal("scripts_toolbar_save"); }
    if (io.KeyCtrl && io.KeysDown['e']) { io.KeysDown['e'] = false; uiSignal("scripts_toolbar_execute"); }
    if (io.KeyCtrl && io.KeysDown['w']) { io.KeysDown['w'] = false; uiSignal("clearConsoles"); }

    // toolbar
    ImGui::Spacing();
    ImGui::Indent(5);  if (ImGui::Button("New")) uiSignal("scripts_toolbar_new");
    ImGui::SameLine(); if (ImGui::Button("Template")) uiSignal("ui_toggle_popup", {{"name","template"}, {"width","400"}, {"height","300"}});
    ImGui::SameLine(); if (ImGui::Button("Group")) uiSignal("scripts_toolbar_group");
    ImGui::SameLine(); if (ImGui::Button("Import")) uiSignal("ui_toggle_popup", {{"name","import"}, {"width","400"}, {"height","300"}});
    ImGui::SameLine(); if (ImGui::Button("Delete")) uiSignal("scripts_toolbar_delete");
    ImGui::SameLine(); if (ImGui::Checkbox("Pause Scripts", &pause)) uiSignal("scripts_toolbar_pause", {{"state",toString(pause)}});
    ImGui::SameLine(); if (ImGui::Button("CPP")) uiSignal("scripts_toolbar_cpp");

                       if (ImGui::Button("Save")) uiSignal("scripts_toolbar_save");
    ImGui::SameLine(); if (ImGui::Button("Execute")) uiSignal("scripts_toolbar_execute");
    ImGui::SameLine(); if (ImGui::Button("Search")) uiSignal("ui_toggle_popup", {{"name","search"}, {"width","400"}, {"height","300"}});
    ImGui::SameLine(); if (ImGui::Button("Documentation")) uiSignal("ui_toggle_popup", {{"name","documentation"}, {"width","800"}, {"height","600"}});
    ImGui::SameLine(); if (ImGui::Checkbox("Performance", &perf)) uiSignal("scripts_toolbar_performance", {{"state",toString(perf)}});
    ImGui::Unindent();

    ImGui::Spacing();
    //ImGui::Separator();

    // script list
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    float w = ImGui::GetContentRegionAvail().x;
    float h = ImGui::GetContentRegionAvail().y;

    ImGui::BeginGroup();
    ImGui::Spacing();
    ImGui::BeginChild("ScriptListPanel", ImVec2(w*0.3, h), true, flags);
    scriptlist.render();
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine();

    // script editor
    ImGui::BeginGroup();
    ImGui::Spacing();
    ImGui::BeginChild("ScriptEditorPanel", ImVec2(w*0.7, h), false, flags);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        if (io.KeyCtrl && io.KeysDown['t']) { io.KeysDown['t'] = false; uiSignal("editor_cmd", {{"cmd","toggleLine"}}); }
        if (io.KeyCtrl && io.KeysDown['d']) { io.KeysDown['d'] = false; uiSignal("editor_cmd", {{"cmd","duplicateLine"}}); }
    }
    editor.render();
    ImGui::EndChild();
    ImGui::EndGroup();
}

