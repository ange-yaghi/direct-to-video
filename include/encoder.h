#ifndef ATG_DIRECT_TO_VIDEO_ENCODER_H
#define ATG_DIRECT_TO_VIDEO_ENCODER_H

#include "frame_queue.h"

#include <thread>
#include <mutex>

namespace atg_dtv {
    class Encoder {
        public:
            struct VideoSettings {
                std::string fname = "";
                bool hardwareEncoding = true;
                int width = 1920;
                int height = 1080;
                int inputWidth = 1920;
                int inputHeight = 1080;
                int frameRate = 60;
                int bitRate = 30000000;
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
            void worker();

        protected:
            std::thread *m_worker;
            std::mutex m_lock;
            Error m_error;

        protected:
            FrameQueue m_queue;
            VideoSettings m_videoSettings;
            bool m_stopped;
    };
}  /* namespace atg_dtv */

#endif /* ATG_DIRECT_TO_VIDEO_ENCODER_H */
