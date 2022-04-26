#include "../include/frame_queue.h"

#include <assert.h>

atg_dtv::FrameQueue::FrameQueue() {
    m_frames = nullptr;
    m_capacity = 0;
    m_length = 0;
    m_readIndex = 0;
    m_stopped = false;
}

atg_dtv::FrameQueue::~FrameQueue() {
    assert(m_frames == nullptr);
}

void atg_dtv::FrameQueue::initialize(int size) {
    assert(m_frames == nullptr);

    m_capacity = size;
    m_length = 0;
    m_readIndex = 0;

    m_frames = new Frame[m_capacity];
}

void atg_dtv::FrameQueue::destroy() {
    if (m_frames == nullptr) return;

    for (int i = 0; i < m_capacity; ++i) {
        delete[] m_frames[i].m_rgb;
        m_frames[i].m_rgb = nullptr;
    }

    delete[] m_frames;

    m_frames = nullptr;
    m_capacity = 0;
    m_length = 0;
    m_readIndex = 0;
}

atg_dtv::Frame *atg_dtv::FrameQueue::newFrame(int width, int height, int lineWidth, bool wait) {
    std::unique_lock<std::mutex> lk(m_lock);

    if (wait) {
        m_cv.wait(lk, [this]{ return m_length < m_capacity || m_stopped; });
    }
    
    if (m_length == m_capacity) return nullptr;

    Frame &f = m_frames[(m_readIndex + m_length) % m_capacity];
    if (f.m_maxHeight < height || f.m_maxWidth < width) {
        delete[] f.m_rgb;
        f.m_rgb = nullptr;
    }

    if (f.m_rgb == nullptr) {
        f.m_rgb = new uint8_t[(size_t)height * lineWidth];
        f.m_maxWidth = width;
        f.m_maxHeight = height;
        f.m_lineWidth = lineWidth;
    }

    f.m_width = width;
    f.m_height = height;

    lk.unlock();

    return &f;
}

void atg_dtv::FrameQueue::submitFrame() {
    std::unique_lock<std::mutex> lk(m_lock);

    ++m_length;

    lk.unlock();
    m_cv.notify_one();
}

atg_dtv::Frame *atg_dtv::FrameQueue::waitFrame() {
    std::unique_lock<std::mutex> lk(m_lock);
    m_cv.wait(lk, [this]{ return this->m_length > 0 || (m_stopped && this->m_length == 0); });

    if (this->m_length == 0) return nullptr;

    Frame &f = m_frames[m_readIndex];

    lk.unlock();
    return &f;
}

void atg_dtv::FrameQueue::popFrame() {
    std::unique_lock<std::mutex> lk(m_lock);

    assert(m_length > 0);

    --m_length;
    m_readIndex = (m_readIndex + 1) % m_capacity;

    lk.unlock();
    m_cv.notify_one();
}

void atg_dtv::FrameQueue::stop() {
    std::unique_lock<std::mutex> lk(m_lock);

    m_stopped = true;

    lk.unlock();
    m_cv.notify_all();
}
