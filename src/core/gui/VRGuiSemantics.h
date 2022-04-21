#ifndef VRGUISEMANTICS_H_INCLUDED
#define VRGUISEMANTICS_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include "core/math/OSGMathFwd.h"
#include "core/scene/VRSceneManager.h"
#include "core/utils/VRFunctionFwd.h"
#include "core/math/VRMathFwd.h"
#include "addons/Semantics/VRSemanticsFwd.h"
#include "VRGuiSignals.h"
#include "VRGuiFwd.h"
#include "addons/Algorithms/VRAlgorithmsFwd.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRGuiSemantics {
    private:
        VRWidgetsCanvasPtr canvas;
        VROntologyPtr current;

        void on_new_clicked();
        void on_del_clicked();
        void on_open_clicked();
        void on_diag_load_clicked();

        void on_treeview_select();
        void on_property_treeview_select();
        void on_query_clicked();

        void setOntology(string name);

        VRSemanticManagerPtr getManager();

    public:
        VRGuiSemantics();

        void clear();

        VROntologyPtr getSelectedOntology();

        bool updateOntoList();
        void updateCanvas();

        void connect(VRSemanticWidgetPtr p1, VRSemanticWidgetPtr p2, string color);
        void disconnect(VRSemanticWidgetPtr p1, VRSemanticWidgetPtr p2);
        void disconnectAny(VRSemanticWidgetPtr p1);

        void copyConcept(VRConceptWidget* w);
        void addEntity(VRConceptWidget* w);
        void addRule(VRConceptWidget* w);
        void remConcept(VRConceptWidget* w);
        void remEntity(VREntityWidget* w);
        void remRule(VRRuleWidget* w);
};

OSG_END_NAMESPACE

#endif // VRGUISEMANTICS_H_INCLUDED
