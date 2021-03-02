#ifndef VRATLAS_H
#define VRATLAS_H

#include <string>
#include <OpenSG/OSGConfig.h>
#include "core/objects/VRObjectFwd.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/scene/VRSceneManager.h"
#include "core/objects/VRTransform.h"
#include "../terrain/VRTerrain.h"
#include "GISFwd.h"

using namespace std;
OSG_BEGIN_NAMESPACE;

class VRAtlas : public std::enable_shared_from_this<VRAtlas>  {
    private:
        enum TYPE {
            INNERQUAD = 0,
            INNERRING = 1,
            OUTERRING = 2
        };
        struct Boundary {
            double minEast = 0.0;
            double maxEast = 0.0;
            double minNorth = 0.0;
            double maxNorth = 0.0;

            Boundary(double minEast, double maxEast, double minNorth, double maxNorth);
            Boundary();
            ~Boundary();
        };
        struct Patch {
            string id;
            int LODlvl;
            VRTerrainPtr terrain;
            Vec2d coords = Vec2d(0,0);

            Patch(string sid, int lvl, VRTerrainPtr ter);
            Patch();
            ~Patch();
        };
        struct Level {
            int LODlvl;
            float edgeLength;
            Vec2i shift = Vec2i(0,0);
            Vec3d currentOrigin = Vec3d(0,0,0);
            int type;
            vector<vector<Patch>> patches;
            Level(int t, int lvl);
            Level();
            ~Level();
        };
        struct Layout {
            int currentLODlvl = 0;
            int currentMaxLODlvl = 0;
            Vec3d origin = Vec3d(0,0,0);
            list<Level> levels;
            Level innerQuad;
            Level innerRing;
            Level outerRing;
            list<Patch> toDestroy;
            list<Patch> toGenerate;
            void shiftEastIns(Level& lev, list<Level>::iterator it);
            void shiftEastOut(Level& lev, list<Level>::iterator it);
            void shiftWestIns(Level& lev, list<Level>::iterator it);
            void shiftWestOut(Level& lev, list<Level>::iterator it);
            void shiftNorthIns(Level& lev, list<Level>::iterator it);
            void shiftNorthOut(Level& lev, list<Level>::iterator it);
            void shiftSouthIns(Level& lev, list<Level>::iterator it);
            void shiftSouthOut(Level& lev, list<Level>::iterator it);
            void setCoords(Patch& pat, Vec3d co3);
            Layout();
            ~Layout();
        };
        float size = 100.0;
        float LODviewHeight = 500.0;
        Vec2d atlasOrigin = Vec2d(0.0,0.0);
        Boundary bounds;
        int LODMax = 0;
        int patchcount = 0;
        bool stop = false;

        string filepath;
        VRTransformPtr atlas;
        VRUpdateCbPtr updatePtr;
        string serverURL = "";
        string localPathOrtho = "";
        string localPathHeight = "";
        Layout layout;
        void update();
        void downSize();
        void upSize();
        void addInnerQuad(int lvl);
        void addInnerRing(int lvl);
        void addOuterRing(int lvl);
        VRGeometryPtr generatePatch(string id);
        VRTerrainPtr generateTerrain(string id, int lvlh);

    public:
        VRAtlas();
        ~VRAtlas();
        static VRAtlasPtr create();
		//VRAtlasPtr ptr();

        VRTransformPtr setup();
        void setCoordOrigin(double east, double north);
        void setBoundary(double minEast, double maxEast, double minNorth, double maxNorth);
        void setServerURL(string url);
        void setLocalPaths(string ortho, string height);
        void test();
        void toggleUpdater();
};

OSG_END_NAMESPACE;

#endif // VRATLAS_H
