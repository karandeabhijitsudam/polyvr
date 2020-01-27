#include "VRSyncNode.h"
#include "VRLight.h"
#include "core/objects/OSGObject.h"
#include "core/objects/material/VRMaterial.h"
#include "core/objects/material/OSGMaterial.h"
#include "core/utils/VRStorage_template.h"
#include "core/networking/VRSocket.h"
#include "core/networking/VRWebSocket.h"
#include "core/scene/VRScene.h"
#include "core/scene/VRSceneManager.h"
#include <OpenSG/OSGMultiPassMaterial.h>
#include <OpenSG/OSGSimpleMaterial.h>
#include <OpenSG/OSGSimpleGeometry.h>        // Methods to create simple geos.

#include <OpenSG/OSGNode.h>
#include <OpenSG/OSGNodeCore.h>
#include <OpenSG/OSGTransformBase.h>
#include "core/objects/OSGTransform.h"

#include <OpenSG/OSGThreadManager.h>

using namespace OSG;

template<> string typeName(const VRSyncNode& o) { return "SyncNode"; }

ThreadRefPtr applicationThread;

void VRSyncNode::printChangeList(){
    //ChangeList* cl = Thread->getCurrentChangeList();
    ChangeList* cl = applicationThread->getChangeList();
    int j = 0;
    for( auto it = cl->begin(); it != cl->end(); ++it) {
        ContainerChangeEntry* entry = *it;
        const FieldFlags* fieldFlags = entry->pFieldFlags;
        BitVector whichField = entry->whichField;

        cout << "uiEntryDesc " << j << ": " << entry->uiEntryDesc << ", uiContainerId: " << entry->uiContainerId << endl;

//        auto thisContainer = FieldContainerFactory::the()->getContainer(entry->uiContainerId);
//        auto mappedContainer = FieldContainerFactory::the()->getMappedContainer(entry->uiContainerId);
//        auto mappedContainerID = mappedContainer->getId();
//        cout << ">>>>>> thisContainer: " << thisContainer->getId() << " mappedContainer" << mappedContainerID << endl;

        for (int i=0; i<64; i++) {
            //int bit = (whichField & ( 1 << i )) >> i;
            BitVector one = 1;
            BitVector mask = ( one << i );
            bool bit = (whichField & mask);
            if (bit) cout << " whichField: " << i << " : " << bit << "  mask: " << mask << endl;
        }
        j++;
    }
}

VRSyncNode::VRSyncNode(string name) : VRTransform(name) {
    type = "SyncNode";
    applicationThread = dynamic_cast<Thread *>(ThreadManager::getAppThread());

    // TODO: get all container and their ID in this VRTransform
    NodeMTRefPtr node = getNode()->node;
    NodeCoreMTRefPtr core = node->getCore();

    container[node->getId()] = true;
    container[core->getId()] = true; //transform

    OSGTransformPtr pt = getOSGTransformPtr();
    //auto id = pt->getTypeName();
    //container[id] = true;

    cout << "VRSyncNode::VRSyncNode " << node->getTypeName() << ", " << core->getTypeName() << endl;

	updateFkt = VRUpdateCb::create("SyncNode update", bind(&VRSyncNode::update, this));
	VRScene::getCurrent()->addUpdateFkt(updateFkt, 100000);
}

VRSyncNode::~VRSyncNode() {
    cout << "VRSyncNode::~VRSyncNode " << name << endl;
}

VRSyncNodePtr VRSyncNode::ptr() { return static_pointer_cast<VRSyncNode>( shared_from_this() ); }
VRSyncNodePtr VRSyncNode::create(string name) { return VRSyncNodePtr(new VRSyncNode(name) ); }

//update this SyncNode
void VRSyncNode::update() {
    //printChangeList();

    // go through all changes, gather changes where the container is known (in containers)
    // serialize changes in new change list (check OSGConnection for serialization Implementation)

    // send over websocket to remote
    for (auto& remote : remotes) {
        //remote.second.socket.sendMessage(cl_data);
    }
}

VRObjectPtr VRSyncNode::copy(vector<VRObjectPtr> children) {
    return 0;
}

void VRSyncNode::startInterface(int port) {
    socket = VRSceneManager::get()->getSocket(port);
    socketCb = new VRHTTP_cb( "VRSyncNode callback", bind(&VRSyncNode::handleChangeList, this, _1) );
    socket->setHTTPCallback(socketCb);
    socket->setType("http receive");
    cout << "maybe started Interface at port " << port << endl;
}

string asUri(string host, int port, string name) {
    return "ws://" + host + ":" + to_string(port) + "/" + name;
}

//Add a SyncRemote
void VRSyncNode::addRemote(string host, int port, string name) {
    string uri = asUri(host, port, name);
    cout << "added SyncRemote (host, port, name) " << "(" << host << ", " << port << ", " << name << ") -> " << uri << endl;
    remotes[uri] = VRSyncRemote::create(uri);
    //remotes.insert(map<string, VRSyncRemote>::value_type(uri, VRSyncRemote(uri)));
    cout << " added SyncRemote 2" << endl;
    //remotes[uri].connect();
}

void VRSyncNode::handleChangeList(void* _args) {
    HTTP_args* args = (HTTP_args*)_args;
    if (!args->websocket) cout << "AAAARGH" << endl;

    int client = args->ws_id;
    string msg = args->ws_data;

    cout << "GOT CHANGES!! " << endl;
    cout << "client " << client << ", received msg: " << msg << " container: ";

    for (auto c : container){
        cout << c.first << " ";
    }
    cout << endl;
}

//broadcast message to all remote nodes
void VRSyncNode::broadcast(string message){
    for (auto& remote : remotes) {
        if (remote.second->send(message)) {
            cout << "sent" << endl;
        }
    }
}

void VRSyncNode::getContainer(){
    FieldContainerFactoryBase* factory = FieldContainerFactory::the();
    //auto containerStore = factory->getFieldContainerStore();

    for( auto it = factory->beginStore(); it != factory->endStore(); ++it) {
        AspectStore* aspect = it->second;
        FieldContainer* container = aspect->getPtr();

        cout << "container Id: " << factory->findContainer(container) << endl;

    }

}

// ------------------- VRSyncRemote


VRSyncRemote::VRSyncRemote(string uri) : uri(uri) {
    socket = VRWebSocket::create("sync node ws");
    cout << "VRSyncRemote::VRSyncRemote " << uri << endl;
    if (uri != "") connect();
}

VRSyncRemote::~VRSyncRemote() { cout << "~VRSyncRemote::VRSyncRemote" << endl; }
VRSyncRemotePtr VRSyncRemote::create(string name) { return VRSyncRemotePtr( new VRSyncRemote(name) ); }

void VRSyncRemote::connect() {
    cout << "VRSyncRemote, try connecting to " << uri << endl;
    bool result = socket->open(uri);
    if (!result) cout << "VRSyncRemote, Failed to open websocket to " << uri << endl;
    else cout << "VRSyncRemote, connected to " << uri << endl;
}

bool VRSyncRemote::send(string message){
    if (!socket->sendMessage(message)) return 0;
    return 1;
}
