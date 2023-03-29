#ifndef ATG_DIRECT_TO_VIDEO_ENCODER_H
#define ATG_DIRECT_TO_VIDEO_ENCODER_H

#include "frame_queue.h"

#include <thread>
#include <mutex>

struct AVStream;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext;
struct AVFormatContext;
struct AVOutputFormat;
struct AVCodec;

namespace atg_dtv {
    struct OutputStream {
        AVStream *av_stream = nullptr;
        AVCodecContext *codecContext = nullptr;

        int64_t nextPts = 0;

        AVFrame *frame = nullptr, *tempFrame = nullptr;
        AVPacket *tempPacket = nullptr;

        SwsContext *swsContext = nullptr;
        SwrContext *swrContext = nullptr;
    };

    class Encoder {
        public:
            struct VideoSettings {
                std::string fname = ""; 
                int width = 1920;
                int height = 1080;
                int inputWidth = 1920;
                int inputHeight = 1080;
                int frameRate = 60;
                int bitRate = 30000000;
                bool hardwareEncoding = true;
                bool inputAlpha = false;
            };

            enum class Error {
                None,
                CouldNotAllocateOutputContext,
                CouldNotDetermineOutputFormat,
                CouldNotFindEncoder,
                CouldNotAllocatePacket,
                CouldNotAllocateStream,
                CouldNotAllocateEncodingContext,
                UnsupportedMediaType,
                NotAVideoFormat,
                CouldNotOpenVideoCodec,
                CouldNotAllocateFrame,
                CouldNotCopyStreamParameters,
                CouldNotOpenFile,
                CouldNotWriteHeader,
                CouldNotCreateConversionContext,
                CouldNotSendFrameToEncoder,
                CouldNotEncodeFrame,
                CouldNotWriteOutputPacket
            };

        public:
            Encoder();
            ~Encoder();

            void run(VideoSettings &settings, int bufferSize);
            void commit();
            void stop();
            Frame *newFrame(bool wait = false);
            void submitFrame();
            Error getError();

        protected:
            void setup();
            void worker();
            void destroy();

        protected:
            std::thread *m_worker;
            std::mutex m_lock;
            Error m_error;

            AVFormatContext *m_oc = nullptr;
            const AVOutputFormat *m_fmt = nullptr;
            const AVCodec *m_videoCodec = nullptr;
            OutputStream m_videoStream;
            bool m_openedFile = false;
            int m_lineWidth = 0;

        protected:
            FrameQueue m_queue;
            VideoSettings m_videoSettings;
            bool m_stopped;
    };
}  /* namespace atg_dtv */

#endif /* ATG_DIRECT_TO_VIDEO_ENCODER_H */
