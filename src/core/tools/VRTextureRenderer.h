#ifndef VRTEXTURERENDERER_H_INCLUDED
#define VRTEXTURERENDERER_H_INCLUDED

#include <OpenSG/OSGColor.h>
#include "core/objects/VRObjectFwd.h"
#include "core/tools/VRToolsFwd.h"
#include "core/objects/object/VRObject.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRTextureRenderer : public VRObject {
    public:
        enum CHANNEL {
            RENDER = 0,
            DIFFUSE = 1,
            NORMAL = 2
        };

    private:
        struct Data;
        Data* data = 0;
        VRMaterialPtr mat = 0;
        VRTexturePtr fbotex = 0;
        VRCameraPtr cam = 0;

        void setChannelFP(string fp);
        void resetChannelFP();

        map<CHANNEL, map<VRMaterial*, VRMaterialPtr>> substitutes;
        map<VRMaterial*, VRMaterialPtr> originalMaterials;

        void setChannelSubstitutes(CHANNEL c);
        void resetChannelSubstitutes();
        void test();

    public:
        VRTextureRenderer(string name);
        ~VRTextureRenderer();

        static VRTextureRendererPtr create(string name = "textureRenderer");

        void setup(VRCameraPtr cam, int width, int height, bool alpha = false);
        void setMaterialSubstitutes(map<VRMaterial*, VRMaterialPtr> substitutes, CHANNEL c);
        void setBackground(Color3f c);

        void setActive(bool b);
        VRMaterialPtr getMaterial();
        VRCameraPtr getCamera();

        VRTexturePtr renderOnce(CHANNEL c = RENDER);
        vector<VRTexturePtr> createCubeMaps();
        VRMaterialPtr createTextureLod(VRObjectPtr scene, PosePtr cam, int res, float aspect, float fov, Color3f bg);
};

OSG_END_NAMESPACE;

#endif // VRTEXTURERENDERER_H_INCLUDED
