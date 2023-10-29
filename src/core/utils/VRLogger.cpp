#include "VRLogger.h"
#ifndef WITHOUT_GTK
#include "core/gui/VRGuiManager.h"
#include "core/gui/VRGuiConsole.h"
#endif

map<string, bool> VRLog::tags;

void VRLog::print(string tag, string s, string c) {
    if (!tags[tag]) return;

#ifndef WITHOUT_GTK
    auto co = OSG::VRConsoleWidget::get(c);
    if (co) co->write(s+"\n");
#endif
}

void VRLog::setTag(string tag, bool b) { tags[tag] = b; }
bool VRLog::tag(string tag) { return tags[tag]; }

void VRLog::log(string tag, string s) { print(tag, "Log: "+s, "Console"); }
void VRLog::wrn(string tag, string s) { print(tag, "Warning: "+s, "Errors"); }
void VRLog::err(string tag, string s) { print(tag, "Error: "+s, "Errors"); }
