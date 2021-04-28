#ifndef VRPIPESYSTEM_H_INCLUDED
#define VRPIPESYSTEM_H_INCLUDED

#include <map>
#include <vector>
#include <OpenSG/OSGConfig.h>
#include "VREngineeringFwd.h"
#include "core/math/VRMathFwd.h"
#include "core/utils/VRFunctionFwd.h"
#include "core/objects/geometry/VRGeometry.h"
#include "addons/Semantics/VRSemanticsFwd.h"

using namespace std;
OSG_BEGIN_NAMESPACE;

class VRPipeSegment {
    public:
        int eID = 0;
        double radius = 0;
        double length = 0;
        double area = 0;
        double volume = 0;
        double density = 1.0;
        double flow = 0.0;
        double dFl = 0.0;
        bool flowBlocked = false;

        double pressure1 = 1.0;
        double pressure2 = 1.0;

        double computeExchange(double hole, VRPipeSegmentPtr other, double dt, bool p1);

    public:
        VRPipeSegment(int eID, double radius, double length);
        ~VRPipeSegment();

        static VRPipeSegmentPtr create(int eID, double radius, double length);

        void handleTank(double& pressure, double otherVolume, double& otherDensity, double dt, bool p1);
        void handleValve(double area, VRPipeSegmentPtr other, double dt, bool p1);
        void handlePump(double performance, bool isOpen, VRPipeSegmentPtr other, double dt, bool p1);

        void addEnergy(double m, double d, bool p1);
        void setLength(double l);
        void computeGeometry();
};

class VRPipeNode {
    public:
        VREntityPtr entity;
        double lastPressureDelta = 0.0;

    public:
        VRPipeNode(VREntityPtr entity);
        ~VRPipeNode();

        static VRPipeNodePtr create(VREntityPtr entity);
};

class VRPipeSystem : public VRGeometry {
	private:
        GraphPtr graph;
        VROntologyPtr ontology;

        VRUpdateCbPtr updateCb;

        bool doVisual = false;
        bool rebuildMesh = true;

        map<int, VRPipeNodePtr> nodes;
        map<string, int> nodesByName;
        map<int, VRPipeSegmentPtr> segments;

        void initOntology();

        vector<VRPipeSegmentPtr> getPipes(int nID);
        vector<VRPipeSegmentPtr> getInPipes(int nID);
        vector<VRPipeSegmentPtr> getOutPipes(int nID);

	public:
		VRPipeSystem();
		~VRPipeSystem();

		static VRPipeSystemPtr create();
		VRPipeSystemPtr ptr();

		int addNode(string name, PosePtr pos, string type, map<string, string> params);
		int addSegment(double radius, int n1, int n2);
		int getNode(string name);
		int getSegment(int n1, int n2);

		void setNodePose(int nID, PosePtr p);
        int disconnect(int nID, int sID);
        int insertSegment(int nID, int sID, float radius);
		void setDoVisual(bool b);

		void update();
		void updateVisual();
		VROntologyPtr getOntology();

		PosePtr getNodePose(int i);
		double getSegmentPressure(int i);
		double getSegmentFlow(int i);
		double getTankPressure(string n);
		double getTankDensity(string n);
		double getTankVolume(string n);
		double getPump(string n);

		void setValve(string n, bool b);
		void setPump(string n, double p);
		void setTankPressure(string n, double p);
		void setTankDensity(string n, double p);

        void printSystem();
};

OSG_END_NAMESPACE;

#endif //VRPIPESYSTEM_H_INCLUDED
