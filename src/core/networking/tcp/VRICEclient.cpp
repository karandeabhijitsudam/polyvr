#include "VRICEclient.h"
#include "VRTCPClient.h"
#include "../udp/VRUDPClient.h"
#include "../tcp/VRTCPUtils.h"
#include "../rest/VRRestClient.h"
#include "../rest/VRRestResponse.h"
#include "core/utils/toString.h"
#include "core/scene/VRScene.h"
#include "core/scene/VRSceneManager.h"
#ifndef WITHOUT_GTK
#include "core/gui/VRGuiConsole.h"
#endif

using namespace OSG;

VRICEClient::VRICEClient() : VRNetworkClient("unnamed") {
    protocol = "ice";
    broker = VRRestClient::create();
    updateCb = VRUpdateCb::create("ice update", bind(&VRICEClient::update, this));
    VRScene::getCurrent()->addTimeoutFkt(updateCb, 0, 1000);
}

VRICEClient::~VRICEClient() {
    removeUser(uID);
}

VRICEClientPtr VRICEClient::create() {
    auto c = VRICEClientPtr( new VRICEClient() );
    VRSceneManager::get()->regNetworkClient(c);
    return c;
}

VRICEClientPtr VRICEClient::ptr() { return static_pointer_cast<VRICEClient>(shared_from_this()); }

void VRICEClient::setTurnServer(string url) {
    turnURL = url;
    turnIP = VRTCPUtils::getHostIP(url);
#ifndef WITHOUT_GTK
    VRConsoleWidget::get("Collaboration")->write( " ICE "+toString((void*)this)+" set turn server "+url+" ("+turnIP+")\n");
#endif
}

string VRICEClient::getTurnServer() { return turnURL + " ("+turnIP+")"; }

void VRICEClient::setName(string n) {
    name = n;
    uID = broker->get(turnURL+"/regUser.php?NAME="+n)->getData();
    //cout << "register name " << n << " -> " << uID << endl;
#ifndef WITHOUT_GTK
    VRConsoleWidget::get("Collaboration")->write( " ICE "+toString((void*)this)+", set named "+name+" ("+uID+")\n");
#endif
}

void VRICEClient::removeLocalUser(string uid) {
    if (users.count(uid)) users.erase(uid);
    else {
        cout << "VRICEClient::removeLocalUser Warning, no user with ID " << uid << endl;
    }
}

void VRICEClient::removeUser(string uid) {
    broker->get(turnURL+"/remUser.php?UID="+uid);
}

string VRICEClient::getID() { return uID; }

void VRICEClient::onEvent( function<void(string)> f ) {
    onEventCb = f;
    //getClient(otherID)->onConnect([this](){;});
}

void VRICEClient::onMessage( function<void(string)> f ) {
    onMessageCb = f;
    //getClient(otherID)->onMessage(f);
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
#ifndef WITHOUT_GTK
    VRConsoleWidget::get("Collaboration")->write( status+"\n");
#endif
}

map<string, string> VRICEClient::getUsers() { return users; }

void VRICEClient::send(string otherID, string msg) {
    msg = VRRestResponse::uriEncode(msg);
    broker->get(turnURL+"/addMessage.php?ORG="+uID+"&UID="+otherID+"&MSG="+msg)->getData();
#ifndef WITHOUT_GTK
    VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" send to "+otherID+": '"+msg+"'\n");
#endif
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
    if (data != "") {
        auto messages = splitString(data, ">>>---");
        for (int i=0; i<messages.size(); i++)  {
            string msg = messages[i];
            if (onEventCb) onEventCb("message|"+msg);
        }
    }
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

VRNetworkClientPtr VRICEClient::getClient(string otherID, CHANNEL channel) {
    if (channel == NONE) return 0;

    if (!clients.count(otherID) || !clients[otherID].count(channel)) {
        if (channel == SCENEGRAPH) {
            auto client = VRTCPClient::create("ice-sync");
            client->setGuard("TCPPVR\n");
            clients[otherID][channel] = client;
        }

        if (channel == AUDIO) {
            clients[otherID][channel] = VRUDPClient::create("ice-audio");
        }
    }

    return clients[otherID][channel];
}

map<string, map<VRICEClient::CHANNEL, VRNetworkClientPtr> > VRICEClient::getClients() { return clients; }

void VRICEClient::connectTo(string otherID) {
    if (uID == "" || otherID == "") {
        cout << "VRICEClient::connectTo failed, empty ID" << endl;
#ifndef WITHOUT_GTK
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" connectTo failed, empty ID "+uID+", "+otherID+"\n", "red");
#endif
        return;
    }
    string uid1 = uID;
    string uid2 = otherID;
    //cout << "VRICEClient::connectTo, name/uid: " << name << "/" << uid1 << " other/uid " << users[uid2] << "/" << uid2 << endl;

    if (!users.count(uid1)) {
        cout << "VRICEClient::connectTo failed, own ID " << uid1 << " not in users!" << endl;
#ifndef WITHOUT_GTK
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" connectTo failed, own ID "+uid1+" not in users\n", "red");
#endif
        return;
    }

    if (!users.count(uid2)) {
        cout << "VRICEClient::connectTo failed, others ID " << uid2 << " not in users!" << endl;
#ifndef WITHOUT_GTK
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+" connectTo failed, other ID "+uid2+" not in users\n", "red");
#endif
        return;
    }

    string data = broker->get(turnURL+"/getConnection.php?UID="+uid1+"&UID2="+uid2)->getData();
    auto params = splitString(data, ':');
    if (params.size() == 0) {
#ifndef WITHOUT_GTK
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect to "+turnIP+" faild! no port received from turn server "+turnURL+", received '"+data+"'\n", "red");
#endif
        return;
    }

    int port1 = toInt( params[0] );
    if (port1 == 0) {
#ifndef WITHOUT_GTK
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect to "+turnIP+" faild! no port received from turn server "+turnURL+", received '"+data+"'\n", "red");
#endif
        return;
    }

    //cout << " -> port " << port1 << endl;
#ifndef WITHOUT_GTK
    VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect to "+users[uid2]+"("+uid2+") over "+turnIP+":"+toString(port1)+", received '"+data+"'\n");
#endif
    getClient(otherID, SCENEGRAPH)->connect(turnIP, port1);

    if (params.size() >= 3) {
        int port2 = toInt( params[3] );
        if (port2 != 0) {
            auto cli = getClient(otherID, AUDIO);
            cli->connect(turnIP, port2);
            cli->send("hi"); // to register the client port on turn server
        }
#ifndef WITHOUT_GTK
        VRConsoleWidget::get("Collaboration")->write( " ICE "+name+"("+uid1+"): connect AUDIO to "+users[uid2]+"("+uid2+") over "+turnIP+":"+toString(port2)+", received '"+data+"'\n");
#endif
    }
}

void VRICEClient::sendTCP(string otherID, string msg, CHANNEL channel) {
    auto cli = dynamic_pointer_cast<VRTCPClient>( getClient(otherID, channel) );
    if (!cli) return;
    if (!cli->connected()) return;
    cli->send(msg, "TCPPVR\n");
}

