#include "virtuose.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <fcntl.h>

#include <OpenSG/OSGQuaternion.h>

#define CHECK(x) { \
  int result = (x); \
  if (result != 0) { \
    fprintf(stderr, "\nRuntime error: %s returned %d at %s:%d", #x, result, __FILE__, __LINE__); \
    fprintf(stderr, "\n err msg: %s", virtGetErrorMessage(virtGetErrorCode(vc))); \
  } \
}

#define CHECK_INIT(vc) \
if (vc == NULL) { \
  fprintf(stderr, "\nRuntime error: virtOpen returned 0 at %s:%d", __FILE__, __LINE__); \
  fprintf(stderr, "\n err msg: %s", virtGetErrorMessage(virtGetErrorCode(vc))); \
}

OSG_BEGIN_NAMESPACE;
using namespace std;


/*

haptic coordinate system

     z
     |
     x--y

to OSG
y -> x
z -> y
x -> z
Vec3f(y,z,x);
virtVec(z,x,y);

*/

virtuose::virtuose() {
    isAttached = false;
    gripperPosition = 0.0f;
    gripperSpeed = 0.0f;

}

virtuose::~virtuose() {
    disconnect();
}

bool virtuose::connected() { return (vc != 0); }

bool checkVirtuoseIP(string IP) { // TODO: try if haptic is found -> right port?
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sockfd, F_SETFL, O_NONBLOCK); // TODO: try blocking but with timeout
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(3131);  // Could be anything
    inet_pton(AF_INET, IP.c_str(), &sin.sin_addr);

    if (connect(sockfd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
        printf("No haptic device at IP %s: %d (%s)\n", IP.c_str(), errno, strerror(errno));
        return false;
    }
    return true;
}

void virtuose::connect(string IP) {
    disconnect();
    //if (!checkVirtuoseIP(IP)) return; // TODO: does not work
    cout << "Open virtuose " << IP << endl;
    vc = virtOpen(IP.c_str());
    CHECK_INIT(vc);
    if (vc == 0) return;

    timestep = 0.003f;

	//CHECK( virtSetCommandType(vc, COMMAND_TYPE_IMPEDANCE) );
	CHECK( virtSetCommandType(vc, COMMAND_TYPE_VIRTMECH) );
	commandType = COMMAND_TYPE_VIRTMECH;
	CHECK( virtSetDebugFlags(vc, DEBUG_SERVO|DEBUG_LOOP) );
	CHECK( virtSetIndexingMode(vc, INDEXING_ALL_FORCE_FEEDBACK_INHIBITION) );
	CHECK( virtSetTimeStep(vc, timestep) );

    float identity[7] = {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f};
    //float baseFrame[7] = { 0.0f, 0.0f, 0.0f, 0.70710678f, 0.0f, 0.70710678f, 0.0f };
	//virtActiveSpeedControl(vc, 0.04f, 10.0f);
	CHECK( virtSetBaseFrame(vc, identity) );
    CHECK( virtSetObservationFrame(vc, identity) );
	CHECK( virtSetPowerOn(vc, 1) );
	//virtSetPeriodicFunction(vc, callback, &timestep, this);
    setSimulationScales(1.0f,0.1f);





}

void virtuose::disconnect() {
    if (vc) {
        CHECK( virtSetPowerOn(vc, 0) );
		CHECK( virtDetachVO(vc) );
		//CHECK( virtStopLoop(vc) );
		CHECK( virtClose(vc) );
        vc = 0;
        isAttached = false;
        CHECK( virtSetCommandType(vc, COMMAND_TYPE_IMPEDANCE) );
        commandType = COMMAND_TYPE_IMPEDANCE;

    }
}

void virtuose::setSimulationScales(float translation, float forces) {
    CHECK( virtSetSpeedFactor(vc, translation) );
    CHECK( virtSetForceFactor(vc, forces) );
}

void virtuose::applyForce(Vec3f force, Vec3f torque) {
    float f[6] = { force[2], force[0], force[1], torque[2], torque[0], torque[1] };
    CHECK( virtAddForce(vc, f) );
}

Matrix virtuose::getPose() {
    float f[7]= {0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,1.0f};

    if(commandType == COMMAND_TYPE_IMPEDANCE) {
        CHECK( virtGetAvatarPosition(vc, f) );
    }
    //CHECK( virtGetPhysicalPosition(vc, f) );

    Matrix m;

    Vec3f pos(f[1], f[2], f[0]);
    Quaternion q;
    q.setValue(f[4], f[5], f[3]);
    m.setRotate(q);
    m.setTranslate(pos);
    return m;
}

Vec3f virtuose::getForce() {
    float force[6];
    CHECK(virtGetForce(vc, force));
    Vec3f vel = Vec3f(force[4],force[5],force[3]);
    return vel;
}



void virtuose::attachTransform(VRTransform* trans) {
    isAttached = true;
    attached = trans;
    btMatrix3x3 t = trans->getPhysics()->getInertiaTensor();
    t.setRotation(btQuaternion(btVector3(0.0,1.0,0.0),90.0));
    t.setRotation(btQuaternion(btVector3(1.0,0.0,0.0),90.0));
    /*float inertia[9] = {(float) t.getRow(0).getX(),(float) t.getRow(0).getY(),(float) t.getRow(0).getZ(),
          (float) t.getRow(1).getX(),(float) t.getRow(1).getY(),(float) t.getRow(1).getZ(),
          (float) t.getRow(2).getX(),(float) t.getRow(2).getY(),(float) t.getRow(2).getZ()};
    */
    float inertia[9] = {1.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,1.0};
    CHECK(virtAttachVO(vc, trans->getPhysics()->getMass(),inertia));
   // virtSetPosition(VC, P);
   // virtSetSpeed(VC, S);


}

void virtuose::detachTransform() {
    isAttached = false;
    CHECK(virtDetachVO(vc));
    attached = 0;

}


/**
* takes positiob, speed of attached object and puts it on the virtuose
**/
void virtuose::updateVirtMech() {


    float position[7] = {0.0,0.0,0.0,0.0,0.0,0.0,1.0};
    float speed[6] = {0.0,0.0,0.0,0.0,0.0,0.0};
    float force[6] = {0.0,0.0,0.0,0.0,0.0,0.0};
    int power = 0;
    CHECK(virtIsInShiftPosition(vc,&power));

    if(commandType == COMMAND_TYPE_VIRTMECH)
	{


            if (!isAttached)
            {
                virtGetPosition(vc, position);
                virtSetPosition(vc, position);
                virtGetSpeed(vc, speed);
                virtSetSpeed(vc, speed);

                virtGetArticularPositionOfAdditionalAxe(vc, &gripperPosition);
                virtGetArticularSpeedOfAdditionalAxe(vc, &gripperSpeed);
                virtSetArticularPositionOfAdditionalAxe(vc, &gripperPosition);
                virtSetArticularSpeedOfAdditionalAxe(vc, &gripperSpeed);
            }
            else
            {
                 btTransform pos = this->attached->getPhysics()->getTransform();
                 position[0] = (float) pos.getOrigin().getZ();
                 position[1] = (float) pos.getOrigin().getX();
                 position[2] = (float) pos.getOrigin().getY();
                 pos.setRotation(pos.getRotation().normalized());
                 position[3] = (float) pos.getRotation().getZ();
                 position[4] = (float) pos.getRotation().getX();
                 position[5] = (float) pos.getRotation().getY();
                 position[6] = (float) pos.getRotation().getW();
                 CHECK(virtSetPosition(vc, position));
                 Vec3f vel = this->attached->getPhysics()->getLinearVelocity();
                 speed[0] =(float) vel.z();
                 speed[1] =(float) vel.x();
                 speed[2] =(float) vel.y();
                 vel = this->attached->getPhysics()->getAngularVelocity();
                 speed[3] =(float) vel.z();
                 speed[4] = (float)vel.x();
                 speed[5] =(float) vel.y();
                 CHECK(virtSetSpeed(vc, speed));


                     CHECK(virtGetForce(vc, force));
                     Vec3f frc = Vec3f((float)force[1],(float)force[2],(float)force[0]);
                     frc *= 0.1f;
                     //attached->getPhysics()->addForce(frc);
                     Vec3f trqu = Vec3f((float)force[4],(float)force[5],(float)force[3]);
                     trqu.normalize();
                     trqu *= 0.0000001f;
                     attached->getPhysics()->addTorque(trqu);

            }
	    }


}


OSG_END_NAMESPACE;
