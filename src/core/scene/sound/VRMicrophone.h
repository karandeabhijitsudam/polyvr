#ifndef VRMICROPHONE_H_INCLUDED
#define VRMICROPHONE_H_INCLUDED

#include <OpenSG/OSGConfig.h>
#include <list>
#include "VRSoundFwd.h"

struct ALCdevice_struct;
namespace std { struct thread; }

using namespace std;
OSG_BEGIN_NAMESPACE;

class VRMutex;

class VRMicrophone : public std::enable_shared_from_this<VRMicrophone> {
	private:
        int sample_size = 1024;
        int sample_rate = 22050;
        bool doRecord = false;
        bool doStream = false;
        bool needsFlushing = false;
        bool streamPaused = false;

        thread* recordingThread = 0;
        thread* streamingThread = 0;
	    ALCdevice_struct* device = 0;
	    VRSoundPtr recording;

	    VRSoundBufferPtr frame;
	    list<VRSoundBufferPtr> frameBuffer;
	    VRMutex* streamMutex = 0;
	    int queueSize = 10;
	    int streamBuffer = 5;
	    int queuedFrames = 0; // frames recorded but not streamed yet
	    int queuedStream = 0; // frames streamed at the beginning

	    void setup();
	    void start();
	    void stop();

	public:
		VRMicrophone();
		~VRMicrophone();

		static VRMicrophonePtr create();
		VRMicrophonePtr ptr();

		void startRecording();
		VRSoundPtr stopRecording();

		void startStreaming(string address, int port);
		void pauseStreaming(bool p);
		void stopStreaming();
};

OSG_END_NAMESPACE;

#endif //VRMICROPHONE_H_INCLUDED