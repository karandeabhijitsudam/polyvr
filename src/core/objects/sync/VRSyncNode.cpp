#include "VRSyncNode.h"
#include "core/networking/tcp/VRTCPServer.h"
#include "core/networking/tcp/VRTCPClient.h"

#include "core/objects/VRLight.h"
#include "core/objects/OSGObject.h"
#include "core/objects/object/OSGCore.h"
#include "core/objects/material/VRMaterial.h"
#include "core/objects/material/OSGMaterial.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/objects/VRCamera.h"
#include "core/setup/VRSetup.h"
#include "core/setup/devices/VRDevice.h"
//#include "core/math/pose.h"
#include "core/utils/VRStorage_template.h"
#include "core/gui/VRGuiConsole.h"
#include "core/scene/VRScene.h"
#include "core/scene/VRSceneManager.h"
#include "core/scene/import/VRImport.h"
#include "core/networking/tcp/VRTCPUtils.h"

#include <OpenSG/OSGMultiPassMaterial.h>
#include <OpenSG/OSGSimpleMaterial.h>
#include <OpenSG/OSGSimpleGeometry.h>        // Methods to create simple geos.

#include <OpenSG/OSGNode.h>
#include <OpenSG/OSGNodeCore.h>
#include <OpenSG/OSGTransformBase.h>
#include "core/objects/OSGTransform.h"
#include <OpenSG/OSGFieldContainerFactory.h>
#include <OpenSG/OSGContainerIdMapper.h>
#include <OpenSG/OSGNameAttachment.h>

#include <OpenSG/OSGChangeList.h>
#include <OpenSG/OSGThreadManager.h>

// needed to filter GLId field masks
#include <OpenSG/OSGSurface.h>
#include <OpenSG/OSGGeoProperty.h>
#include <OpenSG/OSGGeoMultiPropertyData.h>
#include <OpenSG/OSGRenderBuffer.h>
#include <OpenSG/OSGFrameBufferObject.h>
#include <OpenSG/OSGTextureObjRefChunk.h>
#include <OpenSG/OSGUniformBufferObjStd140Chunk.h>
#include <OpenSG/OSGShaderStorageBufferObjStdLayoutChunk.h>
#include <OpenSG/OSGTextureObjChunk.h>
#include <OpenSG/OSGUniformBufferObjChunk.h>
#include <OpenSG/OSGShaderStorageBufferObjChunk.h>
#include <OpenSG/OSGShaderExecutableChunk.h>
#include <OpenSG/OSGSimpleSHLChunk.h>
#include <OpenSG/OSGShaderProgram.h>
#include <OpenSG/OSGProgramChunk.h>

#include <bitset>

//BUGS:
/*
Known bugs:
    - syncedContainer seem not get get empty (keeps filling with same id)
    - in PolyVR: when syncNodes are not initialized on_scene_load but by manually triggering the script - the program will crash
    - syncNodes need to be translated on initialization else no Transform Node will be created to track (PolyVR optimisation initialises with Group Node type; Transform Node only creates after a Transformation)

*/

//TODO:
/*
    - create (Node/Child) change handling and applying on remote SyncNode
    - copy Changes from state for initialization of new remote SyncNode from master's State
    - remove Changes (derefferencing?)
*/

using namespace OSG;

void printGeoGLIDs(Geometry* geo) {
    if (!geo) {
        cout << " printGeoGLIDs, no geo!" << endl;
        return;
    }

    cout << " geometry GL IDs: " << Vec4i(
            0,//geo->getAttribVaoGLId(),
            geo->getClassicGLId(),
            geo->getAttGLId(),
            0//geo->getClassicVaoGLId()
        ) << endl;
}

ThreadRefPtr applicationThread;

VRSyncNode::VRSyncNode(string name) : VRTransform(name) {
    type = "SyncNode";
    selfID = getNode()->node->getId();
    changelist = VRSyncChangelist::create();
    applicationThread = dynamic_cast<Thread *>(ThreadManager::getAppThread());
	updateFkt = VRUpdateCb::create("SyncNode update", bind(&VRSyncNode::update, this));
	VRScene::getCurrent()->addUpdateFkt(updateFkt, 100000);
}

VRSyncNode::~VRSyncNode() {
    cout << " VRSyncNode::~VRSyncNode " << name << endl;
}

VRSyncNodePtr VRSyncNode::ptr() { return static_pointer_cast<VRSyncNode>( shared_from_this() ); }
VRSyncNodePtr VRSyncNode::create(string name) { return VRSyncNodePtr(new VRSyncNode(name) ); }

VRSyncConnectionPtr VRSyncNode::getRemote(string rID) {
    if (!remotes.count(rID)) return 0;
    return remotes[rID];
}

string VRSyncNode::setTCPClient(VRTCPClientPtr client) {
    remotes.clear();
    return addTCPClient(client);
}

string VRSyncNode::addTCPClient(VRTCPClientPtr client) {
    string uri = client->getConnectedUri();
    cout << " -------- addTCPClient to " << uri << endl;
    auto remote = VRSyncConnection::create(client, uri);
    remotes[uri] = remote;
    client->onMessage( bind(&VRSyncNode::handleMessage, this, std::placeholders::_1, uri) );
    VRConsoleWidget::get("Collaboration")->write( name+": add tcp client connected to "+uri+", state is "+toString(client->connected())+"\n");
    remote->send("accConnect|1");
    return uri;
}

void VRSyncNode::accTCPConnection(string msg, string rID) {
    auto remote = getRemote(rID);
    if (!remote) return;
    auto nID = getNode()->node->getId();
    VRConsoleWidget::get("Collaboration")->write( name+": got tcp client acc, "+msg+"\n");
    if (msg == "accConnect|1") remote->send("accConnect|2");
    remote->send("selfmap|"+toString(nID));
    sendTypes(rID);
    remote->send("reqInitState|");
}

void VRSyncNode::reqInitState() {
    VRConsoleWidget::get("Collaboration")->write( name+": got request to send initial state\n");
    if (getChildrenCount() == 0) return;
    changelist->broadcastSceneState(ptr());
}

//Add remote Nodes to sync with
void VRSyncNode::addRemote(string host, int port) {
    cout << " >>> > > VRSyncNode::addRemote to " << getName() << ": " << name << " at " << host << " on " << port << endl;
    string uri = host + ":" + toString(port);
    if (remotes.count(uri)) return;
    auto remote = VRSyncConnection::create(host, port, serverUri);
    remotes[uri] = remote;

    // sync node ID
    auto nID = getNode()->node->getId();
    remote->send("selfmap|"+toString(nID));
    remote->send("newConnect|"+serverUri);
    cout << "   send newConnect from " << uri << endl;

    sendTypes(uri);
}

void VRSyncNode::setDoWrapping(bool b) { doWrapping = b; }

bool VRSyncNode::isSubContainer(const UInt32& id) {
    auto fct = factory->getContainer(id);
    if (!fct) return false;

    UInt32 syncNodeID = getNode()->node->getId();
    auto type = factory->findType(fct->getTypeId());

    function<bool(Node*)> checkAncestor = [&](Node* node) {
        if (!node) return false;
        if (node->getId() == syncNodeID) return true;
        Node* parent = node->getParent();
        return checkAncestor(parent);
    };

    if (type->isNode()) {
        Node* node = dynamic_cast<Node*>(fct);
        if (!node) return false;
        return checkAncestor(node);
    }

    if (type->isNodeCore()) {
        NodeCore* core = dynamic_cast<NodeCore*>(fct);
        if (!core) return false;
        for (auto node : core->getParents())
            if (checkAncestor(dynamic_cast<Node*>(node))) return true;
        return false;
    }

    string typeName = fct->getTypeName();

    Attachment* att = dynamic_cast<Attachment*>(fct);
    if (att) {
        auto parents = att->getMFParents();
        for (UInt32 i = 0; i<parents->size(); i++) {
            FieldContainer* parent = parents->at(i);
            if (parent && isSubContainer(parent->getId())) return true;
        }
        if (parents->size() == 0) {
            if (typeName == "MultiPassMaterial") return false; // Materials may not be attached to a geometry, thats fine!
            cout << " -- WARNING -- attachment FC has no parents: " << id << " type: " << fct->getTypeName();
            if (AttachmentContainer* attc = dynamic_cast<AttachmentContainer*>(fct)) {
                if (auto n = ::getName(attc)) cout << " named: " << n;
                else cout << " unnamed";
            }
            cout << endl;
        }
        return false;
    }

    if (typeName == "Viewport") return false; // TODO, use ID checks instead of string comparisions
    if (typeName == "PassiveWindow") return false;

    if (typeName == "ShaderProgram") {
        if (VRMaterial::fieldContainerMap.count(id)) {
            auto scID = VRMaterial::fieldContainerMap[id];
            return isSubContainer(scID);
        }
        cout << "  -- WARNING -- untracked ShaderProgram " << id << endl;
        return false;
    }

    if (typeName == "ShaderVariableOSG" || typeName == "ShaderVariableInt" || typeName == "ShaderVariableReal"
        || typeName == "ShaderVariableVec4f" || typeName == "ShaderVariableVec3f" || typeName == "ShaderVariableVec2f") {
        if (VRMaterial::fieldContainerMap.count(id)) {
            auto scID = VRMaterial::fieldContainerMap[id];
            return isSubContainer(scID);
        }
        cout << "  -- WARNING -- untracked ShaderVariable " << id << endl;
        return false;
    }

    if (typeName == "Image") { // TODO, implement propper check
        if (VRMaterial::fieldContainerMap.count(id)) {
            auto toID = VRMaterial::fieldContainerMap[id];
            return isSubContainer(toID);
        }
        cout << "  -- WARNING -- untracked image " << id << endl;
        return false;
    }

    if (typeName == "SolidBackground"
        || typeName == "State" // TODO: what is a State?? there are many of those!
        || typeName == "ShaderShadowMapEngine"
        || typeName == "TrapezoidalShadowMapEngine"
        || typeName == "SimpleShadowMapEngine"
        || typeName == "TextureBackground"
        || typeName == "SkyBackground"
        || typeName == "PerspectiveCamera"
        || typeName == "TextureBuffer"
        || typeName == "RenderBuffer"
        || typeName == "FrameBufferObject") { // TODO, implement propper check
        //cout << " -- WARNING -- unhandled FC type in isSubContainer: " << id << " " << typeName << endl;
        return false;
    }


    cout << " -- WARNING -- unhandled FC type in isSubContainer: " << id << " " << typeName << endl;

    return false;
}

// checks if a container was changed by remote
bool VRSyncNode::isRemoteChange(const UInt32& id) {
    return bool(::find(syncedContainer.begin(), syncedContainer.end(), id) != syncedContainer.end());
}

void VRSyncNode::addRemoteMapping(UInt32 lID, UInt32 rID) {
    remoteToLocalID[rID] = lID;
    localToRemoteID[lID] = rID;

    if (false) { // for debugging
        cout << " addRemoteMapping in " << getName() << ", map local " << lID << " to remote " << rID;
        if (auto afc = dynamic_cast<AttachmentContainer*>(factory->getContainer(lID))) {
            if (auto n = ::getName(afc)) cout << ", local named " << n;
        }
        if (auto afc = dynamic_cast<AttachmentContainer*>(factory->getContainer(rID))) {
            if (auto n = ::getName(afc)) cout << ", remote named " << n;
        }
        cout << endl;
    }
}

void VRSyncNode::replaceContainerMapping(UInt32 ID1, UInt32 ID2) {
    for (auto c : container) {
        if (c.second == ID1) addRemoteMapping(c.first, ID2);
    }
}

UInt32 VRSyncNode::getRegisteredContainerID(UInt32 syncID) {
    for (auto registered : container) {
        if (registered.second == syncID) return registered.first;
    }
    return -1;
}

void printNodeFieldMask(BitVector fieldmask) {
    string changeType = "";
    if (fieldmask & Node::AttachmentsFieldMask) changeType = " AttachmentsFieldMask";
//    else if (fieldmask & Node::bInvLocalFieldMask) changeType = " bInvLocalFieldMask ";
//    else if ( fieldmask & Node::bLocalFieldMask) changeType = " bLocalFieldMask      ";
    else if ( fieldmask & Node::ChangedCallbacksFieldMask) changeType = " ChangedCallbacksFieldMask";
    else if ( fieldmask & Node::ChildrenFieldMask) changeType = " ChildrenFieldMask   ";
    else if ( fieldmask & Node::ContainerIdMask) changeType = " ContainerIdMask ";
    else if ( fieldmask & Node::DeadContainerMask) changeType = " DeadContainerMask ";
    else if ( fieldmask & Node::NextFieldMask) changeType = " NextFieldMask ";
    else if ( fieldmask & Node::ParentFieldMask) changeType = " ParentFieldMask ";
    else if ( fieldmask & Node::SpinLockClearMask) changeType = " SpinLockClearMask ";
    else if ( fieldmask & Node::TravMaskFieldMask) changeType = " TravMaskFieldMask ";
    else if ( fieldmask & Node::VolumeFieldMask) changeType = " VolumeFieldMask ";
    else if ( fieldmask & Node::CoreFieldMask) changeType = " CoreFieldMask ";
    else changeType = " none ";

    cout << changeType << endl;
}

void printContainer (FieldContainerFactoryBase* factory, map<UInt32,UInt32> container) {
    for (auto c : container) {
        UInt32 id = c.first;
        FieldContainer* fcPt = factory->getContainer(id);
        if (!fcPt) continue;
        FieldContainerType* fcType = factory->findType(fcPt->getTypeId());
        if (fcType) {
            if (fcType->isNode()) {
                Node* node = dynamic_cast<Node*>(fcPt);
                cout << id << " " << fcPt->getTypeName() << " children " << node->getNChildren() << endl;
                if (node->getNChildren() == 0) cout << "no children" << endl;
                for (UInt32 i = 0; i < node->getNChildren(); i++) {
                    Node* childNode = node->getChild(i);
                    cout << "   " << childNode->getId() << endl;
                }
            }
        }
    }
}


void VRSyncNode::gatherLeafs(VRObjectPtr parent, vector<pair<Node*, VRObjectPtr>>& leafs, vector<VRObjectPtr>& inconsistentCores) {
    if (!parent) return;
    if (!parent->getNode()) return;
    if (!parent->getCore()) return;

    vector<Node*> vrChildren;
    for (auto child : parent->getChildren()) vrChildren.push_back( child->getNode()->node );

    vector<Node*> vrChildrenTMP;
    Node* pNode = parent->getNode()->node;

    // check if a transform core changed
    NodeCore* c1 = parent->getCore()->core;
    NodeCore* c2 = pNode->getCore();
    if (c1 != c2 && c2) {
        string type = c2->getTypeName();
        if (type != "Group") inconsistentCores.push_back(parent);
    }

    // ignore geometry nodes, they are part of vrgeometry
    if (pNode->getNChildren() == 1) {
        if (dynamic_cast<Geometry*>(pNode->getChild(0)->getCore())) { // check if geo core
            pNode = pNode->getChild(0);
        }
    }

    // check if transform core and obj are mapped
    if (c2 && !nodeToVRObject.count(c2->getId())) nodeToVRObject[c2->getId()] = parent;

    for (UInt32 i=0; i<pNode->getNChildren(); i++) {
        Node* child = pNode->getChild(i);
        vrChildrenTMP.push_back(child);
        if (i < vrChildren.size() && vrChildren[i] == child) continue; //try to check with index
        if (::find(vrChildren.begin(), vrChildren.end(), child) != vrChildren.end()) continue;
        leafs.push_back( make_pair(child, parent) );
    }

    for (auto child : parent->getChildren()) gatherLeafs(child, leafs, inconsistentCores);
}

map<UInt32, VRObjectWeakPtr> VRSyncNode::getMappedFCs() { return nodeToVRObject; }

VRObjectPtr VRSyncNode::OSGConstruct(NodeMTRecPtr n, VRObjectPtr parent, Node* geoParent) {
    if (!doWrapping) return 0;
    if (n == 0) return 0; // TODO add an osg wrap method for each object?

    VRObjectPtr tmp = 0;
    VRMaterialPtr tmp_m;
    VRGeometryPtr tmp_g;
    VRTransformPtr tmp_e;
    VRGroupPtr tmp_gr;

    if (!n) { cout << "WARNING! VRSyncNode::OSGConstruct: node invalid!" << endl; return 0; }
    if (!n->getCore()) { cout << "WARNING! VRSyncNode::OSGConstruct: node core invalid!" << endl; return 0; }

    NodeCoreMTRecPtr core = n->getCore();
    string t_name = core->getTypeName();
    string name = ::getName(n);

    if (t_name == "Group" || t_name == "Transform") {
        if (n->getChild(0)) {
            if (n->getChild(0)->getCore()) {
                string tp = n->getChild(0)->getCore()->getTypeName();
                if (tp == "Geometry") {
                    tmp_g = VRGeometry::create(name);
                    tmp_g->wrapOSG(OSGObject::create(n), OSGObject::create(n->getChild(0)));
                    tmp = tmp_g;
                    nodeToVRObject[n->getCore()->getId()] = tmp;
                }
            }
        }
    }

    if (tmp == 0 && t_name == "Group") {
        tmp = VRObject::create(name);
        tmp->wrapOSG(OSGObject::create(n));
        //nodeToVRObject[n->getCore()->getId()] = tmp;
    }

    if (tmp == 0 && t_name == "Transform") {
        tmp_e = VRTransform::create(name);
        tmp_e->wrapOSG(OSGObject::create(n));
        tmp = tmp_e;
        nodeToVRObject[n->getCore()->getId()] = tmp;
    }

    if (t_name == "Geometry") return 0; // ignore geo leafs

    if (tmp == 0) { // fallback
        tmp = VRObject::create(name);
        tmp->wrapOSG(OSGObject::create(n));
        //nodeToVRObject[n->getId()] = tmp;
    }

    for (uint i=0;i<n->getNChildren();i++) { // recursion
        VRObjectPtr obj;
        if (tmp) {
            auto child = n->getChild(i);
            if (child == n) {
                VRConsoleWidget::get("Collaboration")->write( name+": Error in OSGConstruct, found invalid child equaling its parent!\n", "red");
                continue;
            }
            obj = OSGConstruct(child, tmp, geoParent);
            if (obj) tmp->addChild(obj, false);
        }
    }

    return tmp;
}

void VRSyncNode::wrapOSGLeaf(Node* node, VRObjectPtr parent) {
    auto res = OSGConstruct(node, parent);
    if (res) parent->addChild(res, false);
}

void VRSyncNode::wrapOSG() { // TODO: check for deleted nodes!
    // traverse sub tree and get unwrapped OSG nodes
    vector<pair<Node*, VRObjectPtr>> leafs; // pair of nodes and VRObjects they are linked to
    vector<VRObjectPtr> inconsistentCores;
    gatherLeafs(ptr(), leafs, inconsistentCores);

    // TODO: wrapping the nodes breaks DnD sync
    for (auto p : leafs) wrapOSGLeaf(p.first, p.second);
    for (auto i : inconsistentCores) i->wrapOSG(i->getNode());
}

UInt32 VRSyncNode::findParent(map<UInt32,vector<UInt32>>& parentToChildren, UInt32 remoteNodeID) {
    UInt32 parentId = container.begin()->first;
    for (auto remoteParent : parentToChildren) {
        UInt32 remoteParentId = remoteParent.first;
        vector<UInt32> remoteChildren = remoteParent.second;
        for (UInt32 i = 0; i<remoteChildren.size(); i++) {
            UInt32 remoteChildId = remoteChildren[i];
            if (remoteChildId == remoteNodeID) {
                if (remoteToLocalID[remoteParentId]) return remoteToLocalID[remoteParentId];
            }
        }
    }
    return parentId;
}

void VRSyncNode::printRegistredContainers() {
    cout << endl << "registered container: " << name << endl;
    for (auto c : container) {
        UInt32 id = c.first;
        FieldContainer* fc = factory->getContainer(id);
        cout << " " << id << ", syncNodeID " << c.second;

        if (localToRemoteID.count(id))  cout << ", remoteID " << localToRemoteID[id];
        else                            cout << ",              ";

        if (fc) {
            cout << ", type: " << fc->getTypeName() << ", Refs: " << fc->getRefCount();
            if (Node* node = dynamic_cast<Node*>(fc)) cout << ", N children: " << node->getNChildren();
        }
        cout << endl;
    }
}

void VRSyncNode::printSyncedContainers() {
    cout << endl << "synced container:" << endl;
    for (UInt32 id : syncedContainer) cout << id << endl;
}

bool VRSyncNode::isRegistered(const UInt32& id) {
    return bool(container.count(id));
}

bool VRSyncNode::isExternalContainer(const UInt32& id, UInt32& mask) {
    if (externalContainer.count(id)) {
        mask = externalContainer[id];
        return true;
    }
    return false;
}

//checks in container if the node with syncID is already been registered
bool VRSyncNode::isRegisteredRemote(const UInt32& syncID) {
    for (auto reg : container) { //check if the FC is already registered, f.e. if nodeCore create entry arrives first a core along with its node will be created before the node create entry arrives
        if (reg.second == syncID) return true;
    }
    return false;
}

void VRSyncNode::getAllSubContainersRec(FieldContainer* node, FieldContainer* parent, map<FieldContainer*, vector<FieldContainer*>>& res) {
    if (!node) return;
    if (!isRegistered(node->getId())) {
        res[node].push_back(parent);
    }

    auto ptype = factory->findType(node->getTypeId());

    if (ptype->isNode()) {
        Node* pnode = dynamic_cast<Node*>(node);

        NodeCore* core = pnode->getCore();
        getAllSubContainersRec(core, node, res);

        auto attachments = pnode->getSFAttachments()->getValue();
        for (auto a : attachments) {
            Attachment* attachment = a.second;
            getAllSubContainersRec(attachment, node, res);
        }

        for (UInt32 i=0; i<pnode->getNChildren(); i++) {
            Node* child = pnode->getChild(i);
            getAllSubContainersRec(child, node, res);
        }
    }

    if (ptype->isNodeCore()) {
        NodeCore* pcore = dynamic_cast<NodeCore*>(node);
        auto attachments = pcore->getSFAttachments()->getValue();
        for (auto a : attachments) {
            Attachment* attachment = a.second;
            getAllSubContainersRec(attachment, node, res);
        }
    }

    if (ptype->isAttachment()) {
        Attachment* pattachment = dynamic_cast<Attachment*>(node);
        auto attachments = pattachment->getSFAttachments()->getValue();
        for (auto a : attachments) {
            Attachment* attachment = a.second;
            getAllSubContainersRec(attachment, node, res);
        }
    }
}

map<FieldContainer*, vector<FieldContainer*>> VRSyncNode::getAllSubContainers(FieldContainer* node) {
    map<FieldContainer*, vector<FieldContainer*>> res;
    getAllSubContainersRec(node, 0, res);
    return res;
}

//update this SyncNode
void VRSyncNode::update() {
    for (auto& remote : remotes) remote.second->keepAlive();
    auto localChanges = changelist->filterChanges(ptr());
    if (!localChanges) return;
    //if (getChildrenCount() == 0) return; // TODO: this may happen if the only child is dragged, or the only child was just deleted..
    cout << endl << " > > >  " << name << " VRSyncNode::update()" << endl;


    //OSGChangeList* cl = (OSGChangeList*)applicationThread->getChangeList();
    //changelist->printChangeList(ptr(), cl);


    //printRegistredContainers(); // DEBUG: print registered container
    //printSyncedContainers();
    //changelist->printChangeList(ptr(), localChanges);

    //VRConsoleWidget::get("Collaboration")->write( " Broadcast scene updates\n");
    changelist->broadcastChangeList(ptr(), localChanges, true);
    syncedContainer.clear();
    cout << "            / " << name << " VRSyncNode::update()" << "  < < < " << endl;
}

void VRSyncNode::logSyncedContainer(UInt32 id) {
    if (isRemoteChange(id)) syncedContainer.push_back(id); // TODO: irgendwie komisch.. wird syncedContainer ueberhaupt verwendet?
}

void VRSyncNode::registerContainer(FieldContainer* c, UInt32 syncNodeID) {
    UInt32 ID = c->getId();
    if (container.count(ID)) return;
    //cout << " VRSyncNode::registerContainer " << getName() << " container: " << c->getTypeName() << " at fieldContainerId: " << ID << endl;
    container[ID] = syncNodeID;
}

UInt32 VRSyncNode::getNodeID(VRObjectPtr t) {
    return t->getNode()->node->getId();
}

UInt32 VRSyncNode::getTransformID(VRTransformPtr t) {
    return t->getOSGTransformPtr()->trans->getId();
}

void VRSyncNode::setAvatarBeacons(VRTransformPtr headTransform, VRTransformPtr devTransform, VRTransformPtr devAnchor) { // camera and mouse or flystick
    avatarHeadTransform = headTransform;
    avatarDeviceTransform = devTransform;
    avatarDeviceAnchor = devAnchor;
    //VRConsoleWidget::get("Collaboration")->write( name+": Add avatar input, head "+head->getName()+" ("+toString(getTransformID(head))+"), hand "+device->getName()+" ("+toString(getTransformID(device))+")\n", "green");
}

void VRSyncNode::addRemoteAvatar(string remoteID, VRTransformPtr headTransform, VRTransformPtr devTransform, VRTransformPtr devAnchor) { // some geometries
    auto remote = getRemote(remoteID);
    if (!remote) return;

    headTransform->enableOptimization(false);
    devTransform->enableOptimization(false);
    devAnchor->enableOptimization(false);

    UInt32 headTransID = getTransformID(headTransform);
    UInt32 deviceTransID = getTransformID(devTransform);
    UInt32 deviceAnchorID = getNodeID(devAnchor);
    string msg = "addAvatar|"+toString(headTransID)+":"+toString(deviceTransID)+":"+toString(deviceAnchorID);
    remote->send(msg);
    VRConsoleWidget::get("Collaboration")->write( name+": Add avatar representation, head "+headTransform->getName()+" ("+toString(headTransID)+"), hand "+devTransform->getName()+" ("+toString(deviceTransID)+")\n", "green");
}

void VRSyncNode::addExternalContainer(UInt32 id, UInt32 mask) {
    externalContainer[id] = mask;
    VRConsoleWidget::get("Collaboration")->write( name+": Add external container "+toString(id)+" with mask "+toString(mask)+"\n", "green");
}

void VRSyncNode::handleAvatar(string data, string remoteID) {
    auto remote = getRemote(remoteID);
    if (!remote) return;

    if (avatarHeadTransform && avatarDeviceTransform && avatarDeviceAnchor) {
        auto IDs = splitString( splitString(data, '|')[1], ':');
        UInt32 headTransID = toInt(IDs[0]); // remote
        UInt32 deviceTransID = toInt(IDs[1]); // remote
        UInt32 deviceAnchorID = toInt(IDs[2]); // remote

        UInt32 camTrans = getTransformID( avatarHeadTransform ); // local
        UInt32 devTrans = getTransformID( avatarDeviceTransform ); // local
        UInt32 devAnchor = getNodeID( avatarDeviceAnchor ); // local

        string mapping = "mapping";
        mapping += "|"+toString(headTransID)+":"+toString(camTrans);
        mapping += "|"+toString(deviceTransID)+":"+toString(devTrans);
        mapping += "|"+toString(deviceAnchorID)+":"+toString(devAnchor);
        remote->send(mapping);
        VRConsoleWidget::get("Collaboration")->write( name+": Map remote avatar head ID "+toString(headTransID)+" to local camera ID "+toString(camTrans)+"\n", "green");
        VRConsoleWidget::get("Collaboration")->write( name+": Map remote avatar hand ID "+toString(deviceTransID)+" to local device ID "+toString(devTrans)+"\n", "green");
        VRConsoleWidget::get("Collaboration")->write( name+": Map remote avatar anchor ID "+toString(deviceAnchorID)+" to local anchor ID "+toString(devAnchor)+"\n", "green");

        UInt32 childMask = 0;
        childMask |= Node::ChildrenFieldMask;
        childMask |= Node::VolumeFieldMask;

        addExternalContainer(camTrans, -1);
        addExternalContainer(devTrans, -1);
        addExternalContainer(devAnchor, childMask);
    }
}

size_t VRSyncNode::getContainerCount() { return container.size(); }

//returns registered IDs
vector<UInt32> VRSyncNode::registerNode(Node* node) { // deprecated?
    vector<UInt32> res;
    vector<UInt32> localRes;
    vector<UInt32> recursiveRes;
    NodeCoreMTRefPtr core = node->getCore();
    cout << "register node " << node->getId() << endl;

    registerContainer(node, container.size());
    if (!core) cout << "no core" << core << endl;
    cout << "register core " << core->getId() << endl;

    registerContainer(core, container.size());
    localRes.push_back(node->getId());
    localRes.push_back(core->getId());
    for (UInt32 i=0; i<node->getNChildren(); i++) {
        cout << "register child " << node->getChild(i)->getId() << endl;
        recursiveRes = registerNode(node->getChild(i));
    }
    res.reserve(localRes.size() + recursiveRes.size());
    res.insert(res.end(), localRes.begin(), localRes.end());
    res.insert(res.end(), recursiveRes.begin(), recursiveRes.end());
    return res;
}

VRObjectPtr VRSyncNode::copy(vector<VRObjectPtr> children) {
    return 0;
}

UInt32 VRSyncNode::getLocalType(UInt32 id) {
    return typeMapping[id];
}

void VRSyncNode::handleMapping(string mappingData) {
    auto pairs = splitString(mappingData, '|');
    for (auto p : pairs) {
        auto IDs = splitString(p, ':');
        if (IDs.size() != 2) continue;
        UInt32 lID = toInt(IDs[0]);
        UInt32 rID = toInt(IDs[1]);
        addRemoteMapping(lID, rID);
    }
    //printRegistredContainers();
}

void VRSyncNode::sendTypes(string remoteID) {
    auto remote = getRemote(remoteID);
    if (!remote) return;
    auto dtIt = factory->begin(FieldContainer::getClassType());
    auto dtEnd = factory->end();

    string msg = "typeMapping";
    while (dtIt != dtEnd) {
        FieldContainerType* fcT = *dtIt;
        msg += "|" + toString(fcT->getId()) + ":" + fcT->getName();
        ++dtIt;
    }
    remote->send(msg);
}

void VRSyncNode::handleTypeMapping(string mappingData) {
    auto pairs = splitString(mappingData, '|');
    for (auto p : pairs) {
        auto IDs = splitString(p, ':');
        if (IDs.size() != 2) continue;
        UInt32 rID = toInt(IDs[0]);
        string name = IDs[1];
        FieldContainerType* fcT = factory->findType(name.c_str());
        if (!fcT) {
            cout << "Warning in VRSyncNode::handleTypeMapping, unknown remote type " << name.c_str() << endl;
            continue;
        }
        typeMapping[rID] = fcT->getId();
        //cout << " typeMapping: " << rID << " " << name << " " << fcT->getId() << endl;
    }
    //printRegistredContainers();
}

void VRSyncNode::handleOwnershipMessage(string ownership, string rID)  { // TODO: properly test and use rID
    cout << "VRSyncNode::handleOwnership" << endl;
    vector<string> str_vec = splitString(ownership, '|');
    string remoteID = str_vec[1];
    string command = str_vec[2];
    string objectName = str_vec[3];
    if (command == "request") {
        cout << "handle request" << endl;
        for (string s : owned) {

            cout << "check owned" << endl;
            if (s == objectName) {
                cout << "found owned, try to get object..." << endl;
                auto object = VRScene::getCurrent()->getRoot()->find(objectName);
                if (!object) {cout<< "object " << objectName << " not fould!" << endl; return;}
                for (auto dev : VRSetup::getCurrent()->getDevices()) { //if object is grabbed return
                    VRTransformPtr obj = dev.second->getDraggedObject();
                    VRTransformPtr gobj = dev.second->getDraggedGhost();
                    if (obj == 0 || gobj == 0) {cout << "continue" << endl; continue;}
                    if (obj->getName() == objectName || gobj->getName() == objectName) {
                        cout << "object is not available. ownership not granted." << endl;
                        return;
                    }
                }
                for (auto remote : remotes) { //grant ownership
                    cout << "remote.first " << remote.first << endl;
                    if (remote.first.find(remoteID) != string::npos) {
                        string message = "ownership|"+remote.second->getLocalUri()+"|grant|" + objectName + "|" + remoteID;
                        remote.second->send(message);
                        auto it = std::find(owned.begin(), owned.end(), objectName);
                        if (it != owned.end()) owned.erase(it);
                        cout << "grant ownership " << message << endl;
                    }
                }
            }
        }
    }
    else if (command == "grant") {
        string grantedTo = str_vec[4];
        if (grantedTo == serverUri) owned.push_back(objectName);
        cout << "got ownership of object " << objectName << endl;
    }
}

vector<string> VRSyncNode::getOwnedObjects(string nodeName) {
    return owned;
}

void VRSyncNode::requestOwnership(string objectName) {
    for (auto remote : remotes) {
        string message = "ownership|"+remote.second->getLocalUri()+"|request|" + objectName;
        remote.second->send(message);
    }
}

void VRSyncNode::addOwnedObject(string objectName){
    owned.push_back(objectName);
}

vector<string> VRSyncNode::getRemotes() {
    vector<string> res;
    for (auto r : remotes) res.push_back(r.first);
    return res;
}

void VRSyncNode::handleNewConnect(string data){
    VRConsoleWidget::get("Collaboration")->write( name+": got new connection: "+data+"\n");
    cout << "VRSyncNode::handleNewConnect '" << data << "'" << endl;
    auto remoteData = splitString(data, '|');
    if (remoteData.size() < 2) {
        VRConsoleWidget::get("Collaboration")->write( name+":  Error, new connection, data malformed!\n", "red");
        cout << "Warning in VRSyncNode::handleNewConnect!, data malformed" << endl;
        return;
    }

    string remoteName = remoteData[1];
    auto uri = splitString(remoteName, ':');
    if (uri.size() < 2) {
        VRConsoleWidget::get("Collaboration")->write( name+":  Error, new connection, remote name malformed!\n", "red");
        cout << "Warning in VRSyncNode::handleNewConnect!, remote name malformed" << endl;
        return;
    }

    string ip = uri[0];
    string port = uri[1];

    cout << " handleNewConnect with ip " << remoteName << endl;
    if (onEvent) (*onEvent)("connection|"+remoteName); // if not in list then it is a new connection, the add remote
    //else cout << "Warning in VRSyncNode::handleNewConnect!, no onEvent cb!" << endl

    if (!remotes.count(remoteName)) {
        cout << "  new connection -> add remote" << remoteName << endl;
        VRConsoleWidget::get("Collaboration")->write( name+":  new connection, add remote "+remoteName+"\n");
        VRSyncNode::addRemote(ip, toInt(port));
    }
}

void VRSyncNode::startInterface(int port) {
    server = VRTCPServer::create();
    serverUri = VRTCPUtils::getPublicIP() + ":" + toString(port);
    server->listen(port, "TCPPVR\n");
    server->onMessage( bind(&VRSyncNode::handleMessage, this, std::placeholders::_1, serverUri) );
}

void VRSyncNode::handleWarning(string msg) {
    auto data = splitString(msg, '|');
    if (data.size() != 3) { cout << "AAargh" << endl; return; }
    VRConsoleWidget::get("Collaboration")->write( name+": Warning received '"+data[1]+"' about FC "+data[2], "red");
    if (factory) {
        int ID = toInt(data[2]);
        if (auto fct = factory->getContainer(ID)) {
            string name = "";
            if (AttachmentContainer* attc = dynamic_cast<AttachmentContainer*>(fct)) {
                if (auto n = ::getName(attc)) name = n;
            }
            VRConsoleWidget::get("Collaboration")->write( " "+name+" of type: "+fct->getTypeName()+" ("+toString(fct->getTypeId())+")\n", "red");
        }
    }
}

void VRSyncNode::handleSelfmapRequest(string msg) {
    auto data = splitString(msg, '|');
    if (data.size() < 2) {
        VRConsoleWidget::get("Collaboration")->write( name+":  Error, received remote sync node ID, data malformed!\n", "red");
        return;
    }
    int rID = toInt(data[1]);
    VRConsoleWidget::get("Collaboration")->write( name+": map own ID "+toString(selfID)+", to remote ID "+toString(rID)+"\n");
    addRemoteMapping(selfID, rID);
}

string VRSyncNode::handleMessage(string msg, string rID) {
    VRUpdateCbPtr job = 0;
    if (startsWith(msg, "message|"));
    else if (startsWith(msg, "keepAlive"));
    else if (startsWith(msg, "addAvatar|")) job = VRUpdateCb::create( "sync-handleAvatar", bind(&VRSyncNode::handleAvatar, this, msg, rID) );
    else if (startsWith(msg, "selfmap|")) handleSelfmapRequest(msg);
    else if (startsWith(msg, "mapping|")) job = VRUpdateCb::create( "sync-handleMap", bind(&VRSyncNode::handleMapping, this, msg) );
    else if (startsWith(msg, "typeMapping|")) job = VRUpdateCb::create("sync-handleTMap", bind(&VRSyncNode::handleTypeMapping, this, msg));
    else if (startsWith(msg, "ownership|")) job = VRUpdateCb::create( "sync-ownership", bind(&VRSyncNode::handleOwnershipMessage, this, msg, rID) );
    else if (startsWith(msg, "newConnect|")) job = VRUpdateCb::create( "sync-newConnect", bind(&VRSyncNode::handleNewConnect, this, msg) );
    else if (startsWith(msg, "accConnect|")) job = VRUpdateCb::create( "sync-accConnect", bind(&VRSyncNode::accTCPConnection, this, msg, rID) );
    else if (startsWith(msg, "reqInitState|")) job = VRUpdateCb::create( "sync-reqInitState", bind(&VRSyncNode::reqInitState, this) );
    else if (startsWith(msg, "changelistEnd|")) job = VRUpdateCb::create( "sync-finalizeCL", bind(&VRSyncChangelist::deserializeAndApply, changelist.get(), ptr()) );
    else if (startsWith(msg, "warn|")) job = VRUpdateCb::create( "sync-handleWarning", bind(&VRSyncNode::handleWarning, this, msg) );
    //else if (startsWith(msg, "warn|")) handleWarning(msg);
    else job = VRUpdateCb::create( "sync-handleCL", bind(&VRSyncChangelist::gatherChangelistData, changelist.get(), ptr(), msg) );
    if (job) VRScene::getCurrent()->queueJob( job );
    return "";
}

void VRSyncNode::broadcast(string message) { // broadcast message to all remote nodes
    //VRConsoleWidget::get("Collaboration")->write( " Broadcast: "+message+"\n");
    for (auto& remote : remotes) {
        if (!remote.second->send(message)) {
            cout << "Failed to send message to remote." << endl;
        }
    }
}

UInt32 VRSyncNode::getRemoteToLocalID(UInt32 id) {
    if (!remoteToLocalID.count(id)) return 0;
    return remoteToLocalID[id];
}

UInt32 VRSyncNode::getLocalToRemoteID(UInt32 id) {
    if (!localToRemoteID.count(id)) return 0;
    return localToRemoteID[id];
}

UInt32 VRSyncNode::getContainerMappedID(UInt32 id) {
    if (!container.count(id)) return -1;
    return container[id];
}

VRObjectPtr VRSyncNode::getVRObject(UInt32 id) {
    if (!nodeToVRObject.count(id)) return 0;
    return nodeToVRObject[id].lock();
}

void printNode(VRObjectPtr obj, string indent = "") {
    cout << indent << "obj " << obj->getName() << " (" << obj->getType() << ")";
    cout << ", nodeID: " << obj->getNode()->node->getId();
    cout << ", nde coreID: " << obj->getNode()->node->getCore()->getId() << ", nde coreType: " << obj->getNode()->node->getCore()->getTypeName();
    cout << ", obj coreID: " << obj->getCore()->core->getId() << ", obj coreType: " << obj->getCore()->core->getTypeName();
    if (auto geo = dynamic_pointer_cast<VRGeometry>(obj)) cout << ", geoNodeID: " << geo->getNode()->node->getChild(0);
    cout << endl;

    for (auto c : obj->getChildren()) printNode(c, indent+" ");
}

void VRSyncNode::analyseSubGraph() {
    cout << "VRSyncRemote::analyseSubGraph" << endl;
    printNode(ptr());
    printRegistredContainers();
}

string VRSyncNode::getConnectionLink() { return serverUri; }

void VRSyncNode::setCallback(VRMessageCbPtr fkt) {
    onEvent = fkt;
    cout << "VRSyncNode::setCallback" << endl;
}

string VRSyncNode::getConnectionStatus() {
    string status = "SyncNode: " + serverUri + "\n";
    for (auto& remote : remotes) {
        status += " (" + remote.first + ") " + remote.second->getStatus();
        status += "\n";
    }
    return status;
}

