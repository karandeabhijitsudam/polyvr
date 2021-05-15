#include "VRSkeleton.h"
#include "core/utils/toString.h"
#include "core/utils/VRFunction.h"
#include "core/math/partitioning/graph.h"
#include "core/math/Eigendecomposition.h"
#include "core/math/equation.h"
#include "core/math/VRKabschAlgorithm.h"
#include "core/math/kinematics/VRFABRIK.h"
#include "core/objects/geometry/VRGeoData.h"
#include "core/objects/material/VRMaterial.h"
#include "addons/Algorithms/VRPathFinding.h"

#include <OpenSG/OSGQuaternion.h>

using namespace OSG;


VRSkeleton::Configuration::Configuration(string n) { setNameSpace("skConfig"); setName(n); }
VRSkeleton::Configuration::~Configuration() {}
VRSkeleton::ConfigurationPtr VRSkeleton::Configuration::create(string n) { return ConfigurationPtr(new Configuration(n) ); }

void VRSkeleton::Configuration::setPose(int i, Vec3d p) { joints[i] = p; }


VRSkeleton::VRSkeleton() : VRGeometry("skeleton") {
    armature = Graph::create();
    fabrik = FABRIK::create();
}

VRSkeleton::~VRSkeleton() {}

VRSkeletonPtr VRSkeleton::create() { return VRSkeletonPtr(new VRSkeleton() ); }

FABRIKPtr VRSkeleton::getKinematics() { return fabrik; }

void VRSkeleton::clear() {
    bones.clear();
    joints.clear();
    endEffectors.clear();
    armature->clear();
    rootBone = -1;
}

int VRSkeleton::addBone(PosePtr pose, float length, string name) {
    int nID = armature->addNode();
    bones[nID].pose = *pose;
    bones[nID].length = length;
    bones[nID].name = name;
    return nID;
}

int VRSkeleton::addJoint(int bone1, int bone2, VRConstraintPtr constraint, string name, Color3f col) {
    int eID = armature->connect(bone1, bone2);
    joints[eID].bone1 = bone1;
    joints[eID].bone2 = bone2;
    joints[eID].name = name;
    joints[eID].col = col;
    joints[eID].constraint = constraint;
    return eID;
}

void VRSkeleton::setEndEffector(string label, int bone) {
    if (!endEffectors.count(label)) endEffectors[label] = EndEffector();
    endEffectors[label].boneID = bone;
    endEffectors[label].jointID = armature->getInEdges( bone )[0].ID;
}

void VRSkeleton::setRootBone(int bone) { rootBone = bone; }

void VRSkeleton::asGeometry(VRGeoData& data) {
    Vec3d n(1,0,0);
    Color3f red(1,0,0);
    Color3f green(0,1,0);
    Color3f yellow(1,1,0);

    for (auto& b : bones) {
        auto& bone = b.second;
        Pnt3d p1 = bone.pose.pos() + bone.pose.dir() * bone.length*0.5;
        Pnt3d p2 = bone.pose.pos() - bone.pose.dir() * bone.length*0.5;
        Pnt3d p3 = bone.pose.pos() - bone.pose.dir() * bone.length*0.3 + bone.pose.up()*0.1*bone.length;
        Pnt3d p4 = bone.pose.pos() - bone.pose.dir() * bone.length*0.3 - bone.pose.up()*0.1*bone.length;
        Pnt3d p5 = bone.pose.pos() - bone.pose.dir() * bone.length*0.3 + bone.pose.x()*0.1*bone.length;
        Pnt3d p6 = bone.pose.pos() - bone.pose.dir() * bone.length*0.3 - bone.pose.x()*0.1*bone.length;
        int v1 = data.pushVert(p1, n, green);
        int v2 = data.pushVert(p2, n, green);
        int v3 = data.pushVert(p3, n, yellow);
        int v4 = data.pushVert(p4, n, green*0.5);
        int v5 = data.pushVert(p5, n, green);
        int v6 = data.pushVert(p6, n, green);
        data.pushLine(v1, v3);
        data.pushLine(v3, v2);
        data.pushLine(v1, v4);
        data.pushLine(v4, v2);
        data.pushLine(v1, v5);
        data.pushLine(v5, v2);
        data.pushLine(v1, v6);
        data.pushLine(v6, v2);
        data.pushLine(v3, v5);
        data.pushLine(v5, v4);
        data.pushLine(v4, v6);
        data.pushLine(v6, v3);
        //cout << "create bone geo " << bone.length << "  " << bone.pose.pos() << endl;
    }

    for (auto& j : joints) {
        auto& joint = j.second;
        auto& bone1 = bones[joint.bone1];
        auto& bone2 = bones[joint.bone2];
        Pnt3d p1 = bone1.pose.transform( joint.constraint->getReferenceA()->pos() + Vec3d(0,0.01,0) );
        Pnt3d p2 = bone2.pose.transform( joint.constraint->getReferenceB()->pos() - Vec3d(0,0.01,0) );
        int v1 = data.pushVert(p1, n, red);
        int v2 = data.pushVert(p2, n, yellow);
        data.pushPoint(v1);
        data.pushPoint(v2);

        Pnt3d b1 = bone1.pose.transform( Vec3d(0, 0.01,0) );
        Pnt3d b2 = bone2.pose.transform( Vec3d(0,-0.01,0) );
        int l1 = data.pushVert(b1, n, red);
        int l2 = data.pushVert(b2, n, yellow);
        data.pushLine(l1,v1);
        data.pushLine(l2,v2);
    }
}

void VRSkeleton::setupGeometry() {
    auto mS = VRMaterial::get("skeleton");
    mS->setLit(0);
    mS->setLineWidth(2);
    mS->setPointSize(4);
    setMaterial(mS);
	updateGeometry();
}

void VRSkeleton::updateGeometry() {
    fabrik->visualize(ptr());
    /*VRGeoData geo;
    asGeometry(geo);
    geo.apply( ptr() );*/
}

void VRSkeleton::setupSimpleHumanoid() {
    clear();

	fabrik->addJoint(0, Pose::create(Vec3d(0,1.2,0), Vec3d(0,1,0), Vec3d(0,0,1)));

	auto addLeg = [&](string n, float w, int i) {
		fabrik->addJoint(i+1, Pose::create(Vec3d(w,1.0,0), Vec3d(0,1,0), Vec3d(0,0,1)));
		fabrik->addJoint(i+2, Pose::create(Vec3d(w,0.5,0), Vec3d(0,1,0), Vec3d(0,0,1)));
		fabrik->addJoint(i+3, Pose::create(Vec3d(w,0,0), Vec3d(0,1,0), Vec3d(0,0,1)));
		fabrik->addJoint(i+4, Pose::create(Vec3d(w,-0.2,0), Vec3d(0,1,0), Vec3d(0,0,1)));
		//fabrik->addChain(n, {0,i+1,i+2,i+3,i+4});
		fabrik->addChain(n+"_foot", {i+3,i+4});
		fabrik->addChain(n, {i+1,i+2,i+3});

		float a1 = 0.4; // 0.1
		float a2 = 2.0;
		fabrik->addConstraint(i+2, Vec4d(a1,0,a1,a2)); // knee
		fabrik->addConstraint(i+3, Vec4d(a1,a2,a1,a1)); // ankle
		//fabrik->addSpring(i+2, Vec3d(0,-0.5,0)); // knee
		//fabrik->addSpring(i+3, Vec3d(0, 0.2,0)); // ankle

		fabrik->setTarget(i+4, Pose::create(Vec3d(w,-0.01,0.2)));
		fabrik->setTarget(i+3, Pose::create(Vec3d(w,0,0)));
    };

	auto addArm = [&](string n, float w, int i) {
		fabrik->addJoint(i+1, Pose::create(Vec3d(w,1.5,0)));
		fabrik->addJoint(i+2, Pose::create(Vec3d(w,1.2,0)));
		fabrik->addJoint(i+3, Pose::create(Vec3d(w,0.9,0)));
		fabrik->addJoint(i+4, Pose::create(Vec3d(w,0.8,0)));
		fabrik->addChain(n, {0,i+1,i+2,i+3,i+4});
		fabrik->setTarget(i+4, Pose::create(Vec3d(w,0.8,0)));
    };

	addLeg("leg1", 0.2, 0);
	addLeg("leg2",-0.2, 4);
	addArm("arm1", 0.3, 8);
	addArm("arm2",-0.3,12);

	fabrik->addJoint(17, Pose::create(Vec3d(0,1.6,0)));
	fabrik->addJoint(18, Pose::create(Vec3d(0,1.8,0)));
	fabrik->addChain("head", {0,17,18});
	fabrik->setTarget(18, Pose::create(Vec3d(0.1,1.8,0)));
	fabrik->setTarget(18, Pose::create(Vec3d(-0.1,1.8,0)));

	updateGeometry();
}

void VRSkeleton::updateJointPositions() {
    //cout << "VRSkeleton::updateJointPositions" << endl;
    for (auto& j : joints) {
        auto& bone1 = bones[j.second.bone1];
        //auto& bone2 = bones[j.second.bone2];
        j.second.pos = bone1.pose.transform( j.second.constraint->getReferenceA()->pos() );
    }
};

map<int, Vec3d> VRSkeleton::getJointsPositions() {
    map<int, Vec3d> res;
    for (auto j : joints) res[j.first] = j.second.pos;
    return res;
}

vector<int> VRSkeleton::getBoneJoints(int bone) {
    vector<int> res;
    auto edges = armature->getConnectedEdges( armature->getNode(bone) );
    for (auto e : edges) res.push_back(e.ID);
    return res;
}

Vec3d& VRSkeleton::jointPos(int j) { return joints[j].pos; };


/*

Method to solve constraint:

- aim: check constraints limits and move accordingly
    - compute angles between dir1 and dir2 as well as up1 and up2
    - compute rotation direction to extend angles to 2Pi
    - move along vector between the dir vectors, simply take up vectors, until constraint satisfied

- 2D system:
    - lengths of bone1 and bone2 are 'a' and 'b'
    - length to compute, used to move joint, is 'd'
    - angles enclosed by segment a-d and b-d is l
    - angle oposed to a is x, oposed to b is y
    - g' the angle needed to satisfy the constraint
    - work with complementary angle: g = x+y with g = 2*Pi - g'

- computation:
    - sinus law:
        a/sin(x) = b/sin(y) = d/sin(pi-x-l)
        sin(pi-x-l) = sin(x+l)
        => d = a * sin(x+l)/sin(x) = b * sin(y+l)/sin(y)
        => d = a * [ cos(l) + sin(l)/tan(x) ] = b * [ cos(l) + sin(l)/tan(y) ]
        => tan(x) = sin(l) / [d/a - cos(l)]
        => tan(y) = sin(l) / [d/b - cos(l)]
        g = x+y
        tan(g) = tan(x+y) = [tan(x) + tan(y)] / [1 - tan(x)*tan(y)]
        => ... => d² - d * (1/a + 1/b) * a*b * [ (sin(l)/tan(g)) + cos(l) ] + 2*cos(l)*a*b*[ (sin(l)/tan(g)) + cos(l) ] - a*b  =  0
        solve equation

*/

double VRSkeleton::computeAngleProjection(double l, double g, double d1, double d2) {
    g = 2*Pi - g;
    double C = sin(l)/tan(g);
    double cosA = cos(l);

    double b = - (1.0/d1 + 1.0/d2) * d1*d2*(cosA + C);
    double c = 2*cosA * d1*d2*(cosA + C) - d1*d2;

    equation eq(0,1,b,c);
    double x1 = 0, x2 = 0, x3 = 0;
    int resN = eq.solve(x1,x2,x3);
    double d = x1;
    if (abs(x1) > abs(x2)) d = x2;
    return d;
}

string VRSkeleton::pQuat(Quaterniond& q) {
    double a; Vec3d d;
    q.getValueAsAxisRad(d,a);
    return "(" + toString(d) + ") " + toString(a);
};

Quaterniond VRSkeleton::getVecRotation(int i1, int i2, Vec3d dOld)  {
    auto& J1 = joints[i1];
    auto& J2 = joints[i2];
    Vec3d d1 = dOld;
    Vec3d d2 = J2.pos - J1.pos;
    d1.normalize();
    d2.normalize();
    auto q = Quaterniond(d1,d2);
    //if (verbose) cout << " getRotation " << J1.name << " / " << J2.name << ", " << pQuat(q) << endl;
    //if (verbose) cout << "      " << d1 << " / " << d2 << endl;
    if (J1.name == "waist" || J2.name == "waist") {
        cout << " getRotation " << J1.name << " / " << J2.name << ", " << pQuat(q) << ", d1: " << d1 << ", d2: " << d2 << endl;
        cout << "      " << d1 << " / " << d2 << endl;
    }

    return q;
};

Quaterniond VRSkeleton::getRotation(int i1, int i2, Vec3d pOld1, Vec3d pOld2)  {
    return getVecRotation(i1, i2, pOld2 - pOld1);
};

Quaterniond VRSkeleton::getRotation(int i1, int i2, Vec3d pOld)  {
    auto& J2 = joints[i2];
    return getVecRotation(i1, i2, J2.pos - pOld);
};

void VRSkeleton::rotateJoints(int i1, int i2, Quaterniond& R, ChainData& chain) {
    auto& O1 = chain.orientations[i1];
    auto& O2 = chain.orientations[i2];
    R.multVec( O1.dir2, O1.dir2 );
    R.multVec( O1.up2,  O1.up2 );
    R.multVec( O2.dir1, O2.dir1 );
    R.multVec( O2.up1,  O2.up1 );
    O2.dir1.normalize();
    O1.dir2.normalize();
    O2.up1.normalize();
    O1.up2.normalize();

    //if (verbose) cout << "   Rotate " << bones[J1.bone2].name << "  " << bones[J2.bone1].name << " " << pQuat(R) << endl;

    if (joints[i1].name == "waist" || joints[i2].name == "waist") {
        cout << "   rotateJoints " << joints[i1].name << " around " << joints[i2].name << ", " << pQuat(R) << endl;
        cout << "    bones " << bones[joints[i1].bone2].name << ",  " << bones[joints[i2].bone1].name << endl;
        Vec3d seg = joints[i2].pos - joints[i1].pos;
        seg.normalize();
        //cout << "       check seg2: " << seg << ",  dir1: " << J1.dir1 << ", dot: " << J1.dir1.dot(seg) << endl;
        //cout << "       check seg: " << seg << ",  dir2: " << J1.dir2 << ", dot: " << -J1.dir2.dot(seg) << endl;
    }
};

void VRSkeleton::applyFABRIK(string EE) {
    ChainData& data = ChainDataMap[EE];
    auto targetPos = data.targetPos;
    float tol = 0.001; // 1 mm tolerance
    vector<float>& distances = data.distances;
    int Nd = distances.size();

    bool verbose = false;

    auto sum = [](vector<float> v) {
        float r = 0;
        for (auto f : v) r += f;
        return r;
    };

    auto interp = [](Vec3d& a, Vec3d& b, float t) {
        return a*t + b*(1-t);
    };

    auto movePointTowards = [&](int i, Vec3d target, float t) -> Vec3d {
        auto& J = joints[data.joints[i]];
        Vec3d pOld = J.pos;
        J.pos = interp(J.pos, target, t);
        if (verbose) cout << " movePointTowards: " << J.name << " (" << pOld << ") -> " << 1-t << " / " << target << " -> " << J.pos << endl;
        //if (J.name == "elbowLeft")
        //cout << " movePointTowards: " << J.name << " (" << pOld << ") -> " << 1-t << " / " << target << " -> " << J.pos << endl;
        return pOld;
    };

    auto moveToDistance = [&](int i1, int i2, int dID1, int dID2) -> Vec3d {
        auto& J1 = joints[data.joints[i1]];
        auto& J2 = joints[data.joints[i2]];

        /*if (bones[J1.bone2].name == "lArmLeft" || bones[J2.bone1].name == "lArmLeft") {
            cout << "   moveToDistance " << J1.name << " around " << J2.name << endl;
            Vec3d seg = J2.pos - J1.pos;
            seg.normalize();
            cout << "       check seg1: " << seg << ",  dir1: " << J1.dir1 << ", dot: " << J1.dir1.dot(seg) << endl;
            cout << "       check seg1: " << seg << ",  dir2: " << J1.dir2 << ", dot: " << J1.dir2.dot(seg) << endl;
        }*/

        Vec3d pOld = J1.pos;
        if (verbose) cout << "moveToDistance between: " << J1.name << " -> " << J2.name << endl;

        // joint angles
        auto& O1 = data.orientations[data.joints[i1]];
        float aD = O1.dir2.enclosedAngle(O1.dir1);
        float aU = O1.up2.enclosedAngle(O1.up1);

        // joint angle direction
        Vec3d cX = O1.dir1.cross(O1.up1);
        Vec3d cD = O1.dir2.cross(O1.dir1);
        Vec3d cU = O1.up2.cross(O1.up1);
        if (cD.dot(cX) < 0) aD = 2*Pi - aD;
        if (cU.dot(cX) < 0) aU = 2*Pi - aU;


        if (J1.name == "elbowLeft") { // test first constraint
            cout << "moveToDistance between: " << J1.name << " -> " << J2.name << endl;
            cout << " aD " << aD << ", aU " << aU << endl;
            // elbow: constrain aD between 0 and 90

            double cA1 = 0.1; // some small angle
            double cA2 = Pi*0.95; // nearly stretched arm

            if (aD < cA1 || aD > cA2) {
                double g = cA1;
                if (aD > cA2) g = cA2;
                double d = computeAngleProjection(aD*0.5, g, distances[dID1], distances[dID2]);
                //Vec3d D = J1.up1 + J1.up2;
                Vec3d D = O1.dir1 + O1.dir2;
                D.normalize();
                J1.pos += D*d;
            }
        }


        /*if (J1.name == "waist") { // test first spring, not usefull like this
            // elbow: constrain aD between 0 and 90

            double cA1 = -0.1; // some small angle
            double cA2 =  0.1; // nearly stretched arm

            if (aU < cA1 || aU > cA2) {
                double g = (cA1+cA2)*0.5;
                double d = computeAngleProjection(aU*0.5, g, distances[dID1], distances[dID2]);
                Vec3d U = O1.up1 + O1.up2;
                U.normalize();
                J1.pos += U*d;
            }
        }*/

        // move point to fix distance
        float li = distances[dID1] / (J2.pos - J1.pos).length();
        movePointTowards(i1, J2.pos, li);
        return pOld;
    };

    /*"
    - define dir1 and dir2 for each joint
    - define up1 and up2 for each joint
    - dot between dir1 and dir2 corresponds to rotation around x
    - dot between up1 and up2 corresponds to rotation around z
    - compute those vectors based on
        - vectors between joint positions
        - reference positions of each joint
    */

    auto doBackAndForth = [&]() {
        for (int i = Nd-1; i > 0; i--) {
            //cout << "\nmove1 " << joints[data.joints[i]].name << endl;
            auto pOld = moveToDistance(i,i+1,i,i-1);
            //auto pOld = joints[data.joints[i]].pos;
            if (i > 0) { // waist 1
                auto R = getRotation(data.joints[i], data.joints[i-1], pOld);
                rotateJoints(data.joints[i-1], data.joints[i], R, data);
            }

            if (i < Nd) {
                auto R = getRotation(data.joints[i], data.joints[i+1], pOld);
                rotateJoints(data.joints[i], data.joints[i+1], R, data);
            }
        }

        for (int i = 1; i <= Nd; i++) {
            //cout << "\nmove2 " << joints[data.joints[i]].name << endl;
            auto pOld = moveToDistance(i,i-1,i-1,i);
            //auto pOld = joints[data.joints[i]].pos;
            if (i < Nd) {
                auto R = getRotation(data.joints[i], data.joints[i+1], pOld);
                rotateJoints(data.joints[i], data.joints[i+1], R, data);
            }

            if (i > 0) { // waist 2
                auto R = getRotation(data.joints[i], data.joints[i-1], pOld);
                rotateJoints(data.joints[i-1], data.joints[i], R, data);
            }
        }
    };

    // basic FABRIK algorithm
    float Dtarget = (targetPos - jointPos(data.joints[0])).length();
    if (Dtarget > sum(distances)) { // position unreachable
        for (int i=1; i<=Nd; i++) {
            jointPos(data.joints[i]) = targetPos;
            moveToDistance(i,i-1,i-1,i);
        }
    } else { // position reachable
        float difA = (jointPos(data.joints.back())-targetPos).length();
        int k=0;
        while (difA > tol) {
            k++; if(k>50) break;
            if (verbose || 1) cout << "\n\nitr " << k << endl;

            int iE = data.joints.size()-1;
            Vec3d pOld = movePointTowards(iE, targetPos, 0);
            auto R = getRotation(data.joints[iE], data.joints[iE-1], pOld);
            rotateJoints(data.joints[iE-1],data.joints[iE],R,data);

            //jointPos(iE) = targetPos;
            doBackAndForth();
            difA = (jointPos(data.joints[iE])-targetPos).length();
            break;
        }
    }
}

void VRSkeleton::resolveSystem(string bone) {
    auto& system = SystemDataMap[bone];
    map<int, Vec3d> oldPos;
    for (auto j : system.joints) oldPos[j] = joints[j].pos;

    // fix system lengths
    map<int, bool> moved;
    bool doContinue = true;
    while(doContinue) {
        doContinue = false;
        for (auto j1 : system.joints) {
            for (auto j2 : system.joints) {
                if (j2 == j1) continue;
                Vec3d M = (jointPos(j1) + jointPos(j2))*0.5;
                Vec3d D = jointPos(j2) - jointPos(j1);
                float d = system.distances[j1][j2];
                float d2 = D.length();
                if (abs(d2-d) > 0.001) {
                    D.normalize();
                    D *= d*0.5;
                    jointPos(j1) = M - D;
                    jointPos(j2) = M + D;
                    moved[j1] = true;
                    moved[j2] = true;
                    doContinue = true;
                }
            }
        }
    }

    // update joint orientations
    auto getOldJointPos = [&](int jID) {
        return oldPos.count(jID) ? oldPos[jID] : joints[jID].pos;
    };

    for (auto m : moved) {
        Vec3d pOld1 = getOldJointPos(m.first);
        Joint& joint = joints[m.first];

        for (auto c : joint.chains) {
            ChainData& chain = ChainDataMap[c];

            int cID1 = joint.chainIDs[c];
            int cID0 = cID1-1;
            int cID2 = cID1+1;

            if (cID0 >= 0) {
                Vec3d pOld0 = getOldJointPos(chain.joints[cID0]);
                auto R = getRotation(m.first, chain.joints[cID0], pOld1, pOld0);
                rotateJoints(chain.joints[cID0], m.first, R, chain);
            }

            if (cID2 < (int)chain.joints.size()) {
                Vec3d pOld2 = getOldJointPos(chain.joints[cID2]);
                auto R = getRotation(m.first, chain.joints[cID2], pOld1, pOld2);
                rotateJoints(m.first, chain.joints[cID2], R, chain);
            }

            /*for (auto j2 : system.joints) {
                if (j2 == m.first) continue;
                if (!chain.orientations.count(j2)) continue;

                Joint& joint2 = joints[j2];
                Vec3d pOld2 = getOldJointPos(j2);
                auto R = getRotation(m.first, j2, pOld1, pOld2);
                rotateJoints(m.first, j2, R, chain);
            }*/
        }
    }
}


vector<int> VRSkeleton::getBonesChain(string endEffector) {
    int e = endEffectors[endEffector].boneID;
    VRPathFinding::Position pR(rootBone);
    VRPathFinding::Position pE(e);
    VRPathFinding pathFinding;
    pathFinding.setGraph(armature);
    auto path = pathFinding.computePath(pR, pE);
    vector<int> chainedBones;
    for (auto p : path) chainedBones.push_back(p.nID);
    return chainedBones;
};

vector<int> VRSkeleton::getJointsChain(vector<int>& chainedBones) {
    //cout << "VRSkeleton::getJointsChain:" << endl;
    vector<int> chainedJoints;
    for (unsigned int i=1; i<chainedBones.size(); i++) {
        int nID1 = chainedBones[i-1];
        int nID2 = chainedBones[i];
        int eID = armature->getEdgeID(nID1, nID2);
        chainedJoints.push_back(eID);
        //cout << " joint: " << joints[eID].name << " between " << bones[nID1].name << " and " << bones[nID2].name << endl;
    }
    return chainedJoints;
};

void VRSkeleton::setupChains() {
    ChainDataMap.clear();
    SystemDataMap.clear();

    for (auto& e : endEffectors) { // chains
        ChainDataMap[e.first] = ChainData();
        ChainDataMap[e.first].chainedBones = getBonesChain(e.first);
        ChainDataMap[e.first].joints = getJointsChain(ChainDataMap[e.first].chainedBones);
        ChainDataMap[e.first].name = e.first;

        for (unsigned int i=1; i<ChainDataMap[e.first].joints.size(); i++) {
            int jID1 = ChainDataMap[e.first].joints[i-1];
            int jID2 = ChainDataMap[e.first].joints[i];
            float d = (jointPos(jID1) - jointPos(jID2)).length();
            ChainDataMap[e.first].distances.push_back( d );
        }

        for (unsigned int i=0; i<ChainDataMap[e.first].joints.size(); i++) {
            int jID = ChainDataMap[e.first].joints[i];
            auto& joint = joints[jID];
            joint.chains.push_back(e.first);
            joint.chainIDs[e.first] = i;
            JointOrientation o;
            o.dir1 = bones[joint.bone1].pose.transform( joint.constraint->getReferenceA()->dir(), false );
            o.dir2 = bones[joint.bone2].pose.transform( joint.constraint->getReferenceB()->dir(), false );
            o.up1  = bones[joint.bone1].pose.transform( joint.constraint->getReferenceA()->up(),  false );
            o.up2  = bones[joint.bone2].pose.transform( joint.constraint->getReferenceB()->up(),  false );
            o.dir1.normalize();
            o.dir2.normalize();
            o.up1.normalize();
            o.up2.normalize();

            ChainDataMap[e.first].orientations[jID] = o;
        }
        //cout << " joint: " << j.second.name << ", bone1: " << bone1.name << ", jPos: " << j.second.pos << ", refA: " << j.second.constraint->getReferenceA()->pos() << endl;
    }

    for (auto& b : bones) { // systems
        auto joints = getBoneJoints(b.first);
        if (joints.size() <= 2) continue; // not a system!

        auto& data = SystemDataMap[b.second.name];
        data.joints = joints;
        //data.joints.push_back(jointsVec3d(0]); // close cycle
        data.bone = b.first;

        for (auto j1 : data.joints) {
            for (auto j2 : data.joints) {
                if (j2 == j1) continue;
                float d = (jointPos(j1) - jointPos(j2)).length();
                data.distances[j1][j2] = d;
            }
        }
    }
}

void VRSkeleton::updateChains() {
    for (auto& e : endEffectors) {
        auto& joint = joints[ e.second.jointID ];
        auto pose = e.second.target;

        if (pose) ChainDataMap[e.first].targetPos = pose->transform( joint.constraint->getReferenceB()->pos() );
        else {
            ChainDataMap[e.first].targetPos = bones[joint.bone2].pose.transform( joint.constraint->getReferenceB()->pos() );
            e.second.target = Pose::create( bones[joint.bone2].pose );
        }
    }
}

void VRSkeleton::mixOrientations() {
    for (auto& j : joints) {
        int N = j.second.chains.size();
        if (N <= 1) continue;

        JointOrientation mean;
        for (auto& c : j.second.chains) {
            auto& chain = ChainDataMap[c];
            JointOrientation& o = chain.orientations[j.first];
            mean.dir1 += o.dir1;
            mean.dir2 += o.dir2;
            mean.up1  += o.up1;
            mean.up2  += o.up2;
        }

        mean.dir1.normalize();
        mean.dir2.normalize();
        mean.up1.normalize();
        mean.up2.normalize();

        for (auto& c : j.second.chains) {
            auto& chain = ChainDataMap[c];
            chain.orientations[j.first] = mean;
        }
    }
}

void VRSkeleton::resolveKinematics() {
    //updateJointPositions();
    //setupChains();
    updateChains();
    if (fabrik) fabrik->iterate();
    updateBones();
    mixOrientations();
    updateGeometry();
}

void VRSkeleton::updateBones() {
    for (auto& b : bones) {
        Bone& bone = b.second;
        auto bJoints = getBoneJoints(b.first);
        if (bJoints.size() <= 1) continue; // ignore EE for now

        vector<Vec3d> pnts1;
        vector<Vec3d> pnts2;

        map<int, Vec3d> jbPositions;

        for (auto e : armature->getOutEdges(b.first)) {
            Vec3d p = joints[e.ID].constraint->getReferenceA()->pos();
            jbPositions[e.ID] = bone.pose.transform( p );
        }

        for (auto e : armature->getInEdges(b.first)) {
            Vec3d p = joints[e.ID].constraint->getReferenceB()->pos();
            jbPositions[e.ID] = bone.pose.transform( p );
        }

        bool verbose = false; //(bone.name == "uArmLeft");

        for (auto j : bJoints) pnts1.push_back( jbPositions[j] );
        for (auto j : bJoints) {
            pnts2.push_back( jointPos(j) );  // TODO: fix points!
            if (verbose) cout << "new joint position, joint: " << joints[j].name << " P: " << joints[j].pos << endl;
        }


        if (verbose) {
            cout << "update bone " << bone.name << ", pose: " << bone.pose.toString() << endl;
            for (auto p1 : pnts1) cout << " p1: " << p1 << endl;
            for (auto p2 : pnts2) cout << " p2: " << p2 << endl;
        }

        VRKabschAlgorithm a;
        a.setPoints1(pnts1);
        a.setPoints2(pnts2);
        a.setSimpleMatches();
        auto M = a.compute(verbose && 0);
        M.mult( bone.pose.asMatrix() );
        bone.pose = Pose(M);
    }

    // set EE bone poses
    for (auto ee : endEffectors) {
        auto& bone = bones[ee.second.boneID];
        auto& joint = joints[ee.second.jointID];

        auto pose = *ee.second.target;
        pose.setPos(Vec3d(0,0,0));
        auto o = pose.transform( joint.constraint->getReferenceB()->pos() );

        bone.pose = pose;
        bone.pose.setPos( joint.pos - o );
    }

    // update up vectors of limbs (2-jointed bones)
    for (auto& b : bones) {
        auto bJoints = getBoneJoints(b.first);
        if (bJoints.size() != 2) continue;
        Vec3d u = ChainDataMap[joints[bJoints[0]].chains[0]].orientations.begin()->second.up2;
        b.second.pose.setUp(u);
    }
}

vector<VRSkeleton::Joint> VRSkeleton::getChain(string endEffector) {
    vector<Joint> res;
    auto bChain = getBonesChain(endEffector);
    auto jChain = getJointsChain(bChain);
    for (auto j : jChain) res.push_back(joints[j]);
    return res;
}

void VRSkeleton::move(string endEffector, PosePtr pose) {
    endEffectors[endEffector].target = pose;
    resolveKinematics();
}

map<string, VRSkeleton::EndEffector> VRSkeleton::getEndEffectors() { return endEffectors; }





