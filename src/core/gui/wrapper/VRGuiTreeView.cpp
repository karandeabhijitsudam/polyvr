#include "VRGuiTreeView.h"
#include "../VRGuiUtils.h"

#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeselection.h>

VRGuiTreeView::VRGuiTreeView(string name) : VRGuiWidget(name) {
    auto widget = getGUIBuilder()->get_widget(name);
    init(widget);
}

VRGuiTreeView::VRGuiTreeView(_GtkWidget* widget) : VRGuiWidget(widget) {
    init(widget);
}

VRGuiTreeView::~VRGuiTreeView() {
    if (selection) delete selection;
}

void VRGuiTreeView::init(_GtkWidget* widget) {
    tree_view = (GtkTreeView*)widget;
    tree_model = (GtkTreeModel*)gtk_tree_view_get_model(tree_view);
    selection = new GtkTreeIter();
}

bool VRGuiTreeView::hasSelection() {
    updateSelection();
    return validSelection;
}

void VRGuiTreeView::updateSelection() {
    validSelection = getSelection(selection);
}

bool VRGuiTreeView::getSelection(_GtkTreeIter* itr) {
    GtkTreeSelection* sel = gtk_tree_view_get_selection(tree_view);
    return gtk_tree_selection_get_selected(sel, &tree_model, itr);
}

void VRGuiTreeView::selectRow(GtkTreePath* p, GtkTreeViewColumn* c) {
    gtk_tree_view_set_cursor(tree_view, p, c, false);
    grabFocus();
}

void VRGuiTreeView::setValue(GtkTreeIter* itr, int column, void* data) {
    gtk_tree_store_set((GtkTreeStore*)tree_model, itr, column, data, -1);
}

void VRGuiTreeView::setStringValue(GtkTreeIter* itr, int column, string data) {
    setValue(itr, column, (void*)data.c_str());
}

void* VRGuiTreeView::getValue(GtkTreeIter* itr, int column) {
    void* data = 0;
    gtk_tree_model_get(tree_model, itr, column, &data, -1);
    return data;
}

string VRGuiTreeView::getStringValue(GtkTreeIter* itr, int column) {
    char* v = (char*)getValue(itr, column);
    return v?v:"";
}

void VRGuiTreeView::setSelectedValue(int column, void* data) {
    updateSelection();
    if (validSelection) setValue(selection, column, data);
}

void VRGuiTreeView::setSelectedStringValue(int column, string data) {
    setSelectedValue(column, (void*)data.c_str());
}

void* VRGuiTreeView::getSelectedValue(int column) {
    updateSelection();
    if (validSelection) return getValue(selection, column);
    else return 0;
}

string VRGuiTreeView::getSelectedStringValue(int column) {
    char* v = (char*)getSelectedValue(column);
    return v?v:"";
}

int VRGuiTreeView::getSelectedIntValue(int column) {
    int* p = (int*)getSelectedValue(column);
    return p?*p:0;
}

void VRGuiTreeView::removeSelected() {
    updateSelection();
    if (validSelection) removeRow(selection);
}

void VRGuiTreeView::removeRow(GtkTreeIter* itr) {
    gtk_tree_store_remove((GtkTreeStore*)tree_model, itr);
}

bool VRGuiTreeView::getSelectedParent(GtkTreeIter& parent) {
    updateSelection();
    if (!validSelection) return 0;
    return gtk_tree_model_iter_parent(tree_model, &parent, selection);
}

void VRGuiTreeView::expandAll() {
    gtk_tree_view_expand_all(tree_view);
}

string VRGuiTreeView::getSelectedColumnName() {
    GtkTreePath* path;
    GtkTreeViewColumn* column;
    gtk_tree_view_get_cursor(tree_view, &path, &column);
    return gtk_tree_view_column_get_title(column);
}

