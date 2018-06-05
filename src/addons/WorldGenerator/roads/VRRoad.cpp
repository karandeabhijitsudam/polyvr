#include "VRRoad.h"
#include "VRRoadNetwork.h"
#include "../VRWorldGenerator.h"
#include "../terrain/VRTerrain.h"
#include "core/utils/toString.h"
#include "core/math/path.h"
#include "core/objects/geometry/VRStroke.h"
#include "addons/Semantics/Reasoning/VROntology.h"
#include "addons/Semantics/Reasoning/VRProperty.h"
#include "addons/Semantics/Reasoning/VREntity.h"

#include "addons/RealWorld/Assets/StreetLamp.h"

using namespace OSG;

VRRoad::VRRoad() : VRRoadBase("Road") {}
VRRoad::~VRRoad() {}

VRRoadPtr VRRoad::create() { return VRRoadPtr( new VRRoad() ); }

float VRRoad::getWidth() {
    float width = 0;
    for (auto lane : entity->getAllEntities("lanes")) width += toFloat( lane->get("width")->value );
    return width;
}

VREntityPtr VRRoad::getNodeEntry( VREntityPtr node ) {
    /*string rN = entity->getName();
    string nN = node->getName();
    auto nodeEntry = entity->ontology.lock()->process("q(e):NodeEntry(e);Node("+nN+");Road("+rN+");has("+rN+".path,e);has("+nN+",e)");
    return nodeEntry[0];*/

    for (auto rp : entity->getAllEntities("path")) {
        for (auto rnE : rp->getAllEntities("nodes")) {
            for (auto nE : node->getAllEntities("paths")) {
                if (rnE == nE) return nE;
            }
        }
    }
    return 0;
}

bool VRRoad::hasMarkings() {
    string type = "road";
    if (auto t = getEntity()->get("type")) type = t->value;
    return (type != "unclassified" && type != "service" && type != "footway");
}

PosePtr VRRoad::getRightEdge(Vec3d pos) {
    auto path = toPath(getEntity()->getEntity("path"), 16);
    float t = path->getClosestPoint(pos); // get nearest road path position to pos
    auto pose = path->getPose(t);
    Vec3d p = pose->pos() + pose->x()*(getWidth()*0.5 + offsetIn);
    pose->setPos(p);
    return pose;
}

PosePtr VRRoad::getPosition(float t) {
    vector<PathPtr> paths;
    for (auto p : entity->getAllEntities("path")) {
        paths.push_back( toPath(p,32) );
    }
    return paths[0]->getPose(t);
}

VRRoad::edgePoint& VRRoad::getEdgePoints( VREntityPtr node ) {
    if (edgePoints.count(node) == 0) {
        float width = getWidth();
        VREntityPtr rEntry = getNodeEntry( node );
        Vec3d norm = rEntry->getVec3("direction") * toInt(rEntry->get("sign")->value);
        Vec3d x = Vec3d(0,1,0).cross(norm);
        x.normalize();
        Vec3d pNode = node->getVec3("position");
        Vec3d p1 = pNode - x * 0.5 * width; // right
        Vec3d p2 = pNode + x * 0.5 * width; // left
        edgePoints[node] = edgePoint(p1,p2,norm,rEntry);
    }
    return edgePoints[node];
}

vector<VRRoadPtr> VRRoad::splitAtIntersections(VRRoadNetworkPtr network) { // TODO: refactor the code a bit
    vector<VRRoadPtr> roads;
    auto e = getEntity();
    auto path = e->getEntity("path");
    auto nodes = path->getAllEntities("nodes");
    for (int i=1; i<nodes.size()-1; i++) {
        auto node = nodes[i]->getEntity("node");
        int N = node->getAllEntities("paths").size();
        if (N <= 1) continue;

        // shorten old road path and add nodes to new road path
        auto npath = path->copy();
        path->clear("nodes");
        npath->clear("nodes");
        for (int j=0; j<=i; j++) {
            path->add("nodes", nodes[j]->getName());
        }
        nodes[i]->set("sign", "1"); // last node of old path

        auto ne = nodes[i]->copy(); // new node entry
        ne->set("sign", "-1"); // first node of new path
        npath->add("nodes", ne->getName());
        ne->set("path", npath->getName());
        ne->getEntity("node")->add("paths", ne->getName());
        for (int j=i+1; j<nodes.size(); j++) {
            npath->add("nodes", nodes[j]->getName());
            nodes[j]->set("path", npath->getName());
        }

        // copy road
        int rID = network->getRoadID();
        auto r = create();
        r->setWorld(world.lock());
        auto re = e->copy();
        re->set("path", npath->getName());
        re->set("ID", toString(rID));
        r->setEntity(re);
        r->ontology = ontology;
        roads.push_back(r);

        re->clear("lanes");
        for (auto l : e->getAllEntities("lanes")) {
            auto lc = l->copy();
            lc->set("road", re->getName());
            re->add("lanes", lc->getName());
        }

        auto splits = r->splitAtIntersections(network);
        for (auto s : splits) roads.push_back(s);
        break;
    }
    return roads;
}

VRGeometryPtr VRRoad::createGeometry() {
    auto w = world.lock();
    if (!w) return 0;
    auto roads = w->getRoadNetwork();
    if (!roads) return 0;

    auto strokeGeometry = [&]() -> VRGeometryPtr {
	    float width = getWidth();
		float W = width*0.5;
		vector<Vec3d> profile;
		profile.push_back(Vec3d(-W,0,0));
		profile.push_back(Vec3d(W,0,0));

		auto geo = VRStroke::create("road");
		vector<PathPtr> paths;
		for (auto p : entity->getAllEntities("path")) {
            auto path = toPath(p,12);

            if(offsetIn!=0 || offsetOut!=0){
                for (int zz=0; zz<path->size(); zz++) {
                    Vec3d p = path->getPoint(zz).pos();
                    Vec3d n = path->getPoint(zz).dir();
                    Vec3d x = path->getPoint(zz).x();
                    auto po = path->getPoint(zz);
                    x.normalize();
                    float offsetter = offsetIn*(1.0-(float(zz)/(float(path->size())-1.0))) + offsetOut*(float(zz)/(float(path->size())-1.0));
                    if (zz>0 && zz<path->getPoints().size()-1) offsetter = 0; //hack, should be more intelligently solved at further dev
                    po.setPos(x*offsetter  + p);
                    path->setPoint(zz,po);
                };
                path->compute(12);
            }
            paths.push_back( path );
		}
		geo->setPaths( paths );
		geo->strokeProfile(profile, 0, 0);
		if (!geo->getMesh()) return 0;
		if (auto t = terrain.lock()) t->elevateVertices(geo, roads->getTerrainOffset());
		return geo;
	};

	auto geo = strokeGeometry();
	if (!geo) return 0;
	setupTexCoords( geo, entity );
	addChild(geo);
	return geo;
}


void VRRoad::computeMarkings() {
    if (!hasMarkings()) return;
    auto w = world.lock();
    if (!w) return;
    auto roads = w->getRoadNetwork();
    if (!roads) return;
    float mw = roads->getMarkingsWidth();

    // road data
    vector<VREntityPtr> nodes;
    vector<Vec3d> normals;
    VREntityPtr pathEnt = entity->getEntity("path");
    if (!pathEnt) return;

    VREntityPtr nodeEntryIn = pathEnt->getEntity("nodes",0);
    VREntityPtr nodeEntryOut = pathEnt->getEntity("nodes",1);
    VREntityPtr nodeIn = nodeEntryIn->getEntity("node");
    VREntityPtr nodeOut = nodeEntryOut->getEntity("node");
    Vec3d normIn = nodeEntryIn->getVec3("direction");
    Vec3d normOut = nodeEntryOut->getVec3("direction");

    float roadWidth = getWidth();
    auto lanes = entity->getAllEntities("lanes");
    int Nlanes = lanes.size();

    auto add = [&](Vec3d pos, Vec3d n) {
        if (auto t = terrain.lock()) t->elevatePoint(pos, 0.06);
        if (auto t = terrain.lock()) t->projectTangent(n, pos);
        nodes.push_back(addNode(0, pos));
        normals.push_back(n);
    };

    // compute markings nodes
    auto path = toPath(pathEnt, 12);
    int zz=0;
    for (auto point : path->getPoints()) {
        Vec3d p = point.pos();
        Vec3d n = point.dir();
        Vec3d x = point.x();
        x.normalize();
        float offsetter = offsetIn*(1.0-(float(zz)/(float(path->size())-1.0))) + offsetOut*(float(zz)/(float(path->size())-1.0));
        float offsetterOld = offsetter;
        if (zz>0 && zz<path->getPoints().size()-1) offsetter = 0; //hack, should be more intelligently solved at further dev
        float widthSum = -roadWidth*0.5 - offsetter;
        for (int li=0; li<Nlanes; li++) {
            auto lane = lanes[li];
            float width = toFloat( lane->get("width")->value );
            float k = widthSum;
            if (li == 0) k += mw*0.5;
            else if (lanes[li-1]->is_a("ParkingLane")) {
                k += mw*0.5;
                offsetter = toFloat(lanes[li-1]->get("width")->value)*0.5;
                //offsetIn = offsetter;
                //offsetOut = offsetter;
                cout<<"ParkingLane "<<offsetterOld<<" "<<offsetIn<<" "<<offsetOut<<" "<< k <<endl;
            }
            add(-x*k + p, n);
            widthSum += width;
        }
        add(-x*(roadWidth*0.5 - mw*0.5 - offsetter ) + p, n);
        zz+=1;
    }

    // markings
    int pathN = path->size();
    int lastDir = 0;

    for (int li=0; li<Nlanes+1; li++) {
        vector<VREntityPtr> nodes2;
        vector<Vec3d> normals2;
        for (int pi=0; pi<pathN; pi++) {
            int i = pi*(Nlanes+1)+li;
            nodes2.push_back( nodes[i] );
            normals2.push_back( normals[i] );
        }
        auto mL = addPath("RoadMarking", name, nodes2, normals2);
        mL->set("width", toString(mw));
        entity->add("markings", mL->getName());

        if (li != Nlanes) {
            auto lane = lanes[li];
            if (!lane->is_a("Lane")) { lastDir = 0; continue; }

            // dotted lines between parallel lanes
            int direction = toInt( lane->get("direction")->value );
            if (li != 0 && lastDir*direction > 0) {
                mL->set("style", "dashed");
                mL->set("dashLength", "2");
            }
            lastDir = direction;

            // parking lanes
            if (li > 0) if (lanes[li-1]->is_a("ParkingLane")) mL->set("dashLength", "1");

            if (lane->is_a("ParkingLane")) {
                auto linePath = toPath(mL, 12);
                int N = lane->getValue<int>("capacity", -1);
                float W = lane->getValue<float>("width", 0);
                for (int si = 0; si < N+1; si++) {
                    auto pose = linePath->getPose(si*1.0/N);
                    auto n = -pose->x();
                    n.normalize();
                    auto p1 = pose->pos();
                    if (si == 0) p1 += pose->dir()*mw*0.5;
                    if (si == N) p1 -= pose->dir()*mw*0.5;
                    auto p2 = p1 + n*W;

                    auto mL = addPath("RoadMarking", name, { addNode(0,p1), addNode(0,p2) }, {n,n} );
                    mL->set("width", toString(mw));
                    entity->add("markings", mL->getName());
                    mL->set("style", "solid");
                    mL->set("dashLength", "0");
                }
            }
        }
    }
}

void VRRoad::addParkingLane( int direction, float width, int capacity, string type ) {
    if (auto o = ontology.lock()) {
        auto l = o->addEntity( entity->getName()+"Lane", "ParkingLane");
        l->set("width", toString(width));
        l->set("direction", toString(direction));
        entity->add("lanes", l->getName());
        l->set("road", entity->getName());
        l->set("capacity", toString(capacity));
    }
}

VREntityPtr VRRoad::addTrafficLight() {
    if (auto o = ontology.lock()) {
        auto roadEnt = getEntity();
        auto signalEnt = o->addEntity("trafficlight", "TrafficLight");
        roadEnt->add("signs",signalEnt->getName());
        signalEnt->set("road",roadEnt->getName());
        return signalEnt;
    }
    return 0;
}

void VRRoad::setOffsetIn(float o) { offsetIn = o; }
void VRRoad::setOffsetOut(float o) { offsetOut = o; }


