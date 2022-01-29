#include "../include/dtv.h"

int main() {
    atg_dtv::Encoder encoder;
    atg_dtv::Encoder::VideoSettings settings{};
    settings.fname = "test.mp4";
    settings.width = 1920;
    settings.height = 1080;

    atg_dtv::FrameQueue *q = encoder.run(settings, 16);
    
    for (int i = 0; i < 120; ++i) {
        atg_dtv::Frame *f = encoder.newFrame(true);
        
        for (int y = 0; y < 1080; ++y) {
            for (int x = 0; x < 1920; ++x) {
                f->m_rgb[(y * 1920 + x) * 3 + 0] = x % 255;
                f->m_rgb[(y * 1920 + x) * 3 + 1] = y % 255;
                f->m_rgb[(y * 1920 + x) * 3 + 2] = i % 255;
            }
        }

        encoder.submitFrame();
    }

    encoder.commit();
    encoder.stop();
}
