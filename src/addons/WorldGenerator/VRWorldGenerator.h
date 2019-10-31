#ifndef VRWORLDGENERATOR_H_INCLUDED
#define VRWORLDGENERATOR_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include "addons/Semantics/VRSemanticsFwd.h"
#include "addons/WorldGenerator/GIS/GISFwd.h"
#include "addons/WorldGenerator/VRWorldGeneratorFwd.h"
#include "addons/RealWorld/VRRealWorldFwd.h"
#include "core/math/VRMathFwd.h"
#include "core/objects/VRTransform.h"
#include "addons/Bullet/VRPhysicsFwd.h"

using namespace std;
OSG_BEGIN_NAMESPACE;

class VRWorldGenerator : public VRTransform {
    public:
        struct OsmEntity {
            vector<Vec3d> pnts;
            map<string, string> tags;
        };

        ptrRFctFwd( VRUserGen, OsmEntity, bool );

    private:
        VRLodTreePtr lodTree;
        VRSpatialCollisionManagerPtr collisionShape;
        VROntologyPtr ontology;
        VRPlanetPtr planet;
        VRObjectManagerPtr assets;
        VRNaturePtr nature;
        VRTerrainPtr terrain;
        map<string, VRMaterialPtr> materials;
        map<VREntityPtr, VRGeometryPtr> miscAreaByEnt;
        VRRoadNetworkPtr roads;
        VRTrafficSignsPtr trafficSigns;
        VRDistrictPtr district;
        OSMMapPtr osmMap;
        Vec2d coords;
        VRUserGenCbPtr userCbPtr;

        void processOSMMap(double subN = -1, double subE = -1, double subSize = -1);
        void init();
        void initMinimum();

    public:
        VRWorldGenerator();
        ~VRWorldGenerator();

        static VRWorldGeneratorPtr create();
        static VRWorldGeneratorPtr create(int meta);
        VRWorldGeneratorPtr ptr();

        void setOntology(VROntologyPtr ontology);
        void setPlanet(VRPlanetPtr planet, Vec2d coords);
        void addAsset( string name, VRTransformPtr geo );
        void addMaterial( string name, VRMaterialPtr mat );
        void addOSMMap(string path, double subN = -1, double subE = -1, double subSize = -1);
        void readOSMMap(string path);
        void reloadOSMMap(double subN = -1, double subE = -1, double subSize = -1);
        void clear();

        VRLodTreePtr getLodTree();
        VROntologyPtr getOntology();
        VRPlanetPtr getPlanet();
        Vec2d getPlanetCoords();
        VRRoadNetworkPtr getRoadNetwork();
        VRTrafficSignsPtr getTrafficSigns();
        VRObjectManagerPtr getAssetManager();
        VRNaturePtr getNature();
        VRTerrainPtr getTerrain();
        VRDistrictPtr getDistrict();
        VRMaterialPtr getMaterial(string name);
        VRGeometryPtr getMiscArea(VREntityPtr mEnt);

        void setupPhysics();
        void updatePhysics(Boundingbox box);
        VRSpatialCollisionManagerPtr getPhysicsSystem();

        void setUserCallback(VRUserGenCbPtr cb);

        string getStats();
};

OSG_END_NAMESPACE;

#endif // VRWORLDGENERATOR_H_INCLUDED
