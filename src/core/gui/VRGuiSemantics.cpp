
#include "VRGuiSemantics.h"
#include "VRGuiUtils.h"
#include "addons/Semantics/Reasoning/VROntology.h"
#include "addons/Semantics/Reasoning/VRProperty.h"
#include "core/utils/toString.h"
#include "core/math/graph.h"

#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/toolbutton.h>
#include <gtkmm/textview.h>
#include <gtkmm/combobox.h>
#include <gtkmm/builder.h>
#include <gtkmm/fixed.h>
#include <gtkmm/frame.h>
#include <gtkmm/expander.h>
#include <gtkmm/targetentry.h>
#include <gtkmm/separator.h>
#include <gtkmm/stock.h>
#include <gtkmm/box.h>
#include <boost/bind.hpp>
#include "core/scene/VRScene.h"
#include "addons/Algorithms/VRGraphLayout.h"

/** TODO:

- VRGraphLayout
    - 3D bounding boxes and connectors
    - 3D layout algorithms

- add new concept button, extrude from existing concepts
- add concept renaming
- add new property button

- visualise entities
- link concepts to parent concepts
- link entities to concepts
- link object properties of entites to property entity
- link object properties of concept to property concept

- store user changes to build in SBBs
- store user SBBs

IDEAS:
- instead of connectors use color highlights:
    - click on a concept -> all parent concept names go red
                         -> all child concepts go blue
    - click on a obj property -> concept name goes green

*/

OSG_BEGIN_NAMESPACE;
using namespace std;

class ExpanderWithButtons : public Gtk::Expander {
    private:
        Gtk::Toolbar* toolbar = 0;
        Gtk::Label* label = 0;

    public:
        ExpanderWithButtons(string name) {
            label = Gtk::manage( new Gtk::Label( name ) );
            toolbar = Gtk::manage( new Gtk::Toolbar() );
            auto header = Gtk::manage( new Gtk::HBox() );

            header->pack_start(*label, 1, 1, 0);
            header->pack_start(*toolbar, 0, 0, 0);
            set_label_widget(*header);
            set_label_fill(true);
            toolbar->set_icon_size(Gtk::ICON_SIZE_MENU);
            toolbar->set_show_arrow(0);
        }

        void add_button(Gtk::ToolButton* b) { toolbar->add(*b); }
        Gtk::Label* get_label() { return label; }

        bool on_motion_notify_event(GdkEventMotion* e) {
            cout << e->type << " " << e->window << endl;
            return Gtk::Expander::on_motion_notify_event(e);
        }

        bool on_button_press_event(GdkEventButton* e) {
            //cout << "EXPAND2 " << toolbar->on_button_press_event(e) << endl;
            return Gtk::Expander::on_button_press_event(e);
        }

        bool on_button_release_event(GdkEventButton* e) {
            //cout << "EXPAND3 " << toolbar->on_button_release_event(e);
            return Gtk::Expander::on_button_release_event(e);
        }
};

class VRGuiSemantics_ModelColumns : public Gtk::TreeModelColumnRecord {
    public:
        VRGuiSemantics_ModelColumns() { add(name); add(type); }
        Gtk::TreeModelColumn<Glib::ustring> name;
        Gtk::TreeModelColumn<Glib::ustring> type;
};

class VRGuiSemantics_PropsColumns : public Gtk::TreeModelColumnRecord {
    public:
        VRGuiSemantics_PropsColumns() { add(name); add(type); add(prop); add(ptype); add(flag); }
        Gtk::TreeModelColumn<Glib::ustring> name;
        Gtk::TreeModelColumn<Glib::ustring> type;
        Gtk::TreeModelColumn<Glib::ustring> prop;
        Gtk::TreeModelColumn<Glib::ustring> ptype;
        Gtk::TreeModelColumn<int> flag;
};

void VRGuiSemantics_on_drag_data_get(const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& data, unsigned int info, unsigned int time, VRGuiSemantics::ConceptWidget* e) {
    data.set("concept", 0, (const guint8*)&e, sizeof(void*));
}

VRGuiSemantics::ConceptWidget::ConceptWidget(Gtk::Fixed* canvas, VRConceptPtr concept) {
    this->concept = concept;
    this->canvas = canvas;

    // properties treeview
    VRGuiSemantics_PropsColumns cols;
    auto liststore = Gtk::ListStore::create(cols);
    treeview = Gtk::manage( new Gtk::TreeView() );
    treeview->set_model(liststore);
    treeview->set_headers_visible(false);

    auto addMarkupColumn = [&](string title, Gtk::TreeModelColumn<Glib::ustring>& col) {
        Gtk::CellRendererText* renderer = Gtk::manage(new Gtk::CellRendererText());
        Gtk::TreeViewColumn* column = Gtk::manage(new Gtk::TreeViewColumn(title, *renderer));
        column->add_attribute(renderer->property_markup(), col);
        treeview->append_column(*column);
    };

    addMarkupColumn(" Properties:", cols.name);
    addMarkupColumn("", cols.type);

    for (auto p : concept->properties) {
        setPropRow(liststore->append(), p.second->name, p.second->type, "black", 0);
    }

    // buttons
    auto rConcept = Gtk::manage( new Gtk::ToolButton(Gtk::Stock::DELETE) ); // Gtk::Stock::MEDIA_RECORD
    auto nConcept = Gtk::manage( new Gtk::ToolButton(Gtk::Stock::NEW) );

    // expander and frame
    auto expander = Gtk::manage( new ExpanderWithButtons( concept->name ) );
    label = expander->get_label();
    expander->add(*treeview);
    auto frame = Gtk::manage( new Gtk::Frame() );
    frame->add(*expander);
    widget = frame;
    canvas->put(*frame, 0, 0);

    // signals
    treeview->signal_cursor_changed().connect( sigc::mem_fun(*this, &VRGuiSemantics::ConceptWidget::on_select_property) );
    rConcept->signal_clicked().connect( sigc::mem_fun(*this, &VRGuiSemantics::ConceptWidget::on_rem_clicked) );
    nConcept->signal_clicked().connect( sigc::mem_fun(*this, &VRGuiSemantics::ConceptWidget::on_new_clicked) );
    expander->add_button(rConcept);
    expander->add_button(nConcept);

    // dnd
    vector<Gtk::TargetEntry> entries;
    entries.push_back(Gtk::TargetEntry("concept", Gtk::TARGET_SAME_APP));
    expander->drag_source_set(entries, Gdk::BUTTON1_MASK, Gdk::ACTION_MOVE);
    expander->signal_drag_data_get().connect( sigc::bind<ConceptWidget*>( sigc::ptr_fun(VRGuiSemantics_on_drag_data_get), this ) );
}

void VRGuiSemantics::ConceptWidget::setPropRow(Gtk::TreeModel::iterator iter, string name, string type, string color, int flag) {
    string cname = "<span color=\""+color+"\">" + name + "</span>";
    string ctype = "<span color=\""+color+"\">" + type + "</span>";

    Gtk::ListStore::Row row = *iter;
    Glib::RefPtr<Gtk::ListStore> liststore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( treeview->get_model() );

    gtk_list_store_set(liststore->gobj(), row.gobj(), 0, cname.c_str(), -1);
    gtk_list_store_set(liststore->gobj(), row.gobj(), 1, ctype.c_str(), -1);
    gtk_list_store_set(liststore->gobj(), row.gobj(), 2, name.c_str(), -1);
    gtk_list_store_set(liststore->gobj(), row.gobj(), 3, type.c_str(), -1);
    gtk_list_store_set(liststore->gobj(), row.gobj(), 4, flag, -1);
}

void VRGuiSemantics::ConceptWidget::on_rem_clicked() {
    cout << "REMOVE\n";
}

void VRGuiSemantics::ConceptWidget::on_new_clicked() {
    cout << "CREATE\n";
}

bool VRGuiSemantics::ConceptWidget::on_expander_clicked(GdkEventButton* e) {
    cout << "EXPAND\n";
    return true;
}

void VRGuiSemantics::ConceptWidget::on_select() {

}

void VRGuiSemantics::ConceptWidget::move(float x, float y) {
    this->x = x; this->y = y;
    canvas->move(*widget, x, y);
}

void VRGuiSemantics::ConceptWidget::on_select_property() {
    Gtk::TreeModel::iterator iter = treeview->get_selection()->get_selected();
    if (!iter) return;

    Glib::RefPtr<Gtk::ListStore> liststore = Glib::RefPtr<Gtk::ListStore>::cast_dynamic( treeview->get_model() );

    // get selection
    VRGuiSemantics_PropsColumns cols;
    Gtk::TreeModel::Row row = *iter;
    string name = row.get_value(cols.prop);
    string type = row.get_value(cols.ptype);
    int flag = row.get_value(cols.flag);

    // clear selection
    treeview->get_selection()->unselect_all();

    if (flag != 0) {
        setPropRow(iter, name, type, "black", 0);
        return;
    }

    // reset all rows to black
    Gtk::TreeModel::iterator selected;
    liststore->clear();
    for (auto p : concept->properties) {
        Gtk::TreeModel::iterator i = liststore->append();
        setPropRow(i, p.second->name, p.second->type, "black", 0);
        if (p.second->name == name) selected = i;
    }
    if (!selected) return;

    // highlight row in green
    setPropRow(selected, name, type, "green", 1);
}

VRGuiSemantics::ConnectorWidget::ConnectorWidget(Gtk::Fixed* canvas) {
    s1 = Gtk::manage( new Gtk::HSeparator() );
    s2 = Gtk::manage( new Gtk::VSeparator() );
    s3 = Gtk::manage( new Gtk::HSeparator() );
    this->canvas = canvas;
    canvas->put(*s1, 0, 0);
    canvas->put(*s2, 0, 0);
    canvas->put(*s3, 0, 0);
}

void VRGuiSemantics::ConnectorWidget::set(float x1, float y1, float x2, float y2) {
    float w = abs(x2-x1); float h = abs(y2-y1);
    s1->set_size_request(w*0.5, 2);
    s2->set_size_request(2, h);
    s3->set_size_request(w*0.5, 2);
    canvas->move(*s1, x1, y1);
    canvas->move(*s2, x1+w*0.5, y1);
    canvas->move(*s3, x1+w*0.5, y2);
}

void VRGuiSemantics::on_new_clicked() {
    updateLayout();
    return;

    auto scene = VRSceneManager::getCurrent();
    if (scene == 0) return;
    update();
}

void VRGuiSemantics::on_del_clicked() {
    /*Glib::RefPtr<Gtk::TreeView> tree_view  = Glib::RefPtr<Gtk::TreeView>::cast_static(VRGuiBuilder()->get_object("treeview9"));
    Gtk::TreeModel::iterator iter = tree_view->get_selection()->get_selected();
    if(!iter) return;

    VRGuiSemantics_SocketCols cols;
    Gtk::TreeModel::Row row = *iter;
    string name = row.get_value(cols.name);

    string msg1 = "Delete socket " + name;
    if (!askUser(msg1, "Are you sure you want to delete this socket?")) return;
    auto scene = VRSceneManager::getCurrent();
    if (scene) scene->remSocket(name);

    Glib::RefPtr<Gtk::ListStore> list_store  = Glib::RefPtr<Gtk::ListStore>::cast_static(VRGuiBuilder()->get_object("Sockets"));
    list_store->erase(iter);

    Gtk::ToolButton* b;
    VRGuiBuilder()->get_widget("toolbutton15", b);
    b->set_sensitive(false);*/
}

void VRGuiSemantics::clearCanvas() {
    for (auto c : canvas->get_children()) canvas->remove(*c);
    canvas->show_all();
}

void VRGuiSemantics::updateLayout() {
    VRGraphLayout layout;
    graph<Vec3f>& g = layout.getGraph();
    layout.setGravity(Vec3f(0,1,0));
    layout.setRadius(100);

    map<string, int> conceptIDs;

    for (auto c : concepts) {
        Vec3f p = Vec3f(c.second->x, c.second->y, 0);
        conceptIDs[c.first] = g.addNode(p);
        if (c.first == "Thing") layout.fixNode(conceptIDs[c.first]);
    }

    for (auto c : concepts) {
        for (auto c2 : c.second->concept->children) {
            g.connect(conceptIDs[c.first],conceptIDs[c2.second->name]);
        }
    }

    layout.compute(1, 0.002);

    int i = 0;
    for (auto c : concepts) {
        Vec3f& p = g.getNodes()[i];
        c.second->move(p[0], p[1]);
        i++;
    }

    canvas->show_all();
}

void VRGuiSemantics::drawCanvas(string name) {
    auto onto = VROntology::library[name];

    int i = 0;
    concepts.clear();
    function<void(VRConceptPtr,int,int,int,int)> travConcepts = [&](VRConceptPtr c, int cID, int lvl, int xp, int yp) {
        auto cw = ConceptWidgetPtr( new ConceptWidget(canvas, c) );
        concepts[c->name] = cw;

        int x = 10+cID*60;
        int y = 10+40*lvl;
        cw->move(x,y);

        if (lvl > 0) {
            auto co = ConnectorWidgetPtr( new ConnectorWidget(canvas) );
            connectors[c->name] = co;
            co->set(xp, yp, x, y);
        }

        i++;
        int child_i = 0;
        for (auto ci : c->children) { travConcepts(ci.second, child_i, lvl+1, x, y); child_i++; }
    };
    travConcepts(onto->thing, 0, 0, 0, 0);

    canvas->show_all();
}

void VRGuiSemantics::on_treeview_select() {
    setToolButtonSensitivity("toolbutton15", true);

    Glib::RefPtr<Gtk::TreeView> tree_view  = Glib::RefPtr<Gtk::TreeView>::cast_static(VRGuiBuilder()->get_object("treeview16"));
    Gtk::TreeModel::iterator iter = tree_view->get_selection()->get_selected();

    clearCanvas();
    if (!iter) return;

    // get selection
    VRGuiSemantics_ModelColumns cols;
    Gtk::TreeModel::Row row = *iter;
    string name = row.get_value(cols.name);
    string type = row.get_value(cols.type);

    drawCanvas(name);
}

void VRGuiSemantics_on_name_edited(GtkCellRendererText *cell, gchar *path_string, gchar *new_name, gpointer d) {
    /*Glib::RefPtr<Gtk::TreeView> tree_view  = Glib::RefPtr<Gtk::TreeView>::cast_static(VRGuiBuilder()->get_object("treeview9"));
    Gtk::TreeModel::iterator iter = tree_view->get_selection()->get_selected();
    if(!iter) return;

    // get selected socket
    VRGuiSemantics_SocketCols cols;
    Gtk::TreeModel::Row row = *iter;
    string name = row.get_value(cols.name);
    row[cols.name] = new_name;

    // update key in map
    auto scene = VRSceneManager::getCurrent();
    if (scene) scene->changeSocketName(name, new_name);*/
}

void VRGuiSemantics_on_notebook_switched(GtkNotebook* notebook, GtkNotebookPage* page, guint pageN, gpointer data) {
    //if (pageN == 4) update();
}

void VRGuiSemantics_on_drag_data_received(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& data, guint info, guint time, VRGuiSemantics* self) {
    if (data.get_target() != "concept") { cout << "VRGuiSemantics_on_drag_data_received, wrong dnd: " << data.get_target() << endl; return; }
    VRGuiSemantics::ConceptWidget* e = *(VRGuiSemantics::ConceptWidget**)data.get_data();
    e->move(x,y);
}

VRGuiSemantics::VRGuiSemantics() {
    VRGuiBuilder()->get_widget("onto_visu", canvas);
    setToolButtonCallback("toolbutton14", sigc::mem_fun(*this, &VRGuiSemantics::on_new_clicked));
    setToolButtonCallback("toolbutton15", sigc::mem_fun(*this, &VRGuiSemantics::on_del_clicked));
    setTreeviewSelectCallback("treeview16", sigc::mem_fun(*this, &VRGuiSemantics::on_treeview_select) );
    setCellRendererCallback("cellrenderertext51", VRGuiSemantics_on_name_edited);
    setNoteBookCallback("notebook3", VRGuiSemantics_on_notebook_switched);
    setToolButtonSensitivity("toolbutton15", false);

    // dnd
    vector<Gtk::TargetEntry> entries;
    entries.push_back(Gtk::TargetEntry("concept", Gtk::TARGET_SAME_APP));
    canvas->drag_dest_set(entries, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_MOVE);
    canvas->signal_drag_data_received().connect( sigc::bind<VRGuiSemantics*>( sigc::ptr_fun(VRGuiSemantics_on_drag_data_received), this ) );

    // layout update cb
    auto sm = VRSceneManager::get();
    updateLayoutCb = VRFunction<int>::create("layout_update", boost::bind(&VRGuiSemantics::updateLayout, this));
    sm->addUpdateFkt(updateLayoutCb);
}

void VRGuiSemantics::update() {
    auto scene = VRSceneManager::getCurrent();
    if (scene == 0) return;

    // update script list
    Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(VRGuiBuilder()->get_object("onto_list"));
    store->clear();

    for (auto o : VROntology::library) {
        Gtk::ListStore::Row row = *store->append();
        gtk_list_store_set(store->gobj(), row.gobj(), 0, o.first.c_str(), -1);
        gtk_list_store_set(store->gobj(), row.gobj(), 1, "built-in", -1);
    }
}

OSG_END_NAMESPACE;
