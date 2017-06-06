#include "VRPyBaseFactory.h"
#include "VRPyLightBeacon.h"
#include "addons/Semantics/Reasoning/VRPyOntology.h"

#include <OpenSG/OSGVector.h>
#include <OpenSG/OSGColor.h>

using namespace OSG;

template<> void toValue<bool>(PyObject* o, bool& b) { b = PyInt_AsLong(o); }
template<> void toValue<int>(PyObject* o, int& i) { i = PyInt_AsLong(o); }
template<> void toValue<float>(PyObject* o, float& f) { f = PyFloat_AsDouble(o); }
template<> void toValue<Color4f>(PyObject* o, Color4f& f) { f = VRPyBase::parseVec4fList(o); }

// Stuff for proxy setter

template<> bool parseValue<int>(PyObject* args, int& t) {
    if (!PyArg_ParseTuple(args, "i", &t)) return false;
    return true;
}

template<> bool parseValue<bool>(PyObject* args, bool& t) {
    int i = 0;
    if (!PyArg_ParseTuple(args, "i", &i)) return false;
    t = i;
    return true;
}

template<> bool parseValue<float>(PyObject* args, float& t) {
    if (!PyArg_ParseTuple(args, "f", &t)) return false;
    return true;
}

template<> bool parseValue<VREntityPtr>(PyObject* args, VREntityPtr& t) {
    VRPyEntity* e = 0;
    if (!PyArg_ParseTuple(args, "O", &e)) return false;
    if (!e) return false;
    t = e->objPtr;
    return true;
}

template<> bool parseValue<string>(PyObject* args, string& t) {
    const char* c = 0;
    if (!PyArg_ParseTuple(args, "s", &c)) return false;
    if (c == 0) return false;
    t = string(c);
    return true;
}

template<> bool parseValue<Vec3f>(PyObject* args, Vec3f& t) {
    PyObject* c;
    if (!PyArg_ParseTuple(args, "O", &c)) return false;
    if (c == 0) return false;
    if (VRPyBase::pySize(c) != 3) return false;
    t = VRPyBase::parseVec3fList(c);
    return true;
}

template<> bool parseValue<Color4f>(PyObject* args, Color4f& t) {
    PyObject* c;
    if (!PyArg_ParseTuple(args, "O", &c)) return false;
    if (c == 0) return false;
    if (VRPyBase::pySize(c) != 4) return false;
    t = VRPyBase::parseVec4fList(c);
    return true;
}

template<> bool parseValue<VRLightBeaconPtr>(PyObject* args, VRLightBeaconPtr& t) {
    VRPyLightBeacon* b;
    if (!PyArg_ParseTuple(args, "O", &b)) return false;
    if (b == 0) return false;
    t = b->objPtr;
    return true;
}


