#ifndef VRPYTREE_H_INCLUDED
#define VRPYTREE_H_INCLUDED

#include "core/scripting/VRPyBase.h"
#include "VRTree.h"
#include "VRNature.h"

struct VRPyTree : VRPyBaseT<OSG::VRTree> {
    static PyMethodDef methods[];

    static PyObject* setup(VRPyTree* self, PyObject* args);
    static PyObject* addBranching(VRPyTree* self, PyObject* args);
    static PyObject* grow(VRPyTree* self, PyObject* args);
    static PyObject* addLeafs(VRPyTree* self, PyObject* args);
    static PyObject* setLeafMaterial(VRPyTree* self, PyObject* args);
};

struct VRPyNature : VRPyBaseT<OSG::VRNature> {
    static PyMethodDef methods[];

    static PyObject* addTree(VRPyNature* self, PyObject* args);
    static PyObject* addGrassPatch(VRPyNature* self, PyObject* args);
    static PyObject* computeLODs(VRPyNature* self);
    static PyObject* addCollisionModels(VRPyNature* self);
    static PyObject* clear(VRPyNature* self);
    static PyObject* getTree(VRPyNature* self, PyObject* args);
    static PyObject* removeTree(VRPyNature* self, PyObject* args);
};

#endif // VRPYTREE_H_INCLUDED
