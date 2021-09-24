#include "VRICEclient.h"
#include "VRTCPClient.h"
#include "../rest/VRRestClient.h"
#include "../rest/VRRestResponse.h"
#include "core/utils/toString.h"
#include "core/scene/VRScene.h"
#include "core/gui/VRGuiConsole.h"

using namespace OSG;

VRICEClient::VRICEClient() {
    broker = VRRestClient::create();
    client = VRTCPClient::create();
    client->setGuard("TCPPVR\n");
    updateCb = VRUpdateCb::create("ice update", bind(&VRICEClient::update, this));
    VRScene::getCurrent()->addTimeoutFkt(updateCb, 0, 1000);
}

VRICEClient::~VRICEClient() {
    removeUser(uID);
}

VRICEClientPtr VRICEClient::create() { return VRICEClientPtr( new VRICEClient() ); }
VRICEClientPtr VRICEClient::ptr() { return static_pointer_cast<VRICEClient>(shared_from_this()); }

void VRICEClient::setTurnServer(string url, string ip) {
    turnURL = url;
    turnIP = ip;
    VRConsoleWidget::get("Collaboration")->write( " ICE "+toString((void*)this)+" set turn server "+url+" ("+ip+")\n");
}

void VRICEClient::setName(string n) {
    name = n;
    uID = broker->get(turnURL+"/regUser.php?NAME="+n)->getData();
    //cout << "register name " << n << " -> " << uID << endl;
    VRConsoleWidget::get("Collaboration")->write( " ICE "+toString((void*)this)+", set named "+name+" ("+uID+")\n");
}

void VRICEClient::removeUser(string uid) {
    broker->get(turnURL+"/remUser.php?UID="+uid);
}

string VRICEClient::getID() { return uID; }

void VRICEClient::onEvent( function<void(string)> f ) {
    onEventCb = f;
    client->onConnect([this](){;});
}

void VRICEClient::onMessage( function<void(string)> f ) {
    onMessageCb = f;
    client->onMessage(f);
}

void VRICEClient::update() { // TODO: test and debug async
    if (!usrGuard) {
        usrGuard = true;
        pollUsers(true);
    }

    if (!msgGuard) {
        msgGuard = true;
        pollMessages(true);
    }
}

void VRICEClient::updateUsers() {
    string status = " ICE "+name+" updateUsers:";
    users.clear();
    for (auto usrData : splitString(usersList, '\n')) {
        auto data = splitString(usrData, '|');
        if (data.size() != 2) continue;
        string name = data[0];
        string uid = data[1];
        users[uid] = name;
        status += " "+name+"("+uid+")";
    }
    VRConsoleWidget::get("Collaboration")->write( status+"\n");
}

map<string, string> VRICEClient::getUsers() { return users; }

void VRICEClient::send(string otherID, string msg) {
    msg = VRRestResponse::uriEncode(msg);
    broker->get(turnURL+"/addMessage.php?ORG="+uID+"&UID="+otherID+"&MSG="+msg)->getData();
    VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" send to "+otherID+": '"+msg+"'\n");
}

void VRICEClient::processUsers(string data) {
    if (data != usersList && data != "") {
        usersList = data;
        updateUsers();
        if (onEventCb) {
            onEventCb("users changed");
        }
    }
    usrGuard = false;
}

void VRICEClient::processMessages(string data) {
    if (data != "") if (onEventCb) onEventCb("message|"+data);
    msgGuard = false;
}

void VRICEClient::processRespUsers(VRRestResponsePtr r) { processUsers(r->getData()); }
void VRICEClient::processRespMessages(VRRestResponsePtr r) { processMessages(r->getData()); }

void VRICEClient::pollMessages(bool async) {
    if (uID == "") {
        msgGuard = false;
        return;
    }

    if (!async) {
        string data = broker->get(turnURL+"/getMessages.php?UID="+uID)->getData();
        processMessages(data);
    } else {
        auto cb = VRRestCb::create("icePollMsgs", bind(&VRICEClient::processRespMessages, this, placeholders::_1) );
        broker->getAsync(turnURL+"/getMessages.php?UID="+uID, cb);
    }
}

void VRICEClient::pollUsers(bool async) {
    if (!async) {
        string data = broker->get(turnURL+"/listUsers.php")->getData();
        processUsers(data);
    } else {
        auto cb = VRRestCb::create("icePollUsers", bind(&VRICEClient::processRespUsers, this, placeholders::_1) );
        broker->getAsync(turnURL+"/listUsers.php", cb);
    }
}

string VRICEClient::getUserName(string ID) {
    if (users.count(ID)) return users[ID];
    return "";
}

vector<string> VRICEClient::getUserID(string n) {
    vector<string> res;
    for (auto u : users) if (u.second == n) res.push_back(u.first);
    return res;
}

void VRICEClient::connectTo(string otherID) {
    if (uID == "" || otherID == "") {
        cout << "VRICEClient::connectTo failed, empty ID" << endl;
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" connectTo failed, empty ID "+uID+", "+otherID+"\n", "red");
        return;
    }
    string uid1 = uID;
    string uid2 = otherID;
    //cout << "VRICEClient::connectTo, name/uid: " << name << "/" << uid1 << " other/uid " << users[uid2] << "/" << uid2 << endl;

    if (!users.count(uid1)) {
        cout << "VRICEClient::connectTo failed, own ID " << uid1 << " not in users!" << endl;
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" connectTo failed, own ID "+uid1+" not in users\n", "red");
        return;
    }

    if (!users.count(uid2)) {
        cout << "VRICEClient::connectTo failed, others ID " << uid2 << " not in users!" << endl;
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" connectTo failed, other ID "+uid2+" not in users\n", "red");
        return;
    }

    string data = broker->get(turnURL+"/getConnection.php?UID="+uid1+"&UID2="+uid2)->getData();
    auto params = splitString(data, ':');
    if (params.size() == 0) {
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect to "+turnIP+" faild! no port received from turn server "+turnURL+", received '"+data+"'\n", "red");
        return;
    }

    int port = toInt( params[0] );
    if (port == 0) {
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect to "+turnIP+" faild! no port received from turn server "+turnURL+", received '"+data+"'\n", "red");
        return;
    }

    //cout << " -> port " << port << endl;
    VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect to "+users[uid2]+"("+uid2+") over "+turnIP+":"+toString(port)+"\n");
    client->connect(turnIP, port);
}

void VRICEClient::sendTCP(string msg) {
    if (!client->connected()) return;
    client->send(msg, "TCPPVR\n");
}

VRTCPClientPtr VRICEClient::getClient() { return client; }

