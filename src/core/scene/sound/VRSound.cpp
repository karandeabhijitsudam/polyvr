#include "VRSound.h"
#include "VRSoundUtils.h"
#include "core/scene/VRScene.h"
#include "core/math/path.h"
#include "core/math/fft.h"
#include "core/utils/toString.h"
#include "core/utils/VRFunction.h"
#include "VRSoundManager.h"

#ifndef WITHOUT_GTK
#include "core/gui/VRGuiManager.h"
#include "core/gui/VRGuiConsole.h"
#endif

extern "C" {
#include <libavresample/avresample.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <fstream>
#include <map>
#include <climits>

#define WARN(x) \
VRConsoleWidget::get( "Errors" )->write( x+"\n" );

using namespace OSG;

string avErrToStr(const int& e) {
    char buf[100];
    int N = 100;
    av_strerror(e, buf, N);
    return string(buf);
}

struct VRSound::ALData {
    ALenum sample = 0;
    ALenum format = 0;
    ALenum layout = 0;
    ALenum state = AL_INITIAL;
    AVFormatContext* context = 0;
    AVAudioResampleContext* resampler = 0;
    AVCodecContext* codec = NULL;
    AVPacket packet;
    AVFrame* frame;
};

VRSound::VRSound() {
    pos = new Vec3d();
    vel = new Vec3d();

    VRSoundManager::get(); // this may init channel
    al = shared_ptr<ALData>( new ALData() );
    reset();
}

VRSound::~VRSound() {
    close();
    delete pos;
    delete vel;
}

VRSoundPtr VRSound::create() { return VRSoundPtr( new VRSound() ); }

int VRSound::getState() { return al->state; }
string VRSound::getPath() { return path; }
void VRSound::setPath( string p ) { path = p; }

void VRSound::setLoop(bool loop) { this->loop = loop; doUpdate = true; }
void VRSound::setPitch(float pitch) { this->pitch = pitch; doUpdate = true; }
void VRSound::setVolume(float gain) { this->gain = gain; doUpdate = true; }
void VRSound::setUser(Vec3d p, Vec3d v) { *pos = p; *vel = v; doUpdate = true; }
void VRSound::setCallback(VRUpdateCbPtr cb) { callback = cb; }

bool VRSound::isRunning() {
    if (interface) interface->recycleBuffer();
    //cout << "isRunning " << bool(al->state == AL_PLAYING) << " " << bool(al->state == AL_INITIAL) << " " << getQueuedBuffer()<< endl;
    return al->state == AL_PLAYING || al->state == AL_INITIAL || interface && interface->getQueuedBuffer() != 0;
}
void VRSound::stop() { interrupt = true; loop = false; }

void VRSound::pause() {
    if (interface) interface->pause();
}

void VRSound::resume() {
    if (interface) interface->play();
}

void VRSound::close() {
    cout << " !!! VRSound::close !!!" << endl;
    stop();
    interface = 0;
    if (al->context) avformat_close_input(&al->context);
    if (al->resampler) avresample_free(&al->resampler);
    al->context = 0;
    al->resampler = 0;
    init = 0;
    cout << "  VRSound::close done" << endl;
}

void VRSound::reset() { al->state = AL_STOPPED; }
void VRSound::play() { al->state = AL_INITIAL; }

void VRSound::updateSampleAndFormat() {
    if (al->codec->channel_layout == 0) {
        if (al->codec->channels == 1) al->codec->channel_layout = AV_CH_LAYOUT_MONO;
        if (al->codec->channels == 2) al->codec->channel_layout = AV_CH_LAYOUT_STEREO;
        if (al->codec->channel_layout == 0) cout << "WARNING! channel_layout is 0.\n";
    }

    frequency = al->codec->sample_rate;
    al->format = AL_FORMAT_MONO16;
    AVSampleFormat sfmt = al->codec->sample_fmt;

    if (sfmt == AV_SAMPLE_FMT_NONE) cout << "ERROR: unsupported format: none\n";

    if (sfmt == AV_SAMPLE_FMT_U8) al->sample = AL_UNSIGNED_BYTE_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S16) al->sample = AL_SHORT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S32) al->sample = AL_INT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_FLT) al->sample = AL_FLOAT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_DBL) al->sample = AL_DOUBLE_SOFT;

    if (sfmt == AV_SAMPLE_FMT_U8P) al->sample = AL_UNSIGNED_BYTE_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S16P) al->sample = AL_SHORT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_S32P) al->sample = AL_INT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_FLTP) al->sample = AL_FLOAT_SOFT;
    if (sfmt == AV_SAMPLE_FMT_DBLP) al->sample = AL_DOUBLE_SOFT;

    if (al->codec->channel_layout == AV_CH_LAYOUT_MONO) al->layout = AL_MONO_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_STEREO) al->layout = AL_STEREO_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_QUAD) al->layout = AL_QUAD_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_5POINT1) al->layout = AL_5POINT1_SOFT;
    if (al->codec->channel_layout == AV_CH_LAYOUT_7POINT1) al->layout = AL_7POINT1_SOFT;

    switch(al->sample) {
        case AL_UNSIGNED_BYTE_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = AL_FORMAT_MONO8; break;
                case AL_STEREO_SOFT:  al->format = AL_FORMAT_STEREO8; break;
                case AL_QUAD_SOFT:    al->format = alGetEnumValue("AL_FORMAT_QUAD8"); break;
                case AL_5POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_51CHN8"); break;
                case AL_7POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_71CHN8"); break;
                default: cout << "OpenAL unsupported format 8\n"; break;
            } break;
        case AL_SHORT_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = AL_FORMAT_MONO16; break;
                case AL_STEREO_SOFT:  al->format = AL_FORMAT_STEREO16; break;
                case AL_QUAD_SOFT:    al->format = alGetEnumValue("AL_FORMAT_QUAD16"); break;
                case AL_5POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_51CHN16"); break;
                case AL_7POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_71CHN16"); break;
                default: cout << "OpenAL unsupported format 16\n"; break;
            } break;
        case AL_FLOAT_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32"); break;
                case AL_STEREO_SOFT:  al->format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32"); break;
                case AL_QUAD_SOFT:    al->format = alGetEnumValue("AL_FORMAT_QUAD32"); break;
                case AL_5POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_51CHN32"); break;
                case AL_7POINT1_SOFT: al->format = alGetEnumValue("AL_FORMAT_71CHN32"); break;
                default: cout << "OpenAL unsupported format 32\n"; break;
            } break;
        case AL_DOUBLE_SOFT:
            switch(al->layout) {
                case AL_MONO_SOFT:    al->format = alGetEnumValue("AL_FORMAT_MONO_DOUBLE"); break;
                case AL_STEREO_SOFT:  al->format = alGetEnumValue("AL_FORMAT_STEREO_DOUBLE"); break;
                default: cout << "OpenAL unsupported format 64\n"; break;
            } break;
        default: cout << "OpenAL unsupported format";
    }

    if (av_sample_fmt_is_planar(al->codec->sample_fmt)) {
        int out_sample_fmt;
        switch(al->codec->sample_fmt) {
            case AV_SAMPLE_FMT_U8P:  out_sample_fmt = AV_SAMPLE_FMT_U8; break;
            case AV_SAMPLE_FMT_S16P: out_sample_fmt = AV_SAMPLE_FMT_S16; break;
            case AV_SAMPLE_FMT_S32P: out_sample_fmt = AV_SAMPLE_FMT_S32; break;
            case AV_SAMPLE_FMT_DBLP: out_sample_fmt = AV_SAMPLE_FMT_DBL; break;
            case AV_SAMPLE_FMT_FLTP:
            default: out_sample_fmt = AV_SAMPLE_FMT_FLT;
        }

        al->resampler = avresample_alloc_context();
        av_opt_set_int(al->resampler, "in_channel_layout",  al->codec->channel_layout, 0);
        av_opt_set_int(al->resampler, "in_sample_fmt",      al->codec->sample_fmt,     0);
        av_opt_set_int(al->resampler, "in_sample_rate",     al->codec->sample_rate,    0);
        av_opt_set_int(al->resampler, "out_channel_layout", al->codec->channel_layout, 0);
        av_opt_set_int(al->resampler, "out_sample_fmt",     out_sample_fmt,        0);
        av_opt_set_int(al->resampler, "out_sample_rate",    al->codec->sample_rate,    0);
        avresample_open(al->resampler);
    }
}

bool VRSound::initiate() {
    cout << "init sound " << path << endl;
    interface = VRSoundInterface::create();
    initiated = true;

    if (path == "") return 1;

    auto e = avformat_open_input(&al->context, path.c_str(), NULL, NULL);
    if (e < 0) {
        string warning = "ERROR! avformat_open_input of path '"+path+"' failed: " + avErrToStr(e);
        cout << warning << endl;
        WARN(warning);
        return 0;
    }

    if (auto e = avformat_find_stream_info(al->context, NULL)) if (e < 0) { cout << "ERROR! avformat_find_stream_info failed: " << avErrToStr(e) << endl; return 0; }
    av_dump_format(al->context, 0, path.c_str(), 0);

    stream_id = av_find_best_stream(al->context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (stream_id == -1) return 0;

    al->codec = al->context->streams[stream_id]->codec;
    AVCodec* avcodec = avcodec_find_decoder(al->codec->codec_id);
    if (avcodec == 0) return 0;
    if (avcodec_open2(al->codec, avcodec, NULL) < 0) return 0;

    nextBuffer = 0;

    updateSampleAndFormat();
    return true;
}

void VRSound::initWithCodec(AVCodecContext* codec) {
    al->state = AL_STOPPED;
    al->resampler = 0;
    al->codec = codec;

    updateSampleAndFormat();

#ifdef OLD_LIBAV
        al->frame = avcodec_alloc_frame(); // Allocate frame
#else
        al->frame = av_frame_alloc(); // Allocate frame
#endif

    interface = VRSoundInterface::create();
    initiated = true;
}

void VRSound::playBuffer(VRSoundBufferPtr frame) { interface->queueFrame(frame); }
void VRSound::addBuffer(VRSoundBufferPtr frame) { ownedBuffer.push_back(frame); }

void VRSound::queuePacket(AVPacket* packet) {
    for (auto frame : extractPacket(packet)) {
        if (interrupt) { cout << "interrupt sound\n"; break; }
        interface->queueFrame(frame);
    }
}

vector<VRSoundBufferPtr> VRSound::extractPacket(AVPacket* packet) {
    vector<VRSoundBufferPtr> res;
    //cout << "VRSound::queuePacket, alIsSource1: " << bool(alIsSource(source) == AL_TRUE) << endl;
    while (packet->size > 0) { // Decodes audio data from `packet` into the frame
        if (interrupt) { cout << "interrupt sound\n"; break; }

        int finishedFrame = 0;
        int len = avcodec_decode_audio4(al->codec, al->frame, &finishedFrame, packet);
        if (len < 0) { cout << "decoding error\n"; break; }

        if (finishedFrame) {
            if (interrupt) { cout << "interrupt sound\n"; break; }

            // Decoded data is now available in frame->data[0]
            int linesize;
            int data_size = av_samples_get_buffer_size(&linesize, al->codec->channels, al->frame->nb_samples, al->codec->sample_fmt, 0);

            ALbyte* frameData;
            if (al->resampler != 0) {
                frameData = (ALbyte *)av_malloc(data_size*sizeof(uint8_t));
                avresample_convert( al->resampler, (uint8_t **)&frameData, linesize, al->frame->nb_samples, (uint8_t **)al->frame->data, al->frame->linesize[0], al->frame->nb_samples);
            } else frameData = (ALbyte*)al->frame->data[0];

            auto frame = VRSoundBuffer::wrap(frameData, data_size, frequency, al->format);
            res.push_back(frame);
        }

        //There may be more than one frame of audio data inside the packet.
        packet->size -= len;
        packet->data += len;
    } // while packet.size > 0
    return res;
}

void VRSound::playFrame() {
    //cout << "VRSound::playFrame " << interrupt << " " << this << " playing: " << (al->state == AL_PLAYING) << " N buffer: " << getQueuedBuffer() << endl;

    bool internal = (ownedBuffer.size() > 0);

    if (al->state == AL_INITIAL) {
        cout << "playFrame AL_INITIAL" << endl;
        if (!initiated) initiate();
        if (internal) { al->state = AL_PLAYING; interrupt = false; return; }
        if (!al->context) { /*cout << "VRSound::playFrame Warning: no context" << endl;*/ return; }
#ifdef OLD_LIBAV
        al->frame = avcodec_alloc_frame(); // Allocate frame
#else
        al->frame = av_frame_alloc(); // Allocate frame
#endif
        av_seek_frame(al->context, stream_id, 0,  AVSEEK_FLAG_FRAME);
        al->state = AL_PLAYING;
        interrupt = false;
    }

    if (al->state == AL_PLAYING) {
        if (interface->getQueuedBuffer() > 5) {
            interface->recycleBuffer();
            return;
        }

        if (doUpdate) interface->updateSource(pitch, gain);

        bool endReached = false;

        if (!internal) {
            auto avrf = av_read_frame(al->context, &al->packet);
            if (interrupt || avrf < 0) { // End of stream, done decoding
                if (al->packet.data) av_packet_unref(&al->packet);
                av_free(al->frame);
                endReached = true;
            } else {
                if (al->packet.stream_index != stream_id) { cout << "skip non audio\n"; return; } // Skip non audio packets
                queuePacket(&al->packet);
            }
        } else {
            if (nextBuffer < ownedBuffer.size()) {
                interface->queueFrame(ownedBuffer[nextBuffer]);
                nextBuffer++;
            } else endReached = true;
        }

        if (endReached) {
            al->state = loop ? AL_INITIAL : AL_STOPPED;
            cout << "endReached, stop? " << (al->state == AL_STOPPED) << endl;
            if (al->state == AL_STOPPED && callback) {
                auto scene = VRScene::getCurrent();
                if (scene) scene->queueJob(callback);
            }
            return;
        }
    } // while more packets exist inside container.
}



struct OutputStream {
    int64_t next_pts;
    AVStream *st;
    AVCodecContext *enc;
    AVFrame *frame;
    AVFrame *tmp_frame;
    AVAudioResampleContext *avr;
};

void add_audio_stream(OutputStream *ost, AVFormatContext *oc, enum AVCodecID codec_id) {
    AVCodec* codec = avcodec_find_encoder(codec_id);
    if (!codec) { fprintf(stderr, "codec not found\n"); exit(1); }

    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) { fprintf(stderr, "Could not alloc stream\n"); exit(1); }

    AVCodecContext* c = avcodec_alloc_context3(codec);
    if (!c) { fprintf(stderr, "Could not alloc an encoding context\n"); exit(1); }
    ost->enc = c;

    /* put sample parameters */
    c->sample_fmt     = codec->sample_fmts           ? codec->sample_fmts[0]           : AV_SAMPLE_FMT_S16;
    c->sample_rate    = codec->supported_samplerates ? codec->supported_samplerates[0] : 44100;
    c->channel_layout = codec->channel_layouts       ? codec->channel_layouts[0]       : AV_CH_LAYOUT_STEREO;
    c->channels       = av_get_channel_layout_nb_channels(c->channel_layout);
    c->bit_rate       = 64000;

    ost->st->time_base = (AVRational){ 1, c->sample_rate };

    // some formats want stream headers to be separate
    if (oc->oformat->flags & AVFMT_GLOBALHEADER) c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    /* initialize sample format conversion;
     * to simplify the code, we always pass the data through lavr, even
     * if the encoder supports the generated format directly -- the price is
     * some extra data copying;
     */
    ost->avr = avresample_alloc_context();
    if (!ost->avr) { fprintf(stderr, "Error allocating the resampling context\n"); exit(1); }

    av_opt_set_int(ost->avr, "in_sample_fmt",      AV_SAMPLE_FMT_S16,   0);
    av_opt_set_int(ost->avr, "in_sample_rate",     22050,               0);
    av_opt_set_int(ost->avr, "in_channel_layout",  AV_CH_LAYOUT_MONO,   0);
    av_opt_set_int(ost->avr, "out_sample_fmt",     c->sample_fmt,       0);
    av_opt_set_int(ost->avr, "out_sample_rate",    c->sample_rate,      0);
    av_opt_set_int(ost->avr, "out_channel_layout", c->channel_layout,   0);

    int ret = avresample_open(ost->avr);
    if (ret < 0) { fprintf(stderr, "Error opening the resampling context\n"); exit(1); }
}

AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) { fprintf(stderr, "Error allocating an audio frame\n"); exit(1); }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples) {
        int ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) { fprintf(stderr, "Error allocating an audio buffer\n"); exit(1); }
    }

    return frame;
}

void open_audio(AVFormatContext *oc, OutputStream *ost) {
    int nb_samples, ret;

    AVCodecContext* c = ost->enc;
    if (avcodec_open2(c, NULL, NULL) < 0) { fprintf(stderr, "could not open codec\n"); exit(1); }

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) nb_samples = 10000;
    else nb_samples = c->frame_size;

    cout << "Allocate audio frames: " << nb_samples << endl;

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, AV_CH_LAYOUT_MONO, 22050, nb_samples);

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) { fprintf(stderr, "Could not copy the stream parameters\n"); exit(1); }
}

AVFrame* get_audio_frame(OutputStream *ost, VRSoundBufferPtr buffer) {
    AVFrame* frame = ost->tmp_frame;
    int16_t* src = (int16_t*)buffer->data;
    int16_t* dst = (int16_t*)frame->data[0];

    frame->nb_samples = buffer->size*0.5;
    for (int j = 0; j < frame->nb_samples; j++) {
        int v = src[j]; // audio data
        for (int i = 0; i < ost->enc->channels; i++) *dst++ = v;
    }

    return frame;
}

int encode_audio_frame(AVFormatContext *oc, OutputStream *ost, AVFrame *frame) {
    AVPacket pkt = { 0 }; // data and size must be 0;
    int got_packet;

    av_init_packet(&pkt);
    avcodec_encode_audio2(ost->enc, &pkt, frame, &got_packet);

    if (got_packet) {
        pkt.stream_index = ost->st->index;

        av_packet_rescale_ts(&pkt, ost->enc->time_base, ost->st->time_base);

        /* Write the compressed frame to the media file. */
        if (av_interleaved_write_frame(oc, &pkt) != 0) {
            fprintf(stderr, "Error while writing audio frame\n");
            exit(1);
        }
    }

    return (frame || got_packet) ? 0 : 1;
}

void write_buffer(AVFormatContext *oc, OutputStream *ost, VRSoundBufferPtr buffer) {
    AVFrame *frame;
    int ret;

    frame = get_audio_frame(ost, buffer);
    ret = avresample_convert(ost->avr, NULL, 0, 0, frame->extended_data, frame->linesize[0], frame->nb_samples);
    if (ret < 0) {
        fprintf(stderr, "Error feeding audio data to the resampler\n");
        exit(1);
    }

    while ((frame && avresample_available(ost->avr) >= ost->frame->nb_samples) ||
           (!frame && avresample_get_out_samples(ost->avr, 0))) {
        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0) exit(1);

        /* the difference between the two avresample calls here is that the
         * first one just reads the already converted data that is buffered in
         * the lavr output buffer, while the second one also flushes the
         * resampler */
        if (frame) {
            ret = avresample_read(ost->avr, ost->frame->extended_data, ost->frame->nb_samples);
        } else {
            ret = avresample_convert(ost->avr, ost->frame->extended_data, ost->frame->linesize[0], ost->frame->nb_samples, NULL, 0, 0);
        }

        if (ret < 0) {
            fprintf(stderr, "Error while resampling\n");
            exit(1);
        } else if (frame && ret != ost->frame->nb_samples) {
            fprintf(stderr, "Too few samples returned from lavr\n");
            exit(1);
        }

        ost->frame->nb_samples = ret;

        ost->frame->pts        = ost->next_pts;
        ost->next_pts         += ost->frame->nb_samples;

        encode_audio_frame(oc, ost, ret ? ost->frame : NULL);
    }
}

void close_stream(AVFormatContext *oc, OutputStream *ost) {
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    avresample_free(&ost->avr);
}

void VRSound::exportToFile(string path) {
    OutputStream audio_st = { 0 };
    const char *filename = path.c_str();
    av_register_all();

    AVOutputFormat* fmt = av_guess_format(NULL, filename, NULL);
    if (!fmt) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        fmt = av_guess_format("mpeg", NULL, NULL);
    }
    if (!fmt) { fprintf(stderr, "Could not find suitable output format\n"); return; }

    AVFormatContext* oc = avformat_alloc_context();
    if (!oc) { fprintf(stderr, "Memory error\n"); return; }

    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

    if (fmt->audio_codec != AV_CODEC_ID_NONE) add_audio_stream(&audio_st, oc, fmt->audio_codec);

    open_audio(oc, &audio_st);
    av_dump_format(oc, 0, filename, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&oc->pb, filename, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", filename);
            return;
        }
    }

    /* Write the stream header, if any. */
    avformat_write_header(oc, NULL);
    for (auto buffer : ownedBuffer) write_buffer(oc, &audio_st, buffer);
    av_write_trailer(oc);

    // cleanup
    close_stream(oc, &audio_st);
    if (!(fmt->flags & AVFMT_NOFILE)) avio_close(oc->pb);
    avformat_free_context(oc);
    return;
}


// carrier amplitude, carrier frequency, carrier phase, modulation amplitude, modulation frequency, modulation phase, packet duration
void VRSound::synthesize(float Ac, float wc, float pc, float Am, float wm, float pm, float duration) {
    if (!initiated) initiate();

    int sample_rate = 22050;
    size_t buf_size = duration * sample_rate;
    buf_size += buf_size%2;
    vector<short> samples(buf_size);

    double tmp = 0;
    for(uint i=0; i<buf_size; i++) {
        double t = i*2*Pi/sample_rate + synth_t0;
        samples[i] = Ac * sin( wc*t + pc + Am*sin(wm*t + pm) );
        tmp = t;
    }
    synth_t0 = tmp;

    auto frame = VRSoundBuffer::wrap((ALbyte*)&samples[0], samples.size()*sizeof(short), sample_rate, AL_FORMAT_MONO16);
    playBuffer(frame);
}

vector<short> VRSound::synthSpectrum(vector<double> spectrum, uint sample_rate, float duration, float fade_factor, bool returnBuffer, int maxQueued) {
    if (!initiated) initiate();

    if (maxQueued >= 0) {
        interface->recycleBuffer();
        if ( interface->getQueuedBuffer() >= maxQueued ) return vector<short>();
    }

    /* --- fade in/out curve ---
    ::path c;
    c.addPoint(Vec3d(0,0,0), Vec3d(1,0,0));
    c.addPoint(Vec3d(1,1,0), Vec3d(1,0,0));
    c.compute(sample_rate);
    */

    size_t buf_size = duration * sample_rate;
    uint fade = min(fade_factor * sample_rate, duration * sample_rate); // number of samples to fade at beginning and end

    // transform spectrum back to time domain using fftw3
    FFT fft;
    vector<double> out = fft.transform(spectrum, sample_rate);

    vector<short> samples(buf_size);
    for(uint i=0; i<buf_size; ++i) {
        samples[i] = 0.5 * SHRT_MAX * out[i]; // for fftw normalization
    }

    auto calcFade = [](double& t) {
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        // P3*t³ + P2*3*t²*(1-t) + P1*3*t*(1-t)² + P0*(1-t)³
        // P0(0,0) P1(0.5,0) P2(0.5,1) P3(1,1)
        // P0(0,0) P1(0.5,0.1) P2(0.5,1) P3(1,1)
        double s = 1-t;
        return t*t*(3*s + t);
    };

    for (uint i=0; i < fade; ++i) {
        double t = double(i)/(fade-1);
        double y = calcFade(t);
        samples[i] *= y;
        samples[buf_size-i-1] *= y;
    }

    auto frame = VRSoundBuffer::wrap((ALbyte*)&samples[0], samples.size()*sizeof(short), sample_rate, AL_FORMAT_MONO16);
    playBuffer(frame);
    return returnBuffer ? samples : vector<short>();
}

vector<short> VRSound::synthBuffer(vector<Vec2d> freqs1, vector<Vec2d> freqs2, float duration) {
    if (!initiated) initiate();
    // play sound
    int sample_rate = 22050;
    size_t buf_size = duration * sample_rate;
    vector<short> samples(buf_size);
    double Ni = 1.0/freqs1.size();
    double T = 2*Pi/sample_rate;
    static map<int,complex<double>> phasors;
    for (uint i=0; i<buf_size; i++) {
        double k = double(i)/(buf_size-1);
        samples[i] = 0;
        for (uint j=0; j<freqs1.size(); j++) {
            double A = freqs1[j][1]*(1.0-k) + freqs2[j][1]*k;
            double f = freqs1[j][0]*(1.0-k) + freqs2[j][0]*k;

            if (!phasors.count(j)) phasors[j] = complex<double>(1,0);
            phasors[j] *= exp( complex<double>(0,T*f) );
            samples[i] += A*Ni*phasors[j].imag();
        }
    }
    auto frame = VRSoundBuffer::wrap((ALbyte*)&samples[0], samples.size()*sizeof(short), sample_rate, AL_FORMAT_MONO16);
    playBuffer(frame);
    if (true) return samples;
    return vector<short>();
}

vector<short> VRSound::synthBufferForChannel(vector<Vec2d> freqs1, vector<Vec2d> freqs2, int channel, float duration) {
    /*
        this creates synthetic sound samples based on two vectors of frequencies
        the channel is required for separation of the different static phasors
        the duration determines how long these samples will be

        this is based on the work done in synthBuffer
    */

    if (!initiated) initiate();
    // play sound
    int sample_rate = 22050;
    size_t buf_size = duration * sample_rate;
    vector<short> samples(buf_size);
    double Ni = 1.0/freqs1.size();
    double T = 2*Pi/sample_rate;
    static map<int,map<int,complex<double>>> phasors;

    if (!phasors.count(channel)) {
        phasors[channel] = map<int,complex<double>>();
    }

    for (uint i=0; i<buf_size; i++) {
        double k = double(i)/(buf_size-1);
        samples[i] = 0;
        for (uint j=0; j<freqs1.size(); j++) {
            double A = freqs1[j][1]*(1.0-k) + freqs2[j][1]*k;
            double f = freqs1[j][0]*(1.0-k) + freqs2[j][0]*k;

            if (!phasors[channel].count(j)) {
                phasors[channel][j] = complex<double>(1,0);
            }
            phasors[channel][j] *= exp( complex<double>(0,T*f) );
            samples[i] += A*Ni*phasors[channel][j].imag();
        }
    }
    return samples;
}

void VRSound::synthBufferOnChannels(vector<vector<Vec2d>> freqs1, vector<vector<Vec2d>> freqs2, float duration) {
    /*
        this expects two vectors of input vectors consisting of <frequency, amplitude> tuples
        the vectors should contain a vector with input data for every channel
        the duration determines how long the generated sound samples will be played on the audio buffer
    */
    auto num_channels = freqs1.size();
    if (num_channels != freqs2.size()) {
        cout << "synthBufferOnChannels - sizes don't match - freqs1 = " << num_channels << " freqs2 = " << freqs2.size() << endl;
        return;
    }

    // vector to generate all synth buffers into
    vector<vector<short>> synth_buffer(num_channels);

    // generate a new buffer with size for all buffers * amount of channels
    int sample_rate = 22050;
    size_t buffer_size = duration * sample_rate;
    vector<short> buffer(buffer_size * num_channels);

    // generate synth buffers for every channel and store them for later use
    for (uint channel = 0; channel < num_channels; channel++) {
        synth_buffer[channel] = synthBufferForChannel(freqs1[channel], freqs2[channel], channel, duration);
    }

    /*
      create one big buffer composed of every synth buffer
      elements have to be in the order of the real audio channels
      element 0 is the first frame on the first channel
      element 1 the first frame on the second channel
      element 8 is the second frame on the first channel
    */
    for (uint i = 0; i < buffer_size; i++) {
        for (uint channel = 0; channel < num_channels; channel++) {
            buffer[num_channels * i + channel] = synth_buffer[channel][i];
        }
    }

    // try to determine the amount of channels and their respective mapping inside OpenAL
    // play mono as default because that should output sound on every channel
    ALenum format;
    switch (num_channels) {
        case 1:
            format = AL_FORMAT_MONO16;
            break;
        case 2:
            format = AL_FORMAT_STEREO16;
            break;
        case 6:
            format = AL_FORMAT_51CHN16;
            break;
        case 8:
            format = AL_FORMAT_71CHN16;
            break;
        default:
            format = AL_FORMAT_MONO16;
    }

    auto frame = VRSoundBuffer::wrap((ALbyte*)&buffer[0], buffer.size()*sizeof(short), sample_rate, format);
    playBuffer(frame);
}




