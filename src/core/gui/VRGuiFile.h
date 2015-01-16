#ifndef VRGUIFILE_H_INCLUDED
#define VRGUIFILE_H_INCLUDED

#include <string>
#include <sigc++/functors/slot.h>
#include <gtkmm/filechooser.h>

namespace Gtk { class FileChooserDialog; }
using namespace std;

class VRGuiFile {
    private:
        static Gtk::FileChooserDialog* dialog;
        typedef sigc::slot<void> sig;
        static sig sigApply;
        static sig sigClose;
        static void init();

    public:
        static void open(string button, Gtk::FileChooserAction action, string title);
        static void close();
        static void apply();
        static string getPath();
        static string getRelativePath_toScene();
        static string getRelativePath_toWorkdir();

        static void gotoPath(string path);
        static void setFile(string file);
        static void addFilter(string name, string pattern);
        static void clearFilter();

        static void setCallbacks(sig sa = sig(), sig sc = sig());
};

#endif // VRGUIFILE_H_INCLUDED
