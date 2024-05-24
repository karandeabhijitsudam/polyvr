#include "VRRobotArm.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/math/path.h"
#include "core/objects/VRAnimation.h"
#include "core/scene/VRScene.h"
#include "core/utils/VRFunction.h"
#include "core/utils/toString.h"
#include "core/utils/isNan.h"
#include "core/tools/VRAnalyticGeometry.h"
#include <OpenSG/OSGQuaternion.h>

using namespace OSG;


double clamp(double f, double a = -1, double b = 1) { return f<a ? a : f>b ? b : f; }

VRRobotArm::System::System() {
    base = VRTransform::create("base");

#ifndef WASM
    ageo = VRAnalyticGeometry::create();
    ageo->setLabelParams(0.05, 1, 1, Color4f(1,1,1,1), Color4f(1,1,1,0));
    ageo->hide("SHADOW");
    base->addChild(ageo);
#endif
}

VRRobotArm::System::~System() {}

double VRRobotArm::System::convertAngle(double a, int i) {
    if (i >= angle_directions.size() || i >= angle_offsets.size()) return 0;
    return angle_directions[i]*a + angle_offsets[i]*Pi;
}

struct SystemKuka : VRRobotArm::System {
    PosePtr eePose = Pose::create();
    Vec3d edge0;
    Vec3d edge1;
    Vec3d avec;
    double ang_f = 0;
    double ang_a = 0;
    double ang_b = 0;
    double ang_ba = 0;
    double dist2 = 0;

    SystemKuka() : VRRobotArm::System() {
        angles.resize(6,0);
        angle_targets.resize(6,0);
        lengths = { 0.2, 0.3, 0.3, 0.2, 0.03 };
        genKinematics();

        for (int i=0; i<6; i++) angles[i] = convertAngle(0, i);
        applyAngles();
    }

    ~SystemKuka() {}

    void updateState() {
        eePose = calcForwardKinematics(angle_targets);
        ang_f = angle_targets[0];
        ang_a = angle_targets[1] + Pi*0.5;
        ang_b = Pi  + ang_ba - angle_targets[2];
        edge0 = Vec3d(cos(-ang_f),0,sin(-ang_f));
        // end effector
        float e = ang_a+ang_b-ang_ba; // counter angle
        avec = Vec3d(-cos(e)*sin(ang_f), -sin(e), -cos(e)*cos(ang_f));
        edge1 = eePose->dir().cross(avec);
        edge1.normalize();

        updateAnalytics();
    }

    void updateSystem() {
        dist2 = sqrt(lengths[2]*lengths[2] + axis_offsets[1]*axis_offsets[1]);
        ang_ba = atan(axis_offsets[1] / lengths[2]);

        auto baseUpper = parts[0];
        auto beam1 = parts[1];
        auto elbow = parts[2];
        auto beam2 = parts[3];
        auto wrist = parts[4];
        auto hand  = parts[5];

        // TODO: rotate vectors according to axis parameters, axis, direction, angle offsets
        baseUpper->setFrom(Vec3d(0,lengths[0],0));
        double f = -convertAngle(0,0);
        Vec3d p = Vec3d(sin(f), 0, cos(f)) * axis_offsets[0];
        beam1->setFrom(p);
        elbow->setFrom(Vec3d(0,lengths[1],0));
        beam2->setFrom(Vec3d(0,axis_offsets[1],0));
        p = Vec3d(sin(f), 0, cos(f)) * lengths[2];
        wrist->setFrom(p);
        //hand->rotate(Pi*0.5, Vec3d(1,0,0));
        hand->setOrientation(Vec3d(-1,0,0), Vec3d(0,1,0));
        //wrist->setFrom(Vec3d(lengths[2],0,0));

        applyAngles();
        updateState();
        updateAnalytics();
    }

    void genKinematics() { // kuka
        auto baseUpper = VRTransform::create("baseUpper");
        auto beam1 = VRTransform::create("beam1");
        auto elbow = VRTransform::create("elbow");
        auto beam2 = VRTransform::create("beam2");
        auto wrist = VRTransform::create("wrist");
        auto hand  = VRTransform::create("hand");

        base->addChild(baseUpper);
        baseUpper->addChild(beam1);
        beam1->addChild(elbow);
        elbow->addChild(beam2);
        beam2->addChild(wrist);
        wrist->addChild(hand);
        parts = { baseUpper, beam1, elbow, beam2, wrist, hand };

        updateSystem();
    }

    vector<float> calcReverseKinematics(PosePtr p) { // kuka
        eePose = Pose::create( *p );
        Vec3d pos = p->pos();
        Vec3d dir = p->dir();
        Vec3d up  = p->up();
        dir.normalize();
        up.normalize();
        pos -= dir* lengths[3];

        vector<float> resultingAngles = angle_targets;

        pos[1] -= lengths[0];

        ang_f = atan2(pos[0], pos[2]);
        resultingAngles[0] = ang_f;
        edge0 = Vec3d(cos(-ang_f),0,sin(-ang_f));

        Vec3d aD = Vec3d(sin(ang_f), 0, cos(ang_f));
        pos -= aD * axis_offsets[0];

        float r1 = lengths[1];
        float L = pos.length();
        ang_b = acos( clamp( (L*L-r1*r1-dist2*dist2)/(-2*r1*dist2) ) );
        resultingAngles[2] = -ang_b + Pi + ang_ba;

        ang_a = asin( clamp( dist2*sin(ang_b)/L ) ) + asin( clamp( pos[1]/L ) );
        resultingAngles[1] = ang_a - Pi*0.5;

        // end effector
        float e = ang_a+ang_b-ang_ba; // counter angle
        avec = Vec3d(-cos(e)*sin(ang_f), -sin(e), -cos(e)*cos(ang_f));
        edge1 = dir.cross(avec);
        edge1.normalize();

        float det = avec.dot( edge1.cross(edge0) );
        float g = clamp( -edge1.dot(edge0) );
        resultingAngles[3] = det < 0 ? -acos(g) : acos(g);
        resultingAngles[4] = acos( avec.dot(dir) );

        if (resultingAngles.size() > 4) {
            float det = dir.dot( edge1.cross(up) );
            resultingAngles[5] = det < 0 ? -acos( edge1.dot(up) ) : acos( edge1.dot(up) );
        }

        //calcForwardKinematicsKuka(resultingAngles); // temp, remove once debugged!

        return resultingAngles;
    }

    PosePtr calcForwardKinematics(vector<float> angles) { // kuka
        float r1 = lengths[1];
        float r2 = lengths[2];
        //float b = Pi-angles[2];
        float a = angles[1] + Pi*0.5;
        float f = angles[0];

        float a2 = angles[2];
        float a3 = angles[3];
        float a4 = angles[4];
        float a5 = angles[5];

        Pose pB(Vec3d(0,lengths[0],0)); // ok
        Pose pA0(Vec3d()                        , Vec3d(-sin(f), 0, -cos(f))    , Vec3d(cos(f), 0, -sin(f))); // ok
        Pose pA01(Vec3d(0,0,axis_offsets[0])); // ok
        Pose pA1(Vec3d(-r1*sin(a),0,r1*cos(a))  , Vec3d(-sin(a), 0, cos(a))); // ok
        Pose pA2(Vec3d()                        , Vec3d( sin(a2), 0, cos(a2))); // ok
        Pose pA3(Vec3d(0,0,r2)                  , Vec3d(0,0,-1)                 , Vec3d(-sin(a3), cos(a3), 0)); // ok
        Pose pA4(Vec3d()                        , Vec3d(-sin(a4), 0, -cos(a4))); // ok
        Pose pA5(Vec3d(0,0,lengths[3])          , Vec3d(0,0,1)                 , Vec3d(cos(a5), -sin(a5), 0)); // ok

        auto mB = pB.asMatrix();
        mB.mult(pA0.asMatrix());
        mB.mult(pA01.asMatrix());
        mB.mult(pA1.asMatrix());
        mB.mult(pA2.asMatrix());
        mB.mult(pA3.asMatrix());
        mB.mult(pA4.asMatrix());
        mB.mult(pA5.asMatrix());
        //if (ageo) ageo->setVector(18, Vec3d(), Vec3d(mB[3]), Color3f(0,1,0), "a5");
        //if (ageo) ageo->setVector(19, Vec3d(mB[3]), Vec3d(mB[2])*0.2, Color3f(1,0,1), "a5d");
        //if (ageo) ageo->setVector(20, Vec3d(mB[3]), Vec3d(mB[1])*0.2, Color3f(1,0.5,0), "a5u");
        return Pose::create(mB);
    }

    void updateAnalytics() { // kuka
        Vec3d dir = eePose->dir();
        Vec3d up = eePose->up();

        float sA = 0.05;
        Vec3d ra2 = edge0*2*sA; // rotation axis base joint and elbow
        Vec3d ra3 = edge1*2*sA; // rotation axis wrist

        Vec3d pJ0 = Vec3d(0,lengths[0],0); // base joint
        Vec3d pJ01 = pJ0 + Vec3d(sin(ang_f), 0, cos(ang_f)) * axis_offsets[0]; // base joint
        Vec3d pJ1 = pJ01 + Vec3d(cos(ang_a)*sin(ang_f), sin(ang_a), cos(ang_a)*cos(ang_f)) * lengths[1]; // elbow joint
        double oa = ang_b+ang_a-Pi*0.5-ang_ba;
        Vec3d a10 = Vec3d(cos(oa)*sin(ang_f), sin(oa), cos(oa)*cos(ang_f));
        Vec3d pJ11 = pJ1 + a10 * axis_offsets[1];
        Vec3d pJ2 = pJ11 + avec * dist2; // wrist joint

        // EE
        ageo->setVector(0, Vec3d(), pJ2, Color3f(0.6,0.8,1), "");
        ageo->setVector(1, pJ2, dir*0.1, Color3f(0,0,1), "");
        ageo->setVector(2, pJ2, up*0.1, Color3f(1,0,0), "");

        // rot axis
        ageo->setVector(3, pJ01 - Vec3d(0,sA,0), Vec3d(0,2*sA,0), Color3f(1,1,0.5), "");
        ageo->setVector(4, pJ01 - edge0*sA, ra2, Color3f(1,1,0.5), "");
        ageo->setVector(5, pJ1 - edge0*sA, ra2, Color3f(1,1,0.5), "");
        ageo->setVector(6, pJ2 - edge1*sA, ra3, Color3f(1,1,0.5), "");

        // beams
        ageo->setVector(7, Vec3d(), pJ0, Color3f(1,1,1), "l0");
        ageo->setVector(8, pJ01, pJ1-pJ01, Color3f(1,1,1), "r1");
        ageo->setVector(9, pJ11, pJ2-pJ11, Color3f(1,1,1), "r2");

        ageo->setVector(10, pJ0, pJ01-pJ0, Color3f(0.7,0.7,0.7), "");
        ageo->setVector(11, pJ1, pJ11-pJ1, Color3f(0.7,0.7,0.7), "");
    }

    void applyAngles() {
        //cout << "VRRobotArm::applyAngles " << N << ", " << axis.size() << ", " << angles.size() << ", " << parts.size() << endl;
        for (int i=0; i<6; i++) {
            if (i >= axis.size() || i >= angles.size() || i >= parts.size()) break;
            Vec3d euler;
            euler[axis[i]] = angles[i];
            //cout << " applyAngle " << i << ", " << euler << ", " << parts[i] << endl;
            if (parts[i]) parts[i]->setEuler(euler);
        }
    }
};

struct SystemAubo : VRRobotArm::System {
    PosePtr eePose = Pose::create();
    Vec3d pJ0; // base joint
    Vec3d pJ1; // elbow joint
    Vec3d pJ2; // wrist joint (P3)
    Vec3d Pnt1;
    Vec3d Pnt2;
    Vec3d Pnt3;
    Vec3d edge0;
    Vec3d nplane;
    Vec3d posXZ;

    SystemAubo() : VRRobotArm::System() {
        angles.resize(6,0);
        angle_targets.resize(6,0);
        genKinematics();
    }

    ~SystemAubo() {}

    void updateState() {

    }

    void updateSystem() {

    }

    void genKinematics() { // aubo
        // TODO
        //parts = base->getChildren(true, "", true);
    }

    vector<float> calcReverseKinematics(PosePtr p) { // aubo
        eePose = Pose::create( *p );
        Vec3d pos = p->pos();
        Vec3d dir = p->dir();
        Vec3d up  = p->up();

        // front kinematics
        pos -= dir* lengths[3]; // yield space for tool

        posXZ = Vec3d(pos[0], 0, pos[2]);
        float w = acos(lengths[6]/posXZ.length());
        nplane = posXZ;
        nplane.normalize();
        Quaterniond q(Vec3d(0,1,0), w);
        q.multVec(nplane,nplane);
        //Vec3d n = Vec3d(cos(w),0,sin(w));  // -------------------------- n still WRONG!!

        Vec3d e = dir.cross(nplane); // TODO: handle n // d
        e.normalize();

        Vec3d Pnt1 = p->pos()-dir*lengths[3];
        Vec3d Pnt2 = Pnt1-e*lengths[5];
        Vec3d Pnt3 = Pnt2-nplane*lengths[6];

        vector<float> resultingAngles = angle_targets;

        // main arm
        pos = Pnt3;
        pos[1] -= lengths[0]; // substract base offset
        float r1 = lengths[1];
        float r2 = lengths[2];
        float L = pos.length();
        float b = acos( clamp( (L*L-r1*r1-r2*r2)/(-2*r1*r2) ) );
        resultingAngles[2] = -b + Pi;

        float a = asin( clamp( r2*sin(b)/L ) ) + asin( clamp( pos[1]/L ) );
        resultingAngles[1] = a - Pi*0.5;
        //resultingAngles[1] = a;

        float f = atan2(pos[0], pos[2]);
        resultingAngles[0] = f;

        edge0 = Vec3d(cos(-f),0,sin(-f));


        // last angle connecting both subsystems
        pJ0 = Vec3d(0,lengths[0],0); // base joint
        pJ1 = pJ0 + Vec3d(cos(a)*sin(f), sin(a), cos(a)*cos(f)) * lengths[1]; // elbow joint
        pJ2 = pJ1 - Vec3d(cos(a+b)*sin(f), sin(a+b), cos(a+b)*cos(f)) * lengths[2]; // wrist joint (P3)

        // TODO: works only for a quarter of the angles!
        auto getAngle = [](Vec3d u, Vec3d v, Vec3d w) {
            float a = u.enclosedAngle(v);
            Vec3d d = u.cross(v);
            float k = w.dot(d);
            int s = k >= 0 ? 1 : -1;
            return a*s;
        };

        resultingAngles[3] = getAngle(Pnt1-Pnt2, pJ1-pJ2, Pnt3-Pnt2) - Pi*0.5;
        resultingAngles[4] = getAngle(dir, Pnt3-Pnt2, Pnt2-Pnt1) - Pi*0.5;

        return resultingAngles;
    }

    PosePtr calcForwardKinematics(vector<float> angles) { // aubo
        return 0;//getLastPose();
        /*if (parts.size() < 5) return 0;
        auto pose = parts[4]->getWorldPose();
        pose->setDir(-pose->dir());
        Vec3d p = pose->pos() + pose->dir()*lengths[3];
        pose->setPos(p);
        return pose;*/
    }

    void updateAnalytics() { // aubo
        float sA = 0.05;
        Vec3d dir = eePose->dir();
        Vec3d up = eePose->up();
        Vec3d pos = eePose->pos();

        // EE
        ageo->setVector(0, Vec3d(), pJ2, Color3f(0.6,0.8,1), "");
        ageo->setVector(1, Pnt1, dir*0.1, Color3f(0,0,1), "");
        ageo->setVector(2, Pnt1, up*0.1, Color3f(1,0,0), "");

        // rot axis
        ageo->setVector(3, pJ0 - Vec3d(0,sA,0), Vec3d(0,2*sA,0), Color3f(1,1,0.5), "");
        ageo->setVector(4, pJ0 - edge0*sA, edge0*2*sA, Color3f(1,1,0.5), "");
        ageo->setVector(5, pJ1 - edge0*sA, edge0*2*sA, Color3f(1,1,0.5), "");
        ageo->setVector(6, pJ2 - edge0*sA, edge0*2*sA, Color3f(1,1,0.5), "");

        // beams
        ageo->setVector(7, Vec3d(), pJ0, Color3f(1,1,1), "l0");
        ageo->setVector(8, pJ0, pJ1-pJ0, Color3f(1,1,1), "r1");
        ageo->setVector(9, pJ1, pJ2-pJ1, Color3f(1,1,1), "r2");

        // front kinematics
        ageo->setVector(10, Pnt1, pos-Pnt1, Color3f(1,0,0), "");
        ageo->setVector(11, Pnt2, Pnt1-Pnt2, Color3f(1,0,0), "");
        ageo->setVector(12, Pnt3, Pnt2-Pnt3, Color3f(1,0,0), "");

        ageo->setVector(13, pos, Vec3d(), Color3f(1,0,0), "P");
        ageo->setVector(14, Pnt1, Vec3d(), Color3f(1,0,0), "P1");
        ageo->setVector(15, Pnt2, Vec3d(), Color3f(1,0,0), "P2");
        ageo->setVector(16, Pnt3, Vec3d(), Color3f(1,0,0), "P3");

        // initial rectangle structure
        ageo->setVector(17, Vec3d(), nplane*0.3, Color3f(0,1,0), "N");
        ageo->setVector(18, Vec3d(), posXZ, Color3f(0,1,0), "");
        ageo->setVector(19, posXZ, -nplane*lengths[6], Color3f(0,1,0), "l6");
    }

    void applyAngles() {
        //cout << "VRRobotArm::applyAngles " << N << ", " << axis.size() << ", " << angles.size() << ", " << parts.size() << endl;
        for (int i=0; i<6; i++) {
            if (i >= axis.size() || i >= angles.size() || i >= parts.size()) break;
            Vec3d euler;
            euler[axis[i]] = angles[i];
            //cout << " applyAngle " << i << ", " << euler << ", " << parts[i] << endl;
            if (parts[i]) parts[i]->setEuler(euler);
        }
    }
};

struct SystemDelta : VRRobotArm::System {
    PosePtr eePose = Pose::create();

    SystemDelta() : VRRobotArm::System() {
        angles.resize(4,0);
        angle_targets.resize(4,0);
        genKinematics();
    }

    ~SystemDelta() {}

    void updateState() {

    }

    void updateSystem() {

    }

    void genKinematics() { // delta
        double baseOffset = lengths[0];
        double arm1Length = lengths[1];
        double arm2Length = lengths[2];
        double elbowDistance = lengths[3];
        double starOffset = lengths[4];

        double L = arm1Length + arm2Length;
        double x = baseOffset - starOffset;
        double h = sqrt( L*L - x*x );
        double b = Pi - acos(x / h);

        for (int i=1; i<=3; i++) {
            string is = toString(i);
            auto baseArm = VRTransform::create("baseArm"+is);
            auto elbow1 = VRTransform::create("elbow1"+is);
            auto elbow2 = VRTransform::create("elbow2"+is);
            parts.push_back(baseArm);
            parts.push_back(elbow1);
            parts.push_back(elbow2);

            base->addChild(baseArm);
            baseArm->addChild(elbow1);
            baseArm->addChild(elbow2);

            double a = Pi*2.0/3.0 * (i-1);
            baseArm->setEuler(Vec3d(0,a,0));
            baseArm->move(-baseOffset);
            baseArm->setEuler(Vec3d(b,a,0));
            elbow1->translate(Vec3d(elbowDistance, 0, arm1Length));
            elbow2->translate(Vec3d(-elbowDistance, 0, arm1Length));
        }

        auto beam1 = VRTransform::create("beam1");
        auto beam2 = VRTransform::create("beam2");
        auto hand = VRTransform::create("hand");
        parts.push_back(beam1);
        parts.push_back(beam2);
        parts.push_back(hand);

        base->addChild(beam1);
        beam1->addChild(beam2);

        auto star = VRTransform::create("star");
        parts.push_back(star);
        base->addChild(star);
        star->addChild(hand);

        star->translate(Vec3d(0,-h,0));
        beam2->translate(Vec3d(0,-h,0));
    }

    vector<float> calcReverseKinematics(PosePtr p) { // delta
        eePose = Pose::create( *p );
        Vec3d pos = p->pos();
        Vec3d dir = p->dir();
        Vec3d up  = p->up();
        float L = pos.length();

        double baseOffset = lengths[0];
        double arm1Length = lengths[1];
        double arm2Length = lengths[2];
        double elbowDistance = lengths[3];
        double starOffset = lengths[4];

        // compute the orientation and axial translation of the central rod
        double l1 = 1.0;
        double l2 = 1.0;
        double dl = L-l2; // central axis translation
        Vec3d aDir = pos*(1.0/L); // central axis dir
        Vec3d aUp = up - aDir * up.dot(aDir); // central axis up (rotation around axis)

        // first arm
        double lA = 0.5;
        double lB = 0.7;
        Vec3d aN = Vec3d(1,0,0);

        // project lower arm sphere into upper arm plane
        double d = pos.dot(aN);
        Vec3d cS = pos - aN * d;
        double rS = sqrt(lB*lB - d*d);

        // compute circle circle intersection
        Vec3d t = pos.cross(aN);
        t.normalize(); // both points lie on this line
        double L2 = cS.length();
        double h = 1.0/2.0 + (lA*lA - rS*rS)/(2.0*L2*L2);
        double r = sqrt(lA*lA - h*h*L2*L2); // circle radius
        Vec3d cC = cS * h; // circle center

        // compute upper arm angle
        Vec3d x1 = aN.cross(Vec3d(0,1,0));
        Vec3d e = cC - t * r;
        double a0 = x1.enclosedAngle(e);
        if (a0 > Pi*0.5) {
            e = cC + t * r;
            a0 = x1.enclosedAngle(e);
        }

        // compute lower arm orientation
        Vec3d A2 = pos - e;

        vector<float> resultingAngles = angle_targets;
        return resultingAngles;
    }

    PosePtr calcForwardKinematics(vector<float> angles) { // delta
        return 0; //getLastPose();
        /*if (parts.size() < 5) return 0;
        auto pose = parts[4]->getWorldPose();
        pose->setDir(-pose->dir());
        Vec3d p = pose->pos() + pose->dir()*lengths[3];
        pose->setPos(p);
        return pose;*/
    }

    void updateAnalytics() { // delta
        Vec3d dir = eePose->dir();
        Vec3d up = eePose->up();
        Vec3d pos = eePose->pos();

        // EE
        ageo->setVector(0, Vec3d(), pos, Color3f(0.6,0.8,1), "");
        ageo->setVector(1, pos, dir*0.1, Color3f(0,0,1), "");
        ageo->setVector(2, pos, up*0.1, Color3f(1,0,0), "");

        double baseOffset = lengths[0];
        double arm1Length = lengths[1];
        //double arm2Length = lengths[2];
        //double elbowDistance = lengths[3];
        //double starOffset = lengths[4];

        // arms
        for (int i=1; i<=3; i++) {
            double a = Pi*2.0/3.0 * (i-1);
            Vec3d O = Vec3d(cos(a),0,sin(a)) * baseOffset;
            Vec3d A = Vec3d(sin(a),0,cos(a)) * 0.1;
            ageo->setVector(2+i, O - A*0.5, A, Color3f(1,1,0.5), ""); // rot axis
            ageo->setVector(5+i, Vec3d(), O, Color3f(0.5,0.5,0.5), ""); // arm offset
            ageo->setVector(8+i, O, Vec3d(0,-arm1Length,0), Color3f(0.5,0.5,0.5), ""); // arm TODO
        }

        // beams
        /*ageo->setVector(7, Vec3d(), pJ0, Color3f(1,1,1), "l0");
        ageo->setVector(8, pJ01, pJ1-pJ01, Color3f(1,1,1), "r1");
        ageo->setVector(9, pJ11, pJ2-pJ11, Color3f(1,1,1), "r2");

        ageo->setVector(10, pJ0, pJ01-pJ0, Color3f(0.7,0.7,0.7), "");
        ageo->setVector(11, pJ1, pJ11-pJ1, Color3f(0.7,0.7,0.7), "");*/
    }

    void applyAngles() {
        //cout << "VRRobotArm::applyAngles " << N << ", " << axis.size() << ", " << angles.size() << ", " << parts.size() << endl;
        /*for (int i=0; i<6; i++) {
            if (i >= axis.size() || i >= angles.size() || i >= parts.size()) break;
            Vec3d euler;
            euler[axis[i]] = angles[i];
            //cout << " applyAngle " << i << ", " << euler << ", " << parts[i] << endl;
            if (parts[i]) parts[i]->setEuler(euler);
        }*/
    }
};

VRRobotArm::VRRobotArm(string type) : type(type) {
    if (type == "kuka") system = shared_ptr<SystemKuka>( new SystemKuka() );
    if (type == "aubo") system = shared_ptr<SystemAubo>( new SystemAubo() );
    if (type == "delta") system = shared_ptr<SystemDelta>( new SystemDelta() );
    if (!system) system = shared_ptr<SystemKuka>( new SystemKuka() ); // default

    lastPose = Pose::create();
    animPath = Path::create();
    robotPath = Path::create();
    anim = VRAnimation::create("animOnPath");

    animPtr = VRFunction<float>::create("animOnPath", bind(&VRRobotArm::animOnPath, this, placeholders::_1 ) );
    //anim->addUnownedCallback(animPtr);
    anim->addCallback(animPtr);

    updatePtr = VRUpdateCb::create("run engines", bind(&VRRobotArm::update, this) );
    VRScene::getCurrent()->addUpdateFkt(updatePtr, 999);
}

VRRobotArm::~VRRobotArm() {
    anim->stop();
}

shared_ptr<VRRobotArm> VRRobotArm::create(string type) { return shared_ptr<VRRobotArm>(new VRRobotArm(type)); }

void VRRobotArm::setEventCallback(VRMessageCbPtr mCb) { eventCb = mCb; }

void VRRobotArm::setAngleOffsets(vector<float> offsets) { system->angle_offsets = offsets; system->updateSystem(); }
void VRRobotArm::setAngleDirections(vector<int> dirs) { system->angle_directions = dirs; system->updateSystem(); }
void VRRobotArm::setAxis(vector<int> axis) { system->axis = axis; system->updateSystem(); }
void VRRobotArm::setLengths(vector<float> lengths) { system->lengths = lengths; system->updateSystem(); }
void VRRobotArm::setAxisOffsets(vector<float> offsets) { system->axis_offsets = offsets; system->updateSystem(); }
vector<float> VRRobotArm::getAngles() { return system->angles; }
vector<float> VRRobotArm::getTargetAngles() {
    vector<float> res;
    for (int i=0; i<N; i++) res.push_back(system->convertAngle(system->angle_targets[i], i));
    return res;
}

VRTransformPtr VRRobotArm::getKinematics() { return system->base; }

void VRRobotArm::applyAngles() {
    system->applyAngles();
}

void VRRobotArm::update() { // update robot joint angles
    bool m = false;

    for (int i=0; i<N; i++) {
        double a = system->convertAngle(system->angle_targets[i], i);
        double da = a - system->angles[i];
        if (isNan(da)) continue;
        while (da >  Pi) da -= 2*Pi;
        while (da < -Pi) da += 2*Pi;
        if (abs(da) > 1e-4) m = true;
        system->angles[i] += clamp( da, -maxSpeed, maxSpeed );
    }

    if (m) system->applyAngles();
    if (eventCb && moving && !m) (*eventCb)("stopped");
    if (eventCb && !moving && m) (*eventCb)("moving");
    moving = m;
}

bool VRRobotArm::isMoving() { return anim->isActive() || moving; }

void VRRobotArm::grab(VRTransformPtr obj) {
    if (dragged) drop();
    if (system->parts.size() == 0) return;
    auto ee = system->parts[system->parts.size()-1];
    if (!ee) return;
    obj->drag(ee);
    dragged = obj;
    //cout << "VRRobotArm::grab obj " << obj << ", ee " << ee << endl;
    //for (auto e : parts) cout << "  part " << e << endl;
}

VRTransformPtr VRRobotArm::drop() {
    auto d = dragged;
    if (dragged) dragged->drop();
    dragged = 0;
    return d;
}

/*

Analytic Kinematics Model

Bodies:
- body 0 -> upper base, rotates (Y) above the lower base
- body 1 -> first beam
- body 2 -> elbow part
- body 3 -> second beam,
- body 4 -> wrist, last robot part without tool
- body 5 -> tool base
- body 6 -> tool finger 1
- body 7 -> tool finger 2

Angles:
- a[0] -> angle between lower and upper base parts, main Y rotation of the robot
- a[1] -> angle between upper base and first beam
- a[2] -> angle between first beam and elbow part
- a[3] -> angle between elbow part and second beam
- a[4] -> angle between second beam and tool interface
- a[5] -> angle between tool interface and tool up

Lengths:
- l[0] -> height of robot base
- l[1] -> distance between joint A, B
- l[2] -> distance between joint B, C
- l[3] -> padding of end effector

Computation Parameters:
- L -> length from robot base joint to end effector
- r1/r2 -> lengths 1/2, see above
- b, a, f -> angle_targets
- av -> direction from elbow to wrist joints
- e0, e1 -> elbow/wrist joint axis directions

*/

PosePtr VRRobotArm::calcForwardKinematics(vector<float> angles) {
    return system->calcForwardKinematics(angles);
}

vector<float> VRRobotArm::calcReverseKinematics(PosePtr p) {
    vector<float> resultingAngles = system->calcReverseKinematics(p);
    lastPose = p;
    return resultingAngles;
}

void VRRobotArm::showAnalytics(bool b) {
    showModel = b;
    if (system->ageo) system->ageo->setVisible(b);
}

void VRRobotArm::animOnPath(float t) {
    if (job_queue.size() == 0) { anim->stop(); return; }
    auto job = job_queue.front();
    anim->setDuration(job.d);

    auto updateTargets = [&](float t) {
        auto pose = job.p->getPose(t);
        if (job.po) {
            auto poseO = job.po->getPose(t);
            pose->set(pose->pos(), poseO->dir(), poseO->up());
        } else {
            pose->normalizeOrientationVectors();
        }
        if (!job.local) pose = pose->multLeft(gToL);
        system->angle_targets = calcReverseKinematics(pose);
        system->updateAnalytics();

        //auto fP = calcForwardKinematics(angle_targets);
        //cout << " updateTargets, P: " << pose->toString() << endl;
        //cout << "                F: " << fP->toString() << endl;
        //for (auto a : angle_targets) cout << " " << a << endl;
    };

    t += job.t0;

    if (t >= job.t1) { // finished
        updateTargets(job.t1);
        if (!job.loop) job_queue.pop_front(); // queue next job
        anim->start(0); // restart animation
        return;
    }

    updateTargets(t);
}

void VRRobotArm::addJob(job j) {
    job_queue.push_back(j);
    if (!anim->isActive()) anim->start(0);
}

void VRRobotArm::move() {}
void VRRobotArm::pause() {}
void VRRobotArm::stop() {
    job_queue.clear();
    anim->stop();
}

void VRRobotArm::setAngles(vector<float> angles, bool force) {
    system->angle_targets = angles;
    system->updateState();

    if (force) {
        for (int i=0; i<N; i++) system->angles[i] = system->convertAngle(angles[i], i);
        system->applyAngles();
        if (system->parts.size() >= 6) { // TODO: what is this??
            lastPose = system->parts[0]->getPoseTo(system->parts[5]);
            lastPose->setPos(lastPose->pos() + lastPose->dir() * system->lengths[3]);
        }
    }
}

void VRRobotArm::setSpeed(float s) { animSpeed = s; }
void VRRobotArm::setMaxSpeed(float s) { maxSpeed = s; }

PosePtr VRRobotArm::getLastPose() { return lastPose; }
PosePtr VRRobotArm::getPose() { return getLastPose(); }

bool VRRobotArm::canReach(PosePtr p, bool local) {
    if (!local) {
        updateLGTransforms();
        p = p->multLeft(gToL);
    }

    auto angles = calcReverseKinematics(p);
    auto p2 = calcForwardKinematics(angles);
    float D = p2->pos().dist( p->pos() );
    if (D > 1e-3) return false;

    auto d = p->dir();
    auto d2 = p2->dir();
    d.normalize();
    d2.normalize();
    D = d2.dot(d);
    if (D < 1.0-1e-3) return false;

    return true;
}

void VRRobotArm::moveTo(PosePtr p2, bool local) {
    stop();
    auto p1 = getPose();
    if (!p1) return;
    updateLGTransforms();
    if (!local) p1 = p1->multLeft(lToG);
    //p1->setUp(-p1->up());
    //p1->setUp(Vec3d(0,1,0));

    //cout << "VRRobotArm::moveTo " << p1->toString() << "   ->   " << p2->toString() << endl;

    animPath->clear();
    animPath->addPoint( *p1 );
    animPath->addPoint( *p2 );
    animPath->compute(2);

    /*cout << "moveTo, p1: " << p1->toString() << endl;
    cout << "moveTo, p2: " << p2->toString() << endl;
    cout << "path pos:";  for (auto v : animPath->getPositions())  cout << " " << v; cout << endl;
    cout << "path dirs:"; for (auto v : animPath->getDirections()) cout << " " << v; cout << endl;
    cout << "path ups:"; for (auto v : animPath->getUpVectors()) cout << " " << v; cout << endl;*/


    //addJob( job(animPath, 0, 1, 2*animPath->getLength()) ); // TODO
    addJob( job(animPath, 0, 0, 1, 2*animPath->getLength()/animSpeed, false, local) );
}

void VRRobotArm::setGrab(float g) {
    grabDist = g;
    float l = system->lengths[4]*g;
    Vec3d p; p[0] = l;
    if (system->parts.size() >= 9) {
        if (system->parts[7] && system->parts[8]) {
            system->parts[7]->setFrom( p);
            system->parts[8]->setFrom(-p);
        }
    }
}

void VRRobotArm::moveOnPath(float t0, float t1, bool loop, float durationMultiplier, bool local) {
    auto p0 = robotPath->getPose(t0);
    if (orientationPath) {
        auto o0 = orientationPath->getPose(t0);
        p0->setDir(o0->dir());
        p0->setUp(o0->up());
    }
    moveTo( p0 );
    float T = 2*robotPath->getLength()/animSpeed * durationMultiplier;
    addJob( job(robotPath, orientationPath, t0, t1, T, loop, local) );
}

void VRRobotArm::toggleGrab() { setGrab(1-grabDist); }

void VRRobotArm::updateLGTransforms() {
    auto root = dynamic_pointer_cast<VRTransform>( system->parts[0]->getParent() );
    if (root) {
        lToG = root->getWorldPose();
        gToL = lToG->inverse();
    }
}

void VRRobotArm::setPath(PathPtr p, PathPtr po) {
    robotPath = p;
    orientationPath = po;
}

PathPtr VRRobotArm::getPath() { return robotPath; }
PathPtr VRRobotArm::getOrientationPath() { return orientationPath; }

