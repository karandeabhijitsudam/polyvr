#ifndef VRSCENEGRAPHINTERFACE_H_INCLUDED
#define VRSCENEGRAPHINTERFACE_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include <map>

#include "core/objects/object/VRObject.h"
#include "core/scene/VRSceneFwd.h"
#include "core/networking/VRNetworkingFwd.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRScenegraphInterface : public VRObject {
    private:
        int port = 5555;

        VRSocketPtr socket;
        VRFunction<void*>* cb = 0;

        void resetWebsocket();
        void ws_callback(void* args);

    public:
        VRScenegraphInterface(string name);
        ~VRScenegraphInterface();

        static VRScenegraphInterfacePtr create(string name = "ScenegraphInterface");
        VRScenegraphInterfacePtr ptr();

        void clear();

        void setPort(int p);
};

OSG_END_NAMESPACE;

#endif // VRSCENEGRAPHINTERFACE_H_INCLUDED
