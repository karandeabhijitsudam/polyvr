#include "ART.h"
#include "core/scene/VRThreadManager.h"
#include "core/setup/VRSetupManager.h"
#include "core/setup/VRSetup.h"
#include "core/utils/VROptions.h"
#include "core/scene/VRSceneManager.h"
#include "core/utils/toString.h"
#include <libxml++/nodes/element.h>
#include "DTrack.h"
#include "core/setup/devices/VRFlystick.h"
#include "core/utils/VRFunction.h"
#include "core/objects/VRTransform.h"
#include "core/math/coordinates.h"
#include "core/utils/VRStorage_template.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

ART_device::ART_device() { setName("ARTDevice"); }
ART_device::ART_device(int ID, int type) : ID(ID), type(type) { setName("ARTDevice"); init(); }
int ART_device::key() { return key(ID, type); }
int ART_device::key(int ID, int type) { return ID*1000 + type; }

void ART_device::init() {
    if (type != 1) ent = new VRTransform("ART_tracker");
    if (type == 1) {
        dev = new VRFlystick();
        ent = dev->getBeacon();
        VRSetupManager::getCurrent()->addDevice(dev);
    }
}

void ART_device::update() {
    if (ent) ent->setMatrix(m);
    if (dev) dev->update(buttons, joysticks);
}


ART::ART() {
    VRSceneManager::get()->addUpdateFkt(getARTUpdateFkt());

    store("active", &active);
    store("port", &port);
    store("offset", &offset);
    store("up", &up);
}

ART::~ART() {}

template<typename dev>
void ART::getMatrix(dev t, Matrix& m) {
    if (t.quality <= 0) return;
    m[0] = Vec4f(t.rot[0], t.rot[1], t.rot[2], 1); // orientation
    m[1] = Vec4f(t.rot[3], t.rot[4], t.rot[5], 1);
    m[2] = Vec4f(t.rot[6], t.rot[7], t.rot[8], 1);
    m[3] = Vec4f(t.loc[0], t.loc[1], t.loc[2], 1); // position
}

void ART::scan(int type, int N) {
    if (type < 0) {
        scan(0, dtrack->get_num_body());
        scan(1, dtrack->get_num_flystick());
        scan(2, dtrack->get_num_hand());
        scan(3, dtrack->get_num_meatool());
        //scan(4, dtrack->get_num_marker());
        return;
    }

    for (int i=0; i<N; i++) {
        int k = ART_device::key(i,type);
        if (devices.count(k) == 0) devices[k] = new ART_device(i,type);
        getMatrix(dtrack->get_body(i), devices[k]->m);

        if (type == 1) {
            auto fly = dtrack->get_flystick(i);
            devices[k]->buttons = vector<int>(fly.button, &fly.button[fly.num_button-1]);
            devices[k]->joysticks = vector<float>(fly.joystick, &fly.joystick[fly.num_joystick-1]);
        }
    }
}

//update thread
void ART::update() {
    setARTPort(port);
    if (dtrack == 0) dtrack = new DTrack(port, 0, 0, 20000, 10000);
    if (!active || dtrack == 0) return;

    if (dtrack->receive()) {
        scan();
        for (auto d : devices) {
            coords::YtoZ(d.second->m); // LESC -> TODO: use the up value and others to specify the coordinate system
            d.second->update();
        }
    } else {
        if(dtrack->timeout())       cout << "--- ART: timeout while waiting for udp data" << endl;
        if(dtrack->udperror())      cout << "--- ART: error while receiving udp data" << endl;
        if(dtrack->parseerror())    cout << "--- ART: error while parsing udp data" << endl;
    }
}

VRFunction<int>* ART::getARTUpdateFkt() {
    return new VRFunction<int>("ART_update", boost::bind(&ART::update, this));
}

vector<int> ART::getARTDevices() {
    vector<int> devs;
    for (auto& itr : devices) devs.push_back(itr.first);
    return devs;
}

ART_device* ART::getARTDevice(int dev) { return devices[dev]; }

void ART::setARTActive(bool b) { active = b; }
bool ART::getARTActive() { return active; }

int ART::getARTPort() { return port; }
void ART::setARTPort(int port) {
    if (current_port == port) return;
    this->port = port;
    current_port = port;

    if (dtrack != 0) delete dtrack;
    dtrack = new DTrack(port);
    if (!dtrack->valid()) {
        cout << "DTrack init error" << endl;
        delete dtrack;
        port = -1;
        dtrack = 0;
    }
}

void ART::setARTOffset(Vec3f o) { offset = o; }
Vec3f ART::getARTOffset() { return offset; }

void ART::startTestStream() {
    // TODO: create test data
}

OSG_END_NAMESPACE
