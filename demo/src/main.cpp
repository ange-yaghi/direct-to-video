#include "../include/dtv.h"

#include <iostream>
#include <chrono>
#include <cmath>

int main() {
    atg_dtv::Encoder encoder;
    atg_dtv::Encoder::VideoSettings settings{};

    // Output filename
    settings.fname = "direct_to_video_sample_output.mp4";

    // Input dimensions
    settings.inputWidth = 2560;
    settings.inputHeight = 1440;

    // Output dimensions
    settings.width = 2560;
    settings.height = 1440;

    // Encoder settings
    settings.hardwareEncoding = true;
    settings.bitRate = 16000000;

    const int VideoLengthSeconds = 10;
    const int FrameCount = VideoLengthSeconds * settings.frameRate;

    auto start = std::chrono::steady_clock::now();

    std::cout << "==============================================\n";
    std::cout << " Direct to Video (DTV) Sample Application\n\n";

    encoder.run(settings, 2);

    for (int i = 0; i < FrameCount; ++i) {
        if ((i + 1) % 100 == 0 || i >= FrameCount - 10) {
            std::cout << "Frame: " << (i + 1) << "/" << FrameCount << "\n";
        }

        const int sin_i = std::lroundf(255 * (0.5 + 0.5 * std::sin(i * 0.01)));

        atg_dtv::Frame *frame = encoder.newFrame(true);
        if (frame == nullptr) break;
        if (encoder.getError() != atg_dtv::Encoder::Error::None) break;

        const int lineWidth = frame->m_lineWidth;
        for (int y = 0; y < settings.inputHeight; ++y) {
            uint8_t *row = &frame->m_rgb[y * lineWidth];
            for (int x = 0; x < settings.inputWidth; ++x) {
                const int index = x * 3;
                row[index + 0] = (x + i) & 0xFF; // r
                row[index + 1] = (y + i) & 0xFF; // g
                row[index + 2] = sin_i & 0xFF;   // b
            }
        }

        encoder.submitFrame();
    }

    encoder.commit();
    encoder.stop();

    auto end = std::chrono::steady_clock::now();

    const double elapsedSeconds =
        std::chrono::duration<double>(end - start).count();

    std::cout << "==============================================\n";
    if (encoder.getError() == atg_dtv::Encoder::Error::None) {      
        std::cout << "Encoding took: " << elapsedSeconds << " seconds" << "\n";
        std::cout << "Real-time framerate: " << FrameCount / elapsedSeconds << " FPS" << "\n";
    }
    else {
        std::cout << "Encoding failed\n";
    }
}
