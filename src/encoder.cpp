#include "../include/dtv/encoder.h"

#include "../include/dtv/ffmpeg.h"

atg_dtv::Encoder::Encoder() {
    m_stopped = true;
    m_error = Error::None;
    m_worker = nullptr;
}

atg_dtv::Encoder::~Encoder() {}

void atg_dtv::Encoder::run(VideoSettings &settings, int bufferSize) {
    std::lock_guard<std::mutex> lk(m_lock);
    if (!m_stopped) { return; }

    m_videoSettings = settings;
    m_stopped = false;
    m_error = Error::None;

    m_queue.initialize(bufferSize);

    setup();

    if (m_error == Error::None) {
        m_worker = new std::thread(&atg_dtv::Encoder::worker, this);
    } else {
        destroy();
    }
}

atg_dtv::Encoder::Error atg_dtv::Encoder::getError() {
    std::lock_guard<std::mutex> lk(m_lock);
    return m_error;
}

void atg_dtv::Encoder::commit() {
    {
        std::lock_guard<std::mutex> lk(m_lock);
        m_stopped = true;
    }

    m_queue.stop();
}

void atg_dtv::Encoder::stop() {
    if (m_worker != nullptr) {
        m_worker->join();

        delete m_worker;
        m_worker = nullptr;
    }

    m_queue.destroy();
}

atg_dtv::Frame *atg_dtv::Encoder::newFrame(bool wait) {
    int64_t audioSamples = 0;
    if (m_videoSettings.audio) {
        const int64_t frameAudioSamples = m_audioStream.tempFrame->nb_samples;
        while (av_compare_ts(m_videoStream.writePts + 1,
                             m_videoStream.codecContext->time_base,
                             m_audioStream.writePts + audioSamples,
                             m_audioStream.codecContext->time_base) > 0) {
            audioSamples += frameAudioSamples;
        }

        m_audioStream.writePts += audioSamples;
    }

    ++m_videoStream.writePts;
    Frame *frame = m_queue.newFrame(m_videoSettings.inputWidth,
                                    m_videoSettings.inputHeight, m_lineWidth,
                                    audioSamples,
                                    m_audioStream.codecContext->channels, wait);
    return frame;
}

void atg_dtv::Encoder::submitFrame() { m_queue.submitFrame(); }

atg_dtv::Encoder::Error addStream(atg_dtv::OutputStream *ost,
                                  AVFormatContext *oc, const AVCodec **codec,
                                  AVCodecID codecId,
                                  atg_dtv::Encoder::VideoSettings &settings) {
    typedef atg_dtv::Encoder::Error Error;

    AVCodecContext *codecContext;

    *codec = nullptr;
    if (*codec == nullptr) { *codec = avcodec_find_encoder(codecId); }
    if (*codec == nullptr) { return Error::CouldNotFindEncoder; }

    if (settings.hardwareEncoding && (*codec)->type == AVMEDIA_TYPE_VIDEO) {
        *codec = avcodec_find_encoder_by_name("h264_nvenc");
    }

    ost->tempPacket = av_packet_alloc();
    if (ost->tempPacket == nullptr) { return Error::CouldNotAllocatePacket; }

    ost->av_stream = avformat_new_stream(oc, nullptr);
    if (ost->av_stream == nullptr) { return Error::CouldNotAllocateStream; }

    ost->av_stream->id = oc->nb_streams - 1;
    codecContext = avcodec_alloc_context3(*codec);

    if (codecContext == nullptr) {
        return Error::CouldNotAllocateEncodingContext;
    }

    ost->codecContext = codecContext;

    if (strcmp((*codec)->name, "libx264") == 0) {
        av_opt_set(ost->codecContext->priv_data, "preset", "ultrafast", 0);
        av_opt_set(ost->codecContext->priv_data, "tune", "zerolatency", 0);
    }

    switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO:
            codecContext->sample_fmt = (*codec)->sample_fmts
                                               ? (*codec)->sample_fmts[0]
                                               : AV_SAMPLE_FMT_FLTP;
            codecContext->bit_rate = 64000;
            codecContext->sample_rate = 44100;
            if ((*codec)->supported_samplerates) {
                codecContext->sample_rate = (*codec)->supported_samplerates[0];
                for (uint64_t i = 0; (*codec)->supported_samplerates[i]; i++) {
                    if ((*codec)->supported_samplerates[i] == 44100)
                        codecContext->sample_rate = 44100;
                }
            }

            codecContext->channels = av_get_channel_layout_nb_channels(
                    codecContext->channel_layout);
            codecContext->channel_layout = AV_CH_LAYOUT_STEREO;
            if ((*codec)->channel_layouts) {
                codecContext->channel_layout = (*codec)->channel_layouts[0];
                for (uint64_t i = 0; (*codec)->channel_layouts[i]; i++) {
                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                        codecContext->channel_layout = AV_CH_LAYOUT_STEREO;
                }
            }

            codecContext->channels = av_get_channel_layout_nb_channels(
                    codecContext->channel_layout);
            ost->av_stream->time_base =
                    AVRational{1, codecContext->sample_rate};
            break;
        case AVMEDIA_TYPE_VIDEO:
            codecContext->codec_id = codecId;
            codecContext->bit_rate = settings.bitRate;
            codecContext->width = settings.width;
            codecContext->height = settings.height;

            ost->av_stream->time_base = AVRational{1, settings.frameRate};
            codecContext->time_base = ost->av_stream->time_base;

            codecContext->gop_size = 12;
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

            if (codecContext->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                codecContext->max_b_frames = 2;
            } else if (codecContext->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                codecContext->mb_decision = 2;
            }
            break;
        default:
            return Error::UnsupportedMediaType;
    }

    if ((oc->oformat->flags & AVFMT_GLOBALHEADER) > 0) {
        ost->codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    return Error::None;
}

void freeStream(atg_dtv::OutputStream *ost) {
    if (ost->codecContext != nullptr) {
        avcodec_free_context(&ost->codecContext);
    }
    if (ost->frame != nullptr) { av_frame_free(&ost->frame); }
    if (ost->tempFrame != nullptr) { av_frame_free(&ost->tempFrame); }
    if (ost->tempPacket != nullptr) { av_packet_free(&ost->tempPacket); }
    if (ost->swsContext != nullptr) { sws_freeContext(ost->swsContext); }
    if (ost->swrContext != nullptr) { swr_free(&ost->swrContext); }
}

AVFrame *allocateAudioFrame(AVSampleFormat sampleFormat, uint64_t channelLayout,
                            int sampleRate, int samples) {
    AVFrame *frame = av_frame_alloc();
    if (frame == nullptr) { return nullptr; }

    frame->format = sampleFormat;
    frame->channel_layout = channelLayout;
    frame->sample_rate = sampleRate;
    frame->nb_samples = samples;

    if (samples > 0) {
        if (av_frame_get_buffer(frame, 0) < 0) { return nullptr; }
    }

    return frame;
}

atg_dtv::Encoder::Error
openAudioStream(AVFormatContext *, const AVCodec *codec,
                atg_dtv::OutputStream *ost,
                atg_dtv::Encoder::VideoSettings &settings) {
    typedef atg_dtv::Encoder::Error Error;

    if (avcodec_open2(ost->codecContext, codec, nullptr) < 0) {
        return Error::CouldNotOpenVideoCodec;
    }

    int nb_samples = 0;
    if ((ost->codecContext->codec->capabilities &
         AV_CODEC_CAP_VARIABLE_FRAME_SIZE) != 0) {
        nb_samples = 10000;
    } else {
        nb_samples = ost->codecContext->frame_size;
    }

    ost->frame = allocateAudioFrame(ost->codecContext->sample_fmt,
                                    ost->codecContext->channel_layout,
                                    ost->codecContext->sample_rate, nb_samples);

    if (ost->frame == nullptr) { return Error::CouldNotAllocateFrame; }

    ost->tempFrame = allocateAudioFrame(
            AV_SAMPLE_FMT_S16, ost->codecContext->channel_layout,
            ost->codecContext->sample_rate, nb_samples);

    if (ost->tempFrame == nullptr) { return Error::CouldNotAllocateFrame; }

    if (avcodec_parameters_from_context(ost->av_stream->codecpar,
                                        ost->codecContext) < 0) {
        return Error::CouldNotCopyStreamParameters;
    }

    ost->swrContext = swr_alloc();
    if (ost->swrContext == nullptr) {
        return Error::CouldNotCreateResamplerContext;
    }

    av_opt_set_int(ost->swrContext, "in_channel_count",
                   ost->codecContext->channels, 0);
    av_opt_set_int(ost->swrContext, "in_sample_rate",
                   ost->codecContext->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swrContext, "in_sample_fmt", AV_SAMPLE_FMT_S16,
                          0);
    av_opt_set_int(ost->swrContext, "out_channel_count",
                   ost->codecContext->channels, 0);
    av_opt_set_int(ost->swrContext, "out_sample_rate",
                   ost->codecContext->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swrContext, "out_sample_fmt",
                          ost->codecContext->sample_fmt, 0);

    if (swr_init(ost->swrContext) < 0) {
        return Error::CouldNotCreateResamplerContext;
    }

    return Error::None;
}

int copyAudioData(atg_dtv::Frame *src,
                  atg_dtv::Encoder::VideoSettings &settings,
                  atg_dtv::OutputStream *ost, int readOffset) {
    const int pixelSize =
            settings.inputAlpha ? (4 * sizeof(uint8_t)) : (3 * sizeof(uint8_t));
    memcpy(ost->tempFrame->data[0], src->m_audio + readOffset,
           size_t(sizeof(uint16_t) * ost->codecContext->channels *
                  ost->tempFrame->nb_samples));

    ost->frame->pts = ost->nextPts;
    ost->nextPts += ost->tempFrame->nb_samples;

    return ost->tempFrame->nb_samples;
}

atg_dtv::Encoder::Error writeAudioFrame(AVFormatContext *oc,
                                        atg_dtv::OutputStream *ost) {
    typedef atg_dtv::Encoder::Error Error;

    AVCodecContext *c = ost->codecContext;
    AVFrame *frame = ost->tempFrame;
    const int dstNbSamples = av_rescale_rnd(
            swr_get_delay(ost->swrContext, c->sample_rate) + frame->nb_samples,
            c->sample_rate, c->sample_rate, AV_ROUND_UP);

    if (av_frame_make_writable(ost->frame) < 0) {
        return Error::CouldNotEncodeFrame;
    }

    if (swr_convert(ost->swrContext, ost->frame->data, dstNbSamples,
                    (const uint8_t **) frame->data, frame->nb_samples) < 0) {
        return Error::CouldNotEncodeFrame;
    }

    ost->frame->pts = av_rescale_q(ost->audioSamples,
                                   AVRational{1, c->sample_rate}, c->time_base);
    ost->audioSamples += dstNbSamples;

    {
        AVCodecContext *codecContext = ost->codecContext;
        if (avcodec_send_frame(codecContext, ost->frame) < 0) {
            return Error::CouldNotSendFrameToEncoder;
        }

        while (true) {
            const int r = avcodec_receive_packet(codecContext, ost->tempPacket);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
                break;
            } else if (r < 0) {
                return Error::CouldNotEncodeFrame;
            }

            av_packet_rescale_ts(ost->tempPacket, codecContext->time_base,
                                 ost->av_stream->time_base);
            ost->tempPacket->stream_index = ost->av_stream->index;

            if (av_interleaved_write_frame(oc, ost->tempPacket) < 0) {
                return Error::CouldNotWriteOutputPacket;
            }
        }
    }

    return Error::None;
}

atg_dtv::Encoder::Error flush(AVFormatContext *oc, atg_dtv::OutputStream *ost) {
    using Error = atg_dtv::Encoder::Error;

    AVCodecContext *codecContext = ost->codecContext;
    if (avcodec_send_frame(codecContext, nullptr) < 0) {
        return Error::CouldNotSendFrameToEncoder;
    }

    while (true) {
        const int r = avcodec_receive_packet(codecContext, ost->tempPacket);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
            break;
        } else if (r < 0) {
            return Error::CouldNotEncodeFrame;
        }

        av_packet_rescale_ts(ost->tempPacket, codecContext->time_base,
                             ost->av_stream->time_base);
        ost->tempPacket->stream_index = ost->av_stream->index;

        if (av_interleaved_write_frame(oc, ost->tempPacket) < 0) {
            return Error::CouldNotWriteOutputPacket;
        }
    }

    return Error::None;
}

AVFrame *allocateVideoFrame(AVPixelFormat pixelFormat, int width, int height) {
    AVFrame *frame = av_frame_alloc();
    if (frame == nullptr) { return nullptr; }

    frame->format = pixelFormat;
    frame->width = width;
    frame->height = height;

    if (av_frame_get_buffer(frame, 0) < 0) { return nullptr; }

    return frame;
}

atg_dtv::Encoder::Error
openVideoStream(AVFormatContext *, const AVCodec *codec,
                atg_dtv::OutputStream *ost,
                atg_dtv::Encoder::VideoSettings &settings) {
    typedef atg_dtv::Encoder::Error Error;

    if (avcodec_open2(ost->codecContext, codec, nullptr) < 0) {
        return Error::CouldNotOpenVideoCodec;
    }

    ost->frame = allocateVideoFrame(ost->codecContext->pix_fmt,
                                    ost->codecContext->width,
                                    ost->codecContext->height);

    if (ost->frame == nullptr) { return Error::CouldNotAllocateFrame; }

    ost->tempFrame = allocateVideoFrame(
            (settings.inputAlpha)
                    ? (settings.bgr ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA)
                    : (settings.bgr ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24),
            settings.inputWidth, settings.inputHeight);

    if (ost->tempFrame == nullptr) { return Error::CouldNotAllocateFrame; }

    if (avcodec_parameters_from_context(ost->av_stream->codecpar,
                                        ost->codecContext) < 0) {
        return Error::CouldNotCopyStreamParameters;
    }

    ost->swsContext = sws_getContext(
            settings.inputWidth, settings.inputHeight,
            (settings.inputAlpha)
                    ? (settings.bgr ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA)
                    : (settings.bgr ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24),
            ost->codecContext->width, ost->codecContext->height,
            ost->codecContext->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);

    if (ost->swsContext == nullptr) {
        return Error::CouldNotCreateConversionContext;
    }

    return Error::None;
}

void copyVideoData(atg_dtv::Frame *src, AVFrame *,
                   atg_dtv::Encoder::VideoSettings &settings,
                   atg_dtv::OutputStream *ost) {
    const int pixelSize =
            settings.inputAlpha ? (4 * sizeof(uint8_t)) : (3 * sizeof(uint8_t));
    memcpy(ost->tempFrame->data[0], src->m_rgb,
           size_t(src->m_width * src->m_height * pixelSize));

    if (ost->swsContext != nullptr) {
        sws_scale(ost->swsContext,
                  (const uint8_t *const *) ost->tempFrame->data,
                  ost->tempFrame->linesize, 0, settings.inputHeight,
                  ost->frame->data, ost->frame->linesize);
    }

    ost->frame->pts = ost->nextPts++;
}

atg_dtv::Encoder::Error writeVideoFrame(AVFormatContext *oc,
                                        AVCodecContext *codecContext,
                                        AVStream *av_stream, AVFrame *frame,
                                        AVPacket *packet) {
    typedef atg_dtv::Encoder::Error Error;

    if (avcodec_send_frame(codecContext, frame) < 0) {
        return Error::CouldNotSendFrameToEncoder;
    }

    while (true) {
        const int r = avcodec_receive_packet(codecContext, packet);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
            break;
        } else if (r < 0) {
            return Error::CouldNotEncodeFrame;
        }

        av_packet_rescale_ts(packet, codecContext->time_base,
                             av_stream->time_base);
        packet->stream_index = av_stream->index;

        if (av_interleaved_write_frame(oc, packet) < 0) {
            return Error::CouldNotWriteOutputPacket;
        }
    }

    return Error::None;
}

void atg_dtv::Encoder::setup() {
    Error err = Error::None;

    avformat_alloc_output_context2(&m_oc, nullptr, nullptr,
                                   m_videoSettings.fname.c_str());

    if (m_oc == nullptr) {
        m_error = Error::CouldNotAllocateOutputContext;
        return;
    }

    m_fmt = m_oc->oformat;

    if (m_fmt->video_codec != AV_CODEC_ID_NONE) {
        addStream(&m_videoStream, m_oc, &m_videoCodec, m_fmt->video_codec,
                  m_videoSettings);
    } else {
        m_error = Error::NotAVideoFormat;
        return;
    }

    if (m_videoSettings.audio) {
        addStream(&m_audioStream, m_oc, &m_audioCodec, m_fmt->audio_codec,
                  m_videoSettings);
    }

    err = openVideoStream(m_oc, m_videoCodec, &m_videoStream, m_videoSettings);
    if (err != Error::None) {
        m_error = err;
        return;
    }

    if (m_videoSettings.audio) {
        err = openAudioStream(m_oc, m_audioCodec, &m_audioStream,
                              m_videoSettings);
        if (err != Error::None) {
            m_error = err;
            return;
        }
    }

    if ((m_fmt->flags & AVFMT_NOFILE) == 0) {
        if (avio_open(&m_oc->pb, m_videoSettings.fname.c_str(),
                      AVIO_FLAG_WRITE) < 0) {
            m_error = Error::CouldNotOpenFile;
            return;
        } else {
            m_openedFile = true;
        }
    }

    if (avformat_write_header(m_oc, nullptr) < 0) {
        m_error = Error::CouldNotWriteHeader;
        return;
    }

    m_lineWidth = m_videoStream.tempFrame->linesize[0];
}

void atg_dtv::Encoder::worker() {
    Error err = Error::None;
    while (true) {
        Frame *frame = m_queue.waitFrame();
        if (frame != nullptr) {
            copyVideoData(frame, m_videoStream.frame, m_videoSettings,
                          &m_videoStream);
            err = writeVideoFrame(m_oc, m_videoStream.codecContext,
                                  m_videoStream.av_stream, m_videoStream.frame,
                                  m_videoStream.tempPacket);

            for (int audioSamples = 0; audioSamples < frame->m_audioSamples;) {
                audioSamples += copyAudioData(frame, m_videoSettings,
                                              &m_audioStream, audioSamples);
                err = writeAudioFrame(m_oc, &m_audioStream);
            }

            m_queue.popFrame();

            if (err != Error::None) { goto end; }
        } else {
            std::lock_guard<std::mutex> lk(m_lock);
            if (m_stopped) { break; }
        }
    }

    flush(m_oc, &m_videoStream);
    flush(m_oc, &m_audioStream);

    av_write_trailer(m_oc);

end:
    std::lock_guard<std::mutex> lk(m_lock);
    m_error = err;
    m_stopped = true;

    m_queue.stop();

    destroy();
}

void atg_dtv::Encoder::destroy() {
    freeStream(&m_videoStream);
    freeStream(&m_audioStream);

    if (m_openedFile) { avio_closep(&m_oc->pb); }
    if (m_oc != nullptr) { avformat_free_context(m_oc); }
}
