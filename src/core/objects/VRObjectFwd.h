#ifndef VROBJECTFWD_H_INCLUDED
#define VROBJECTFWD_H_INCLUDED

#include "core/utils/VRFwdDeclTemplate.h"

ptrFwd(VRPrimitive);

namespace OSG {

// objects
ptrFwd(OSGCore);
ptrFwd(OSGObject);
ptrFwd(OSGTransform);
ptrFwd(OSGGeometry);
ptrFwd(OSGMaterial);
ptrFwd(OSGCamera);

ptrFwd(VRStorage);
ptrFwd(VRStorageContext);
ptrFwd(VRObject);
ptrFwd(VRTransform);
ptrFwd(VRGeometry);
ptrFwd(VRGeoData);
ptrFwd(VRCamera);
ptrFwd(VRLod);
ptrFwd(VRLodEvent);
ptrFwd(VRLodLeaf);
ptrFwd(VRSprite);
ptrFwd(VRSpriteResizeTool);
ptrFwd(VRMultiGrid);
ptrFwd(VRStroke);
ptrFwd(VRLight);
ptrFwd(VRLightBeacon);
ptrFwd(VRGroup);
ptrFwd(VRStage);
ptrFwd(VRSky);
ptrFwd(VRRain);
ptrFwd(VRRainCarWindshield);//temp
ptrFwd(VRPointCloud);
ptrFwd(VRSyncNode);

// other
ptrFwd(VRBackground);
ptrFwd(VRBackgroundBase);
ptrFwd(VRConstraint);
ptrFwd(VRAnimation);
ptrFwd(VRKeyFrameAnimation);

// addons
ptrFwd(VRLodTree);
ptrFwd(VRMetaBalls);
ptrFwd(VRBlinds);
ptrFwd(VROpening);
ptrFwd(VRTree);
ptrFwd(VRHandGeo)
ptrFwd(VRNumberingEngine);
ptrFwd(VRLODSpace);

}

#endif // VROBJECTFWD_H_INCLUDED
