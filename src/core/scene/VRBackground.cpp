#include "VRBackground.h"

#include <OpenSG/OSGBackground.h>
#include <OpenSG/OSGSkyBackground.h>
#include <OpenSG/OSGSimpleTexturedMaterial.h>
#include <OpenSG/OSGTextureBackground.h>
#include <OpenSG/OSGSolidBackground.h>
#include <OpenSG/OSGImage.h>

#include "core/setup/VRSetup.h"
#include "core/scene/VRScene.h"
#include "core/scene/import/VRImport.h"
#include "core/utils/toString.h"
#include "core/utils/VRStorage_template.h"
#include "core/utils/system/VRSystem.h"
#include "core/objects/geometry/VRSky.h"
#include "core/tools/VRTextureRenderer.h"

OSG_BEGIN_NAMESPACE;
using namespace std;


class VRBackgroundBase {
    public:
        BackgroundRecPtr bg;
        SkyBackgroundRecPtr skybg;
        SolidBackgroundRecPtr sbg;
        TextureBackgroundRecPtr tbg;
        VRSkyPtr sky;
        vector<ImageMTRecPtr> skyImgs;

        int type;
        string path;
        string format;
        Color3f color;

        TextureObjChunkMTRecPtr createSkyTexture();
        void updateSkyTextures();
        void initSky();

        void readImage(int ID, string path);
        void updateImgTexture();
        void initImg();
};

TextureObjChunkMTRecPtr VRBackgroundBase::createSkyTexture() {
    ImageMTRecPtr image = Image::create();
    skyImgs.push_back(image);
    TextureObjChunkMTRecPtr chunk = TextureObjChunk::create();

    chunk->setImage(image);
    chunk->setMinFilter( GL_LINEAR_MIPMAP_LINEAR );
    chunk->setMagFilter( GL_LINEAR );
    chunk->setWrapS( GL_CLAMP_TO_EDGE );//GL_CLAMP //GL_REPEAT
    chunk->setWrapT( GL_CLAMP_TO_EDGE );//GL_CLAMP_TO_EDGE //GL_REPEAT
    //chunk->setEnvMode( GL_REPLACE );

    return chunk;
}

string normPath(string p) {
    if (exists(p)) return canonical(p);
    return p;
}

void VRBackgroundBase::readImage(int ID, string path) {
    if ( !VRImport::get()->checkPath( path ) ) {
        cout << " Error in VRBackground read texture: '" << path << "' not found" << endl;
        return;
    }

    skyImgs[ID]->read(normPath(path).c_str());
}

void VRBackgroundBase::updateSkyTextures() {
    if (path == "") return;

    readImage(0, path + "_back" + format);
    readImage(1, path + "_front" + format);
    readImage(2, path + "_left" + format);
    readImage(3, path + "_right" + format);
    readImage(4, path + "_down" + format);
    readImage(5, path + "_up" + format);
}

void VRBackgroundBase::initSky() {
    skybg = SkyBackground::create();
    skybg->setBackTexture( createSkyTexture() );
    skybg->setFrontTexture( createSkyTexture() );
    skybg->setLeftTexture( createSkyTexture() );
    skybg->setRightTexture( createSkyTexture() );
    skybg->setBottomTexture( createSkyTexture() );
    skybg->setTopTexture( createSkyTexture() );
}

void VRBackgroundBase::updateImgTexture() {
    if (path == "") return;
    readImage(6, path);
}

void VRBackgroundBase::initImg() {
    tbg = TextureBackground::create();
    tbg->setTexture( createSkyTexture() );
}


VRBackground::VRBackground () {
    base = shared_ptr<VRBackgroundBase>( new VRBackgroundBase() );

    base->sbg = SolidBackground::create();
    base->initSky();
    base->initImg();

    base->color = Color3f(0.6, 0.6, 0.6);
    setBackground(SOLID);

    base->format = ".png";

    setStorageType("Background");
    store("type", &base->type);
    store("color", &base->color);
    store("path", &base->path);
    store("format", &base->format);
}

VRBackground::~VRBackground() {}

VRSkyPtr VRBackground::getSky() { return base->sky; }

void VRBackground::setBackground(TYPE t) {
    base->type = t;
    if (base->sky) base->sky->hide();
    switch(t) {
        case SOLID:
            base->bg = base->sbg;
            base->sbg->setColor(base->color);
            //base->sbg->setAlpha(0.1); // just a test
            break;
        case IMAGE:
            base->bg = base->tbg;
            base->updateImgTexture();
            break;
        case SKYBOX:
            base->bg = base->skybg;
            base->updateSkyTextures();
            break;
        case SKY:
            base->bg = base->sbg; // set solid bg
            base->sbg->setColor(Color3f(0,0,0));
            if (!base->sky) {
                base->sky = VRSky::create();
                auto scene = VRScene::getCurrent();
                scene->getRoot()->addLink( base->sky );
            } else base->sky->reloadShader();
            base->sky->show();
            break;
    }

    updateBackground();
}

void VRBackground::setBackgroundColor(Color3f c) {
    base->color = c;
    base->sbg->setColor(c);
    setBackground(TYPE(base->type));
}

void VRBackground::setBackgroundPath(string s) {
    base->path = s;
    if (base->type == IMAGE) base->updateImgTexture();
    if (base->type == SKYBOX) base->updateSkyTextures();
    setBackground(TYPE(base->type));
}

BackgroundRecPtr VRBackground::getBackground() { return base->bg; }
void VRBackground::setSkyBGExtension(string f) { base->format = f; base->updateSkyTextures(); }
string VRBackground::getSkyBGExtension() { return base->format; }
VRBackground::TYPE VRBackground::getBackgroundType() { return TYPE(base->type); }
Color3f VRBackground::getBackgroundColor() { return base->color; }
string VRBackground::getBackgroundPath() { return base->path; }

void VRBackground::updateBackground() {
    auto setup = VRSetup::getCurrent();
    if (setup) setup->setViewBackground(getBackground());
    VRTextureRenderer::updateSceneBackground();
}

void VRBackground::update() { setBackground(TYPE(base->type)); }


OSG_END_NAMESPACE;
