#include "../include/encoder.h"

#include "../include/ffmpeg.h"

atg_dtv::Encoder::Encoder() {
    m_stopped = true;
    m_error = Error::None;
    m_worker = nullptr;
}

atg_dtv::Encoder::~Encoder() {
    /* void */
}

void atg_dtv::Encoder::run(
        VideoSettings &settings,
        int bufferSize)
{
    std::lock_guard<std::mutex> lk(m_lock);
    if (!m_stopped) return;

    m_videoSettings = settings;
    m_stopped = false;
    m_error = Error::None;

    m_queue.initialize(bufferSize);

    setup();

    if (m_error == Error::None) {
        m_worker = new std::thread(&atg_dtv::Encoder::worker, this);
    }
    else {
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
    m_worker->join();

    delete m_worker;
    m_worker = nullptr;

    m_queue.destroy();
}

atg_dtv::Frame *atg_dtv::Encoder::newFrame(bool wait) {
    return m_queue.newFrame(
        m_videoSettings.inputWidth,
        m_videoSettings.inputHeight,
        m_lineWidth,
        wait);
}

void atg_dtv::Encoder::submitFrame() {
    m_queue.submitFrame();
}

atg_dtv::Encoder::Error addStream(
        atg_dtv::OutputStream *ost,
        AVFormatContext *oc,
        const AVCodec **codec,
        AVCodecID codecId,
        atg_dtv::Encoder::VideoSettings &settings)
{
    typedef atg_dtv::Encoder::Error Error;

    AVCodecContext *codecContext;

    *codec = nullptr;
    if (settings.hardwareEncoding) {
        *codec = avcodec_find_encoder_by_name("h264_nvenc");
    }

    if (*codec == nullptr) {
        *codec = avcodec_find_encoder(codecId);
    }

    if (*codec == nullptr) {
        return Error::CouldNotFindEncoder;
    }

    ost->tempPacket = av_packet_alloc();
    if (ost->tempPacket == nullptr) {
        return Error::CouldNotAllocatePacket;
    }

    ost->av_stream = avformat_new_stream(oc, nullptr);
    if (ost->av_stream == nullptr) {
        return Error::CouldNotAllocateStream;
    }

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
            return Error::UnsupportedMediaType;
        case AVMEDIA_TYPE_VIDEO:
            codecContext->codec_id = codecId;
            codecContext->bit_rate = settings.bitRate;
            codecContext->width = settings.width;
            codecContext->height = settings.height;

            ost->av_stream->time_base = { 1, settings.frameRate };
            codecContext->time_base = ost->av_stream->time_base;

            codecContext->gop_size = 12;
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

            if (codecContext->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                codecContext->max_b_frames = 2;
            }
            else if (codecContext->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
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
    if (ost->codecContext != nullptr) avcodec_free_context(&ost->codecContext);
    if (ost->frame != nullptr) av_frame_free(&ost->frame);
    if (ost->tempFrame != nullptr) av_frame_free(&ost->tempFrame);
    if (ost->tempPacket != nullptr) av_packet_free(&ost->tempPacket);
    if (ost->swsContext != nullptr) sws_freeContext(ost->swsContext);
    if (ost->swrContext != nullptr) swr_free(&ost->swrContext);
}

AVFrame *allocateFrame(
        AVPixelFormat pixelFormat,
        int width,
        int height)
{
    AVFrame *frame = av_frame_alloc();
    if (frame == nullptr) return nullptr;

    frame->format = pixelFormat;
    frame->width = width;
    frame->height = height;

    if (av_frame_get_buffer(frame, 0) < 0) {
        return nullptr;
    }

    return frame;
}

atg_dtv::Encoder::Error openVideo(
        AVFormatContext *oc,
        const AVCodec *codec,
        atg_dtv::OutputStream *ost,
        atg_dtv::Encoder::VideoSettings &settings)
{
    typedef atg_dtv::Encoder::Error Error;

    if (avcodec_open2(ost->codecContext, codec, nullptr) < 0) {
        return Error::CouldNotOpenVideoCodec;
    }

    ost->frame = allocateFrame(
            ost->codecContext->pix_fmt,
            ost->codecContext->width,
            ost->codecContext->height);

    if (ost->frame == nullptr) {
        return Error::CouldNotAllocateFrame;
    }

    ost->tempFrame = allocateFrame(
            (settings.inputAlpha)
                ? AV_PIX_FMT_RGBA
                : AV_PIX_FMT_RGB24,
            settings.inputWidth,
            settings.inputHeight);

    if (ost->tempFrame == nullptr) {
        return Error::CouldNotAllocateFrame;
    }

    if (avcodec_parameters_from_context(
                ost->av_stream->codecpar,
                ost->codecContext) < 0)
    {
        return Error::CouldNotCopyStreamParameters;
    }

    ost->swsContext = sws_getContext(
            settings.inputWidth, settings.inputHeight,
            (settings.inputAlpha)
                ? AV_PIX_FMT_RGBA
                : AV_PIX_FMT_RGB24,
            ost->codecContext->width, ost->codecContext->height,
            ost->codecContext->pix_fmt,
            SWS_BICUBIC,
            nullptr,
            nullptr,
            nullptr);

    if (ost->swsContext == nullptr) {
        return Error::CouldNotCreateConversionContext;
    }

    return Error::None;
}

void generateFrame(
        atg_dtv::Frame *src,
        AVFrame *dest,
        atg_dtv::Encoder::VideoSettings &settings,
        atg_dtv::OutputStream *ost)
{
    AVFrame *target = ost->tempFrame;

    const int pixelSize = settings.inputAlpha
        ? 4 * sizeof(uint8_t)
        : 3 * sizeof(uint8_t);
    memcpy(
        target->data[0],
        src->m_rgb,
        (size_t)src->m_width * src->m_height * pixelSize);

    if (ost->swsContext != nullptr) {
        sws_scale(
                ost->swsContext,
                (const uint8_t *const *)ost->tempFrame->data,
                ost->tempFrame->linesize,
                0,
                settings.inputHeight,
                ost->frame->data,
                ost->frame->linesize);
    }

    ost->frame->pts = ost->nextPts++;
}

atg_dtv::Encoder::Error writeFrame(
        AVFormatContext *oc,
        AVCodecContext *codecContext,
        AVStream *av_stream,
        AVFrame *frame,
        AVPacket *packet)
{
    typedef atg_dtv::Encoder::Error Error;

    if (avcodec_send_frame(codecContext, frame) < 0) {
        return Error::CouldNotSendFrameToEncoder;
    }

    while (true) {
        const int r = avcodec_receive_packet(codecContext, packet);
        if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
        else if (r < 0) {
            return Error::CouldNotEncodeFrame;
        }

        av_packet_rescale_ts(packet, codecContext->time_base, av_stream->time_base);
        packet->stream_index = av_stream->index;

        if (av_interleaved_write_frame(oc, packet) < 0) {
            return Error::CouldNotWriteOutputPacket;
        }
    }

    return Error::None;
}

void atg_dtv::Encoder::setup() {
    Error err = Error::None;

    avformat_alloc_output_context2(
        &m_oc,
        nullptr,
        nullptr,
        m_videoSettings.fname.c_str());

    if (m_oc == nullptr) {
        m_error = Error::CouldNotAllocateOutputContext;
        return;
    }

    m_fmt = m_oc->oformat;

    if (m_fmt->video_codec != AV_CODEC_ID_NONE) {
        addStream(&m_videoStream, m_oc, &m_videoCodec, m_fmt->video_codec, m_videoSettings);
    }
    else {
        m_error = Error::NotAVideoFormat;
        return;
    }

    err = openVideo(m_oc, m_videoCodec, &m_videoStream, m_videoSettings);
    if (err != Error::None) {
        m_error = err;
        return;
    }

    if ((m_fmt->flags & AVFMT_NOFILE) == 0) {
        if (avio_open(&m_oc->pb, m_videoSettings.fname.c_str(), AVIO_FLAG_WRITE) < 0) {
            m_error = Error::CouldNotOpenFile;
            return;
        }
        else {
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
            generateFrame(frame, m_videoStream.frame, m_videoSettings, &m_videoStream);
            err = writeFrame(
                    m_oc,
                    m_videoStream.codecContext,
                    m_videoStream.av_stream,
                    m_videoStream.frame,
                    m_videoStream.tempPacket);

            m_queue.popFrame();

            if (err != Error::None) goto end;
        }
        else {
            std::lock_guard<std::mutex> lk(m_lock);
            if (m_stopped) break;
        }
    }

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

    if (m_openedFile) {
        avio_closep(&m_oc->pb);
    }

    if (m_oc != nullptr) avformat_free_context(m_oc);
}
