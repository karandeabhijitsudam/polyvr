#include "VRImguiInput.h"

#include <iostream>
using namespace std;

ImInput::ImInput(string ID, string label, string value, int flags) : ID("##"+ID), label(label), value(value), flags(flags) {}

int imguiInputCb(ImGuiInputTextCallbackData* data) { // the callback reacts on any edit to make sure the value is allways set, even if InputTexxt only reacts on ENTER
    ImInput& input = *((ImInput*)data->UserData);
    input.value = string(data->Buf, data->BufTextLen);
    //cout << "imguiInputCb " << data->BufTextLen << ", " << input.value << endl;
    return 0;
}

bool ImInput::render() {
    if (label != "") {
        ImGui::Text(label.c_str());
        ImGui::SameLine();
    }

    static char str0[128] = "";
    int N = min(128, int(value.size()));
    memcpy(str0, value.c_str(), N);
    str0[N] = 0;

    if (ImGui::InputText(ID.c_str(), str0, 128, flags | ImGuiInputTextFlags_CallbackEdit, imguiInputCb, this) ) {
        value = string(str0);
        return true;
    }

    return false;
}
