#pragma once

#include "core/setup/devices/VRDevice.h"
#include "core/networking/VRWebSocket.h"

#include "VRLeapFrame.h"


OSG_BEGIN_NAMESPACE;


class VRLeap : public VRDevice {
    private:
        vector<std::function<void(VRLeapFramePtr)>> frameCallbacks;

        string host{"localhost"};
        int port{6437};
        string connectionStatus{"not connected"};

        VRWebSocket webSocket;
        bool transformed{false};
        Matrix4d transformation;
        bool calibrate{false};
        string serial;

        void newFrame(Json::Value json);
        Pose computeCalibPose(vector<PenPtr>& pens);

    public:
        VRLeap();

        ~VRLeap() {
            cout << "~VRLeap" << endl;
            frameCallbacks.clear();
            webSocket.close();
        }

        static VRLeapPtr create();

        bool reconnect();

        string getHost();
        void setHost(string newHost);
        int getPort();
        void setPort(int newPort);
        string getConnectionStatus();
        string getSerial();

        string getAddress();
        void setAddress(string a);

        void registerFrameCallback(std::function<void(VRLeapFramePtr)> func);
        void clearFrameCallbacks();
        void setPose(Pose pose);
        void setPose(Vec3d pos, Vec3d dir, Vec3d up);
        Matrix4d getTransformation();

        void startCalibration();
        void stopCalibration();
};

OSG_END_NAMESPACE;
