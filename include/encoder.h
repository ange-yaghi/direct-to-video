#ifndef ATG_DIRECT_TO_VIDEO_ENCODER_H
#define ATG_DIRECT_TO_VIDEO_ENCODER_H

#include "frame_queue.h"

#include <thread>
#include <mutex>

namespace atg_dtv {
    class Encoder {
        public:
            struct VideoSettings {
                std::string fname;
                int width;
                int height;
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

            FrameQueue *run(VideoSettings &settings, int bufferSize);
            void commit();
            void stop();
            Frame *newFrame(bool wait = false);
            void submitFrame();

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
