#include "VRPyPose.h"
#include "VRPyBaseT.h"

using namespace OSG;

simplePyType( Pose, VRPyPose::New );

PyMethodDef VRPyPose::methods[] = {
    {"pos", PyCastWrap2(Pose, pos, "Get the position", Vec3d ) },
    {"dir", PyCastWrap2(Pose, dir, "Get the direction", Vec3d ) },
    {"up", PyCastWrap2(Pose, up, "Get the up vector", Vec3d ) },
    {"set", PyWrap2(Pose, set, "Set the pose", void, Vec3d, Vec3d, Vec3d ) },
    {"mult", PyWrap2(Pose, transform, "Transform a vector", Vec3d, Vec3d ) },
    {"multInv", PyWrap2(Pose, transformInv, "Transform back a vector", Vec3d, Vec3d ) },
    {"invert", PyWrap2(Pose, invert, "Invert pose", void ) },
    {NULL}  /* Sentinel */
};

PyObject* VRPyPose::New(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    PyObject *p, *d, *u;
    if (! PyArg_ParseTuple(args, "OOO", &p, &d, &u)) return NULL;
    return allocPtr( type, OSG::Pose::create( parseVec3dList(p), parseVec3dList(d), parseVec3dList(u) ) );
}


