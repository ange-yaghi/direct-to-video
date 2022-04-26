#include "../include/frame.h"

#include <assert.h>

atg_dtv::Frame::Frame() {
    m_rgb = nullptr;
    m_width = m_height = 0;
    m_maxHeight = 0;
    m_maxWidth = 0;
    m_lineWidth = 0;
}

atg_dtv::Frame::~Frame() {
    assert(m_rgb == nullptr);
}
