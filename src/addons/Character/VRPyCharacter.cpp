#include "VRPyCharacter.h"
#include "core/scripting/VRPyBaseT.h"
#include "core/scripting/VRPyPose.h"
#include "core/scripting/VRPyMath.h"
#include "core/scripting/VRPyConstraint.h"

using namespace OSG;

simpleVRPyType(Behavior, New_ptr);
simpleVRPyType(Skin, 0);
simpleVRPyType(Skeleton, New_ptr);
simpleVRPyType(Character, New_VRObjects_ptr);
simpleVRPyType(Humanoid, New_VRObjects_ptr);


PyMethodDef VRPyBehavior::methods[] = {
    {NULL}  /* Sentinel */
};

PyMethodDef VRPySkin::methods[] = {
    {NULL}  /* Sentinel */
};

PyMethodDef VRPySkeleton::methods[] = {
    {"getKinematics", PyWrap( Skeleton, getKinematics, "Get internal FABRIK solver", FABRIKPtr ) },
    {"getTarget", PyWrap( Skeleton, getTarget, "Get joint target by name", PosePtr, string ) },
    {"getJointID", PyWrap( Skeleton, getJointID, "Get joint ID by name", int, string ) },
    {NULL}  /* Sentinel */
};

PyMethodDef VRPyCharacter::methods[] = {
    {"getSkeleton", PyWrap( Character, getSkeleton, "Get the skeleton", VRSkeletonPtr ) },
    //{"setSkin", PyWrap( Character, setSkin, "Set the skin geometry", void, string, bool ) },
    {"addBehavior", PyWrap( Character, addBehavior, "Add a behavior pattern", void, VRBehaviorPtr ) },
    //{"triggerBehavior", PyWrap( Character, triggerBehavior, "Trigger a certain behavior", void, string ) },
    {"simpleSetup", PyWrap( Character, simpleSetup, "Simple character setup", void ) },
    {"addDebugSkin", PyWrap( Character, addDebugSkin, "Add a simple debug skin", void ) },
    {"setSkin", PyWrap( Character, setSkin, "Set geometry with skin", void, VRGeometryPtr, VRSkinPtr, VRSkeletonPtr ) },
    {"move", PyWrap( Character, move, "Move end effector, 'wristL/R', 'palmL/R', 'ankleL/R', 'toesL/R'", PathPtr, string, PosePtr, float ) },
    {"moveTo", PyWrapOpt( Character, moveTo, "Move to position", "4", PathPtr, Vec3d, float ) },
    {"grab", PyWrapOpt( Character, grab, "Grab an object with right hand", "2", PathPtr, Vec3d, float ) },
    {NULL}  /* Sentinel */
};

PyMethodDef VRPyHumanoid::methods[] = {
    {NULL}  /* Sentinel */
};



