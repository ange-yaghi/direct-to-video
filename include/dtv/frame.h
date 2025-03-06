#ifndef ATG_DIRECT_TO_VIDEO_FRAME_H
#define ATG_DIRECT_TO_VIDEO_FRAME_H

#include <cinttypes>

namespace atg_dtv {
class Frame {
public:
    Frame();
    ~Frame();

    uint8_t *m_rgb;
    int m_width, m_height;
    int m_maxWidth, m_maxHeight;
    int m_lineWidth;

    int16_t *m_audio;
    int m_audioCapacity;
    int m_audioSamples;
};
} /* namespace atg_dtv */

#endif /* ATG_DIRECT_TO_VIDEO_FRAME_H */
