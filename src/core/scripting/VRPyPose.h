#ifndef VRPYPOSE_H_INCLUDED
#define VRPYPOSE_H_INCLUDED

#include "core/math/pose.h"
#include "core/scripting/VRPyBase.h"

struct VRPyPose : VRPyBaseT<OSG::Pose> {
    static PyMethodDef methods[];

    static PyObject* New(PyTypeObject *type, PyObject *args, PyObject *kwds);
};

#endif // VRPYPOSE_H_INCLUDED
