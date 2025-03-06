#include "../include/dtv/frame.h"

#include <assert.h>

atg_dtv::Frame::Frame() {
    m_rgb = nullptr;
    m_width = m_height = 0;
    m_maxHeight = 0;
    m_maxWidth = 0;
    m_lineWidth = 0;

    m_audio = nullptr;
    m_audioCapacity = 0;
    m_audioSamples = 0;
}

atg_dtv::Frame::~Frame() {
    assert(m_rgb == nullptr);
    assert(m_audio == nullptr);
}
