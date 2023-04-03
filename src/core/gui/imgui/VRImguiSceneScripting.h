#ifndef VRIMGUISCENESCRIPTING_H_INCLUDED
#define VRIMGUISCENESCRIPTING_H_INCLUDED

#include "VRImguiUtils.h"
#include "imEditor/TextEditor.h"

using namespace std;

struct ImScriptGroup {
    vector<string> scripts;
    string name;
    ImScriptGroup() {}
    ImScriptGroup(string name);
};

class ImScriptList {
    private:
        map<string, ImScriptGroup> groups;
        vector<string> groupsList;

        void clear();
        void addGroup(string name, string ID);
        void addScript(string name, string groupID);

    public:
        ImScriptList();
        void render();
};

class ImScriptEditor {
    private:
        TextEditor imEditor;
        void setBuffer(string data);

    public:
        ImScriptEditor();
        void render();
};

class ImScripting {
    private:
        bool perf = false;
        bool pause = false;

    public:
        ImScriptList scriptlist;
        ImScriptEditor editor;

        ImScripting();
        void render();
};

#endif // VRIMGUISCENESCRIPTING_H_INCLUDED
