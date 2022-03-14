#ifndef VRPYENGINEERING_H_INCLUDED
#define VRPYENGINEERING_H_INCLUDED

#include "core/scripting/VRPyObject.h"
#include "VRRobotArm.h"
#include "VRPipeSystem.h"
#include "VRNumberingEngine.h"
#include "Space/VRRocketExhaust.h"
#include "Wiring/VRWiringSimulation.h"
#include "Wiring/VRElectricSystem.h"

struct VRPyNumberingEngine : VRPyBaseT<OSG::VRNumberingEngine> {
    static PyMethodDef methods[];
};

struct VRPyRobotArm : VRPyBaseT<OSG::VRRobotArm> {
    static PyMethodDef methods[];
};

struct VRPyPipeSystem : VRPyBaseT<OSG::VRPipeSystem> {
    static PyMethodDef methods[];
};

struct VRPyWiringSimulation : VRPyBaseT<OSG::VRWiringSimulation> {
    static PyMethodDef methods[];
};

struct VRPyElectricSystem : VRPyBaseT<OSG::VRElectricSystem> {
    static PyMethodDef methods[];
};

struct VRPyRocketExhaust : VRPyBaseT<OSG::VRRocketExhaust> {
    static PyMethodDef methods[];
};

#endif //VRPYENGINEERING_H_INCLUDED
