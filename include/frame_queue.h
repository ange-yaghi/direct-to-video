#ifndef ATG_DIRECT_TO_VIDEO_FRAME_QUEUE_H
#define ATG_DIRECT_TO_VIDEO_FRAME_QUEUE_H

#include "frame.h"

#include <mutex>
#include <condition_variable>

namespace atg_dtv {
    class FrameQueue {
        public:
            FrameQueue();
            ~FrameQueue();

            void initialize(int size);
            void destroy();

            Frame *newFrame(int width, int height, int lineWidth, bool wait = false);
            void submitFrame();
            Frame *waitFrame();
            void popFrame();

            void stop();

        protected:
            std::mutex m_lock;
            std::condition_variable m_cv;

            Frame *m_frames;
            int m_capacity;
            int m_length;
            int m_readIndex;

            bool m_stopped;
    };
}  /* namespace atg_dtv */

#endif /* ATG_DIRECT_TO_VIDEO_FRAME_QUEUE_H */
