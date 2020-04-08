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

template<> string typeName(const VRSyncNode& o) { return "SyncNode"; }

ThreadRefPtr applicationThread;

class OSGChangeList : public ChangeList {
    public:
        ~OSGChangeList() {};

        void addCreate(ContainerChangeEntry* entry) {
            ContainerChangeEntry* pEntry = getNewCreatedEntry();
            pEntry->uiEntryDesc   = entry->uiEntryDesc;
            pEntry->uiContainerId = entry->uiContainerId;
            pEntry->whichField    = entry->whichField;
            if (pEntry->whichField == 0 && entry->bvUncommittedChanges != 0)
                pEntry->whichField |= *entry->bvUncommittedChanges;
            pEntry->pList         = this;
        }

        void addChange(ContainerChangeEntry* entry) {
                if (entry->uiEntryDesc == ContainerChangeEntry::AddReference   ||
                    entry->uiEntryDesc == ContainerChangeEntry::SubReference   ||
                    entry->uiEntryDesc == ContainerChangeEntry::DepSubReference) {
                    ContainerChangeEntry *pEntry = getNewEntry();
                    pEntry->uiEntryDesc   = entry->uiEntryDesc;
                    pEntry->uiContainerId = entry->uiContainerId;
                    pEntry->pList         = this;
                } else if(entry->uiEntryDesc == ContainerChangeEntry::Change||
                    entry->uiEntryDesc == ContainerChangeEntry::Create) {
                    ContainerChangeEntry *pEntry = getNewEntry();
                    pEntry->uiEntryDesc   = entry->uiEntryDesc; //ContainerChangeEntry::Change; //TODO: check what I did here (workaround to get created entries into the changelist aswell)
                    pEntry->pFieldFlags   = entry->pFieldFlags;
                    pEntry->uiContainerId = entry->uiContainerId;
                    pEntry->whichField    = entry->whichField;
                    if (pEntry->whichField == 0 && entry->bvUncommittedChanges != 0)
                        pEntry->whichField |= *entry->bvUncommittedChanges;
                    pEntry->pList         = this;
                }
        }
};

void VRSyncNode::printChangeList(OSGChangeList* cl) {
    if (!cl) return;
    cout << endl << "ChangeList:";
    if (cl->getNumChanged() == 0 && cl->getNumCreated() == 0) cout << " no changes " << endl;
    else cout << " node: " << name << ", changes: " << cl->getNumChanged() << ", created: " << cl->getNumCreated() << endl;

    auto printEntry = [&](ContainerChangeEntry* entry) {
        const FieldFlags* fieldFlags = entry->pFieldFlags;
        BitVector whichField = entry->whichField;
        UInt32 id = entry->uiContainerId;

        // ----- print info ---- //
        string type, changeType;
        if (factory->getContainer(id)) type = factory->getContainer(id)->getTypeName();

        switch (entry->uiEntryDesc) {
            case ContainerChangeEntry::Change:
                changeType = " Change            ";
                break;
            case ContainerChangeEntry::AddField:
                changeType = " addField/SubField ";
                break;
            case ContainerChangeEntry::AddReference:
                changeType = " AddReference      ";
                break;
            case ContainerChangeEntry::Create:
                changeType = " Create            ";
                break;
            case ContainerChangeEntry::DepSubReference:
                changeType = " DepSubReference   ";
                break;
            case ContainerChangeEntry::SubReference:
                changeType = " SubReference      ";
                break;
            default:
                changeType = " none              ";;
        }
        cout << "  " << "uiContainerId: " << id << ", changeType: " << changeType << ", container: " << type << endl;
    };

    cout << " Created:" << endl;
    for (auto it = cl->beginCreated(); it != cl->endCreated(); ++it) printEntry(*it);
    cout << " Changes:" << endl;
    for (auto it = cl->begin(); it != cl->end(); ++it) printEntry(*it);
}

VRSyncNode::VRSyncNode(string name) : VRTransform(name) {
    type = "SyncNode";
    applicationThread = dynamic_cast<Thread *>(ThreadManager::getAppThread());

    NodeMTRefPtr node = getNode()->node;
    registerNode(node);

	updateFkt = VRUpdateCb::create("SyncNode update", bind(&VRSyncNode::update, this));
	//VRScene::getCurrent()->addUpdateFkt(updateFkt, 100000);
}

VRSyncNode::~VRSyncNode() {
    cout << "VRSyncNode::~VRSyncNode " << name << endl;
}

VRSyncNodePtr VRSyncNode::ptr() { return static_pointer_cast<VRSyncNode>( shared_from_this() ); }
VRSyncNodePtr VRSyncNode::create(string name) { return VRSyncNodePtr(new VRSyncNode(name) ); }

typedef unsigned char BYTE;

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(BYTE c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(BYTE const* buf, unsigned int bufLen) {
  std::string ret;
  int i = 0;
  int j = 0;
  BYTE char_array_3[3];
  BYTE char_array_4[4];

  while (bufLen--) {
    char_array_3[i++] = *(buf++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';
  }

  return ret;
}

std::vector<BYTE> base64_decode(std::string const& encoded_string) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  BYTE char_array_4[4], char_array_3[3];
  std::vector<BYTE> ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
          ret.push_back(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
  }

  return ret;
}

UInt32 VRSyncNode::getRegisteredContainerID(int syncID) {
    for (auto registered : container) {
        if (registered.second == syncID) return registered.first;
    }
    return -1;
}

struct SerialEntry {
    int localId = 0;
    BitVector fieldMask;
    int len = 0;
    int clen = 0;
    int syncNodeID = -1;
    int uiEntryDesc = -1;
    int fcTypeID = -1;
    int coreID = -1;
    int cplen = 0;
    int parentID = -1;
};

class ourBinaryDataHandler : public BinaryDataHandler {
    public:
        ourBinaryDataHandler() {
            forceDirectIO();
        }

        void read(MemoryHandle src, UInt32 size) {
//            cout << "ourBinaryDataHandler -> read() data.size" << data.size() << " size " << size << endl;
            memcpy(src, &data[0], min((size_t)size, data.size())); //read data from handler into src (sentry.fieldMask)
        }

        void write(MemoryHandle src, UInt32 size) {
            data.insert(data.end(), src, src + size);
        }

        vector<BYTE> data;
};


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

void printContainer (FieldContainerFactoryBase* factory, map<int,int> container) {
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
                for (int i = 0; i < node->getNChildren(); i++) {
                    Node* childNode = node->getChild(i);
                    cout << "   " << childNode->getId() << endl;
                }
            }
        }
    }
}

void VRSyncNode::serialize_entry(ContainerChangeEntry* entry, vector<BYTE>& data, int syncNodeID) {
    UInt32 id = entry->uiContainerId;
    FieldContainer* fcPtr = factory->getContainer(id);
    if (fcPtr) {
        SerialEntry sentry;
        sentry.localId = id;
        sentry.fieldMask = entry->whichField;
        sentry.syncNodeID = syncNodeID;
        sentry.uiEntryDesc = entry->uiEntryDesc;
        sentry.fcTypeID = fcPtr->getTypeId();

        ourBinaryDataHandler handler;
        fcPtr->copyToBin(handler, sentry.fieldMask); //calls handler->write
        sentry.len = handler.data.size();//UInt32(fcPtr->getBinSize(sentry.fieldMask));

        vector<pair<int,int>> children;
        if (factory->findType(sentry.fcTypeID)->isNode()) { // children and cores
            Node* node = dynamic_cast<Node*>(fcPtr);

            if (sentry.fieldMask & Node::CoreFieldMask) { // node core changed
                sentry.coreID = node->getCore()->getId();
            }

            if (sentry.fieldMask & Node::ChildrenFieldMask) { // new child added
                for (int i=0; i<node->getNChildren(); i++) {
                    Node* child = node->getChild(i);
                    int syncID = container[child->getId()] ? container[child->getId()] : -1;
                    children.push_back(make_pair(child->getId(), syncID));
                    cout << "children.push_back " << child->getId() << endl;
                }
            }
        }
        sentry.cplen = children.size();

        data.insert(data.end(), (BYTE*)&sentry, (BYTE*)&sentry + sizeof(SerialEntry));
        data.insert(data.end(), handler.data.begin(), handler.data.end());
//        cout << "data size sentry + handler " << data.size() << endl;
//        if (sentry.clen > 0) data.insert(data.end(), (BYTE*)&childIDs[0], (BYTE*)&childIDs[0] + sizeof(int)*sentry.clen);
//        cout << " total data size " << data.size() << endl;
        if (sentry.cplen > 0) cout << "children " << sentry.cplen << endl; data.insert(data.end(), (BYTE*)&children[0], (BYTE*)&children[0] + sizeof(pair<int,int>)*sentry.cplen);
        cout << "serialize fc " << factory->findType(fcPtr->getTypeId())->getName() << " " << fcPtr->getTypeId() << " > > > sentry: " << sentry.localId << " syncID " << sentry.syncNodeID << " fieldMask " << sentry.fieldMask << " len " << sentry.len << " | encoded: " << data.size() << endl;
    }
}

string VRSyncNode::serialize(ChangeList* clist) {
    int counter = 0;
    cout << "> > >  " << name << " VRSyncNode::serialize()" << endl;
    vector<BYTE> data;
    for (auto it = clist->begin(); it != clist->end(); ++it) {
        ContainerChangeEntry* entry = *it;
        UInt32 id = entry->uiContainerId;

        //if (entry->uiEntryDesc != ContainerChangeEntry::Change) continue;
        serialize_entry(entry, data, container[id]);
        counter++;
    }
    cout << "serialized entries: " << counter << endl;
    cout << "            / " << name << " / VRSyncNode::serialize()" <<"  < < <" << endl;
    return base64_encode(&data[0], data.size());
}

UInt32 VRSyncNode::getLocalId(UInt32 remoteID, int syncID) {
    UInt32 id = -1;
    if (syncID != -1) {
        for (auto c : container) {
            if (c.second == syncID) {
                id = c.first;
                remoteToLocalID[syncID] = id;
                cout << "replaced remoteID " << remoteID;
                remoteID = id; //replace remoteID by localID
                cout << " with localID " << remoteID<< endl;
                break;
            }
        }
    }
    return id;
}

//checks in container if the node with syncID is already been registered
bool VRSyncNode::isRegistered(int syncID) {
    bool registered = false;
    for (auto reg : container) { //check if the FC is already registered, f.e. if nodeCore create entry arrives first a core along with its node will be created before the node create entry arrives
        UInt32 id = reg.first;
        if (reg.second == syncID) {
            registered = true;
            break;
        }
    }
    return registered;
}

//get children IDs and map them to their parents. if a child was already registered, update it's parent
void VRSyncNode::deserializeChildrenData(vector<BYTE>& childrenData, UInt32 fcID, map<int,int>& childToParent) {
    cout << "children " << childrenData.size() << endl;
    map<int,int> children;
    for (int i = 0; i < childrenData.size(); i+=sizeof(pair<int,int>)) { //NOTE: int can be either 4 (assumed here) or 2 bytes, depending on system
        int childID;
        int childSyncID;
        memcpy(&childID, &childrenData[i], sizeof(int));
        memcpy(&childSyncID, &childrenData[i] + sizeof(int), sizeof(int));
        children[childID] = childSyncID;
        childToParent[childSyncID] = fcID;
        cout << "childID " << childID << " childSyncID " << childSyncID << endl;
        //update parent id if child id is registered
        int childFCId = getRegisteredContainerID(childSyncID);
        cout << "registered child id " << childFCId << endl;
        cout << "registered parent id " << childToParent[childSyncID] << endl;
        cout << "id " << fcID << " childToParent[childSyncID]  " << childToParent[childSyncID] << " container " << container[fcID] << endl;
        if (childFCId > 0) { //if the child is already been registered, check the parent id
//                if (id != childToParent[childSyncID]) { //if parent id changed, get parent and set new child
            Node* parent = dynamic_cast<Node*>(factory->getContainer(fcID));
            Node* child = dynamic_cast<Node*>(factory->getContainer(childFCId));
            parent->addChild(child);
            cout << "!!!!!!!!!! update parent id " << container[parent->getId()] << " add child " << container[childFCId] << endl;
            cout << parent->getId() << " children" << endl; //TODO: remove after debugging
            for (int i = 0; i < parent->getNChildren(); i++) {
                cout << parent->getChild(i)->getId() << endl;
            }
//                }
        } else { //TODO: remove after debugging
            Node* node = dynamic_cast<Node*>(factory->getContainer(fcID));
            cout << "children" << endl;
            for (int i = 0; i < node->getNChildren(); i++) {
                cout << node->getChild(i)->getId() << endl;
            }
        }
    }
}

void VRSyncNode::deserializeAndApply(string& data) {
    cout << "> > >  " << name << " VRSyncNode::deserializeAndApply(), received data size: " << data.size() << endl;
    vector<BYTE> vec = base64_decode(data);
    int pos = 0;
    int counter = 0; // remove after debugging
    ourBinaryDataHandler handler; //use ourBinaryDataHandler to somehow apply binary change to fieldcontainer (use connection instead of handler, see OSGRemoteaspect.cpp (receiveSync))
    map<int, int> childToParent; //maps parent ID to its children syncIDs


    //deserialize and collect change and create entries
    while (pos + sizeof(SerialEntry) < vec.size()) {
        SerialEntry sentry = *((SerialEntry*)&vec[pos]);
        cout << "deserialize > > > sentry: " << sentry.localId << " " << sentry.fieldMask << " " << sentry.len << " desc " << sentry.uiEntryDesc << " syncID " << sentry.syncNodeID << " at pos " << pos << endl;

        UInt32 id = getLocalId(sentry.localId, sentry.syncNodeID);// map remote id to local id if exist (otherwise id = -1)

        pos += sizeof(SerialEntry);
        vector<BYTE> FCdata;
        FCdata.insert(FCdata.end(), vec.begin()+pos, vec.begin()+pos+sentry.len);
        pos += sentry.len;

        vector<BYTE> childrenData;
        childrenData.insert(childrenData.end(), vec.begin()+pos, vec.begin()+pos+sentry.cplen*sizeof(pair<int,int>)); //TODO: get rid of the extra vector and fill childToParent map asap
        if (childrenData.size() > 0) deserializeChildrenData(childrenData, id, childToParent);

        pos += sentry.cplen * sizeof(pair<int,int>);

        cout << "childToParent " << childToParent.size() << " " << children.size() << endl; //TODO: remove after debugging
        for (auto child : childToParent) {
            cout << child.first << " " << child.second << endl;
        }

        counter++;

        FieldContainerRecPtr fcPtr = nullptr; // Field Container to apply changes to

        if (sentry.uiEntryDesc == ContainerChangeEntry::Create) { //if create and not registered | sentry.uiEntryDesc == ContainerChangeEntry::Create &&
            FieldContainerType* fcType = factory->findType(sentry.fcTypeID);

            if (!isRegistered(sentry.syncNodeID)) { //if not registered create a FC of fcType
                fcPtr = fcType->createContainer();
                registerContainer(fcPtr.get(), sentry.syncNodeID);
                id = fcPtr.get()->getId();
                remoteToLocalID[sentry.syncNodeID] = id;
            } else fcPtr = factory->getContainer(getRegisteredContainerID(sentry.syncNodeID));//else its registered. then get the existing FC

            if (fcType->isNode()) { createNode(fcPtr, sentry.syncNodeID, childToParent);  cout << "isNode" << endl;}
            else if (fcType->isNodeCore()) { createNodeCore(fcPtr, sentry.syncNodeID, childToParent); cout << id << " isCore" << endl; }
        }
        else fcPtr = factory->getContainer(id);

        if(fcPtr == nullptr) { cout << "no container found with id " << id << " syncNodeID " << sentry.syncNodeID << endl; continue; } //TODO: This is causing the WARNING: Action::recurse: core is NULL, aborting traversal.


        handler.data.insert(handler.data.end(), FCdata.begin(), FCdata.end()); //feed handler with FCdata
        fcPtr->copyFromBin(handler, sentry.fieldMask); //calls handler->read

        if (id != -1) {
            if (count(syncedContainer.begin(), syncedContainer.end(), id) < 1) {
                syncedContainer.push_back(id);
                cout << "syncedContainer.push_back " << id << endl;
            }
        }
    }

    //DEBUG: print registered container
    cout << "print registered container: " << endl;
    for (auto c : container){
        UInt32 id = c.first;
        FieldContainer* fc = factory->getContainer(id);
        cout << id << " syncNodeID " << c.second ;
        if (factory->getContainer(id)){
            cout << " " << fc->getTypeName();
        }
        cout << endl;
    }

    printContainer(factory, container);

    cout << "print remoteToLocalID: " << endl;
    for (auto c : remoteToLocalID){
        cout << c.first << " " << c.second << endl;
    }

    //Thread::getCurrentChangeList()->commitChanges(ChangedOrigin::Sync);
    //Thread::getCurrentChangeList()->commitChangesAndClear(ChangedOrigin::Sync);

    cout << "deserialized entries: " << counter << endl;
    cout << "            / " << name << " VRSyncNode::deserializeAndApply()" << "  < < <" << endl;
}

void VRSyncNode::createNode(FieldContainerRecPtr& fcPtr, int syncNodeID, map<int,int>& childToParent) {
    Node* node = dynamic_cast<Node*>(fcPtr.get());
    int parentId = childToParent[syncNodeID] ? childToParent[syncNodeID] : container.begin()->first;
    cout << "parentId " << parentId << endl;
    FieldContainer* parentFC = factory->getContainer(parentId);
    Node* parent = dynamic_cast<Node*>(parentFC);
    parent->addChild(node);
    cout << "!!!!! parent->addChild(node) parent " << container[parentId] << " container " << container[parentId] << " add child " << container[node->getId()] << endl;
    cout << "children" << endl;
    for (int i = 0; i < parent->getNChildren(); i++) {
        cout << parent->getChild(i)->getId() << endl;
    }
//                if (isRegistered) node->getParent()->subChild(id); //delete parent ref if was registered by core

    //check if core was already registered. if yes, set its parent and nodes core
    int coreId = ++syncNodeID;
    for (auto entry : container) {
        UInt32 id = entry.first;
        int syncID = entry.second;
        if (coreId == syncID) {
            FieldContainer* coreFC = factory->getContainer(id);
            NodeCore* core = dynamic_cast<NodeCore*>(coreFC);
            node->setCore(core);
            break;
        }
    }
}

void VRSyncNode::createNodeCore(FieldContainerRecPtr& fcPtr, int syncNodeID, map<int,int>& childToParent) {
    NodeCore* core = dynamic_cast<NodeCore*>(fcPtr.get());
    //get node via syncID: node syncID is core syncID-1
    UInt32 nodeSyncID = --syncNodeID;
    bool found = false;
    for (auto entry : container) {
        UInt32 id = entry.first;
        int syncID = entry.second;
        if (syncID == nodeSyncID) {
            cout << "found container entry for syncID " << nodeSyncID << " id " << id << endl;
            found = true;
            FieldContainer* nodeFC = factory->getContainer(id);
            Node* node = dynamic_cast<Node*>(nodeFC);
            node->setCore(core);
            cout << "added core " << core->getId() << " to node " << node->getId() << endl;
            break;
        }
    }
    if (!found) {
        NodeRefPtr nodePtr = Node::create();
        Node* node = nodePtr.get();
        node->setCore(core);
        int parentId = childToParent[nodeSyncID] ? childToParent[nodeSyncID] : container.begin()->first; //if parent is not registered yet, take first registered Node as parent meanwhile
        cout << "parent id " << parentId << " for node syncID " << nodeSyncID << endl;
        FieldContainer* parentFC = factory->getContainer(parentId);
        Node* parent = dynamic_cast<Node*>(parentFC);
        parent->addChild(node); //TODO reassure

        cout << "!!!!! parent->addChild(node) parent " << container[parentId] << " add child " << node->getId() << endl;
        cout << "children" << endl;
        for (int i = 0; i < parent->getNChildren(); i++) {
            cout << parent->getChild(i)->getId() << endl;
        }
        registerContainer(node, nodeSyncID);
        cout << "registered container " << node->getId() << " " << container[node->getId()] << " " << nodeSyncID << endl;
        remoteToLocalID[nodeSyncID] = node->getId();
    }
}

//copies state into a CL and serializes it as string
string VRSyncNode::copySceneState() {
    OSGChangeList* localChanges = (OSGChangeList*)ChangeList::create();
    localChanges->fillFromCurrentState();
    printChangeList(localChanges);

    string data = serialize(localChanges);
    delete localChanges;
    return data;
}

void VRSyncNode::printRegistredContainers() {
    cout << endl << "registered container:" << endl;
    for (auto c : container){
        UInt32 id = c.first;
        FieldContainer* fc = factory->getContainer(id);
        cout << " " << id << " " << getRegisteredContainerID(c.second) << " syncNodeID " << c.second << " " << container[id];
        if (factory->getContainer(id)){
            cout << " " << fc->getTypeName() << " Refs " << fc->getRefCount();
        }
        cout << endl;
    }
}

void VRSyncNode::printSyncedContainers() {
    cout << endl << "synced container:" << endl;
    for (int id : syncedContainer) cout << id << endl;
}

// checks if a container was changed by remote
bool VRSyncNode::isRemoteChange(const UInt32& id) {
    return bool(::find(syncedContainer.begin(), syncedContainer.end(), id) != syncedContainer.end());
}

bool VRSyncNode::isRegistred(const UInt32& id) {
    return bool(container.count(id));
}

OSGChangeList* VRSyncNode::getFilteredChangeList() {
    // go through all changes, gather changes where the container is known (in containers)
    // create local changelist with changes of containers of the subtree of this sync node :D

    ChangeList* cl = applicationThread->getChangeList();
    if (cl->getNumChanged() + cl->getNumCreated() == 0) return 0;

    OSGChangeList* localChanges = (OSGChangeList*)ChangeList::create();
    vector<UInt32> createdNodes;

    // add created entries to local CL
    for (auto it = cl->beginCreated(); it != cl->endCreated(); ++it) {
        ContainerChangeEntry* entry = *it;
        UInt32 id = entry->uiContainerId;
        if (isRemoteChange(id)) continue;

        if (isRegistred(id)) { // TODO: just created containers cannot be registered!, this is acctually just getting the initial sync node, which is bad!
            localChanges->addChange(entry);
            createdNodes.push_back(id);
        }
    }

    auto justCreated = [&](UInt32 id) { return bool(::find(createdNodes.begin(), createdNodes.end(), id) != createdNodes.end()); };

    // add changed entries to local CL
    for (auto it = cl->begin(); it != cl->end(); ++it) {
        ContainerChangeEntry* entry = *it;
        UInt32 id = entry->uiContainerId;
        if (isRemoteChange(id)) continue;
        if (isRegistred(id) || justCreated(id)) {
            localChanges->addChange(entry);
        }
    }

    return localChanges;
}

void VRSyncNode::gatherCreatedContainers(OSGChangeList* localChanges) {
    if (!localChanges) return;
// check for addChild changes
    createdNodes.clear();
    for (auto it = localChanges->begin(); it != localChanges->end(); ++it) {
        cout << "update child changes" << endl;
        ContainerChangeEntry* entry = *it;
        UInt32 id = entry->uiContainerId;
        FieldContainer* fct = factory->getContainer(id);
        if (fct) {
            //string type = fct->getTypeName();
            if (factory->findType(fct->getTypeId())->isNode() && entry->whichField & Node::ChildrenFieldMask) { //if the children filed mask of node has changed, we check if a new child was added
                Node* node = dynamic_cast<Node*>(fct);
                for (int i=0; i<node->getNChildren(); i++) {
                    Node* child = node->getChild(i);
                    if (!container.count(child->getId())) { //check if it is an unregistered child
                        cout << "register child for node " << node->getId() << " entry type " << entry->uiEntryDesc << endl;
                        vector<int> newNodes = registerNode(child);
                        //createdNodes.push_back(child->getId()); //TODO
//                        createdNodes.insert(createdNodes.end(), newNodes.begin(), newNodes.end());
                        for (int i = 0; i<newNodes.size(); ++i){
                            createdNodes.push_back(newNodes[i]);
//                            cout << "pushed_back newNodes[i] " << newNodes[i] << endl;
//                            cout << "search id in CL " << endl;
//                            for (auto it = cl->begin(); it != cl->end(); ++it) {
//                                ContainerChangeEntry* entry = *it;
//                                UInt32 id = entry->uiContainerId;
//                                if (id == newNodes[i]) cout << "found entry in global CL!!!!" << newNodes[i] << endl;
//                            }
//                            cout << "search id in syncedContainer " << endl;
//                            for (int id : syncedContainer) {
//                                if (id == newNodes[i]) cout << "found entry in syncedContainer!!!!" << newNodes[i] << endl;
//                            }
                        }
                    }
                }
                cout << "node get core field id " << node->CoreFieldId << " core field mask " << node->CoreFieldMask << endl;
                NodeCore* core = node->getCore();
                for (auto p : core->getParents()) {
                    cout << "parent type " << p->getTypeName() << " id " << p->getId() << " of core " << core->getId() << endl;
                }
            }
            if (factory->findType(fct->getTypeId())->isNode() && entry->whichField & Node::CoreFieldMask) { // core change of known node
                cout << "  node core changed!" << endl;
                Node* node = dynamic_cast<Node*>(fct);
                registerContainer(node->getCore(), container.size());
                createdNodes.push_back(node->getCore()->getId()); //TODO
            }
        }
    }
}

void VRSyncNode::broadcastChangeList(OSGChangeList* cl, bool doDelete) {
    if (!cl) return;
    string data = serialize(cl); // serialize changes in new change list (check OSGConnection for serialization Implementation)
    if (doDelete) delete cl;
    for (auto& remote : remotes) remote.second->send(data); // send over websocket to remote
}

//update this SyncNode
void VRSyncNode::update() {
    auto localChanges = getFilteredChangeList();
    if (!localChanges) return;
    cout << endl << " > > >  " << name << " VRSyncNode::update(), local created: " << localChanges->getNumCreated() << ", local changes: " << localChanges->getNumChanged() << endl;

    printRegistredContainers(); // DEBUG: print registered container
    //printSyncedContainers();
    gatherCreatedContainers(localChanges);
    printRegistredContainers(); // DEBUG: print registered container
    printChangeList(localChanges);

    broadcastChangeList(localChanges, true);
    syncedContainer.clear();
    cout << "            / " << name << " VRSyncNode::update()" << "  < < < " << endl;
}

void VRSyncNode::registerContainer(FieldContainer* c, int syncNodeID) {
    cout << " VRSyncNode::registerContainer " << getName() << " container: " << c->getTypeName() << " at fieldContainerId: " << c->getId() << endl;
    container[c->getId()] = syncNodeID;
}

//returns registered IDs
vector<int> VRSyncNode::registerNode(Node* node) {
    vector<int> res;
    vector<int> localRes;
    vector<int> recursiveRes;
    NodeCoreMTRefPtr core = node->getCore();
    cout << "register node " << node->getId() << endl;

    registerContainer(node, container.size());
    if (!core) cout << "no core" << core << endl;
    cout << "register core " << core->getId() << endl;

    registerContainer(core, container.size());
    localRes.push_back(node->getId());
    localRes.push_back(core->getId());
    for (int i=0; i<node->getNChildren(); i++) {
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

void VRSyncNode::startInterface(int port) {
    socket = VRSceneManager::get()->getSocket(port);
    socketCb = new VRHTTP_cb( "VRSyncNode callback", bind(&VRSyncNode::handleChangeList, this, _1) );
    socket->setHTTPCallback(socketCb);
    socket->setType("http receive");
}

string asUri(string host, int port, string name) {
    return "ws://" + host + ":" + to_string(port) + "/" + name;
}

//Add remote Nodes to sync with
void VRSyncNode::addRemote(string host, int port, string name) {
    string uri = asUri(host, port, name);
    remotes[uri] = VRSyncRemote::create(uri);

//    OSGChangeList* cl = (OSGChangeList*)ChangeList::create();
//    int id = container.begin()->first;
//    cl->fillFromCurrentState(id,1);
//    cout << "add remote cl->fillFromCurrentState " << id << endl;
//    OSGChangeList* clFilter = (OSGChangeList*)ChangeList::create();
//    //add create entries
//    for (auto it = cl->beginCreated(); it != cl->endCreated(); ++it) {
//        ContainerChangeEntry* entry = *it;
//        int id = entry->uiContainerId;
//        string type = "";
//        if (factory->getContainer(id)){
//            type += factory->getContainer(id)->getTypeName();
//        }
////        if (type == "Node" || type == "Group" || type == "Geometry" || type == "Transform")
//        clFilter->addChange(entry);
//    }
//    //add change entries
//    for (auto it = cl->begin(); it != cl->end(); ++it) {
//        ContainerChangeEntry* entry = *it;
//        int id = entry->uiContainerId;
//        string type = "";
//        if (factory->getContainer(id)){
//            type += factory->getContainer(id)->getTypeName();
//        }
////        if (type == "Node" || type == "Group" || type == "Geometry" || type == "Transform")
//        clFilter->addChange(entry);
//    }
//
//    printChangeList(clFilter);
}

void VRSyncNode::handleChangeList(void* _args) { //TODO: rename in handleMessage
    HTTP_args* args = (HTTP_args*)_args;
    if (!args->websocket) cout << "AAAARGH" << endl;

    int client = args->ws_id;
    string msg = args->ws_data;

    //cout << "GOT CHANGES!! " << endl;
    //cout << name << ", received msg: "  << msg << endl;//<< " container: ";
    if (msg == "hello") {
        cout << "received hello from remote" << endl;
        //TODO: send it only back to the sender
//        string data = copySceneState();
//        for (auto& remote : remotes) {
//            remote.second->send(data);
//            //cout << name << " sending " << data  << endl;
//        }
    }
    else deserializeAndApply(msg);
}

//broadcast message to all remote nodes
void VRSyncNode::broadcast(string message){
    for (auto& remote : remotes) {
        if (!remote.second->send(message)) {
            cout << "Failed to send message to remote." << endl;
        }
    }
}


// ------------------- VRSyncRemote


VRSyncRemote::VRSyncRemote(string uri) : uri(uri) {
    socket = VRWebSocket::create("sync node ws");
    //cout << "VRSyncRemote::VRSyncRemote " << uri << endl;
    if (uri != "") connect();
    //map IDs
}

VRSyncRemote::~VRSyncRemote() { cout << "~VRSyncRemote::VRSyncRemote" << endl; }
VRSyncRemotePtr VRSyncRemote::create(string name) { return VRSyncRemotePtr( new VRSyncRemote(name) ); }

void VRSyncRemote::connect() {
    //cout << "VRSyncRemote, try connecting to " << uri << endl;
    bool result = socket->open(uri);
    if (!result) cout << "VRSyncRemote, Failed to open websocket to " << uri << endl;
    //else cout << "VRSyncRemote, connected to " << uri << endl;
    if (result) send("hello");
}

bool VRSyncRemote::send(string message){
    if (!socket->sendMessage(message)) return 0;
    return 1;
}








