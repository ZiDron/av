/// Использованные материалы:
///     libav-12.3/doc/examples/avcodec.c
///     http://dranger.com/ffmpeg/tutorial01.c

#include <algorithm>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

int savePpm(AVFrame *pFrame, int width, int height)
{
    const char *filename = "frame.ppm";

    FILE *pFile = fopen(filename, "wb");
    if (!pFile)
        return -101;

    if (fprintf(pFile, "P6\n%d %d\n255\n", width, height) < 0)
        return -102;

    for (int y = 0; y < height; y++)
        fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, 3 * width, pFile) < 0;

    if (!fclose(pFile))
        return -103;
}

int main(int argc, char *argv[])
{
    if (2 != argc) {
        return -1;
    }

    AVFormatContext *pFormatContex = nullptr;
    if (avformat_open_input(&pFormatContex, argv[1], nullptr, nullptr) < 0)
        return -2;

    if (avformat_find_stream_info(pFormatContex, nullptr) < 0)
        return -3;

    auto beginIt = pFormatContex->streams;
    auto endIt = pFormatContex->streams + pFormatContex->nb_streams;
    AVStream **streamIt = std::find_if(beginIt, endIt, [](AVStream *stream) {
        return stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
    });
    if (endIt == streamIt)
        return -4;

    int streamIndex = std::distance(beginIt, streamIt);
    AVCodecParameters *pCodecParametersFile = (*streamIt)->codecpar;

    AVCodec *pCodec = avcodec_find_decoder(pCodecParametersFile->codec_id);
    if (!pCodec) {
        return -5;
    }

    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecContext, pCodecParametersFile) < 0)
        return -6;

    if (avcodec_open2(pCodecContext, pCodec, nullptr) < 0)
        return -7;

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame)
        return -8;

    AVFrame *pFrameRGB = av_frame_alloc();
    if (!pFrameRGB)
        return -8;

    int linesize = 3 * pCodecContext->width;
    int pictureBufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
                                                     pCodecContext->width,
                                                     pCodecContext->height,
                                                     linesize);
    void *pPictureBuffer = av_malloc(pictureBufferSize);
    if (!pPictureBuffer)
        return -8;

    // FIXME: avpicture_fill помечена deprecated
    avpicture_fill(reinterpret_cast<AVPicture *>(pFrameRGB),
                   static_cast<uint8_t *>(pPictureBuffer),
                   AV_PIX_FMT_RGB24,
                   pCodecContext->width,
                   pCodecContext->height);

    SwsContext *pConvertPictureContext = sws_getContext(pCodecContext->width,
                                                        pCodecContext->height,
                                                        pCodecContext->pix_fmt,
                                                        pCodecContext->width,
                                                        pCodecContext->height,
                                                        AV_PIX_FMT_RGB24,
                                                        SWS_BILINEAR,
                                                        nullptr,
                                                        nullptr,
                                                        nullptr);

    AVPacket packet;
    int gotPicture = 0;
    while (!av_read_frame(pFormatContex, &packet) && !gotPicture) {
        if (streamIndex == packet.stream_index) {
            // FIXME: avcodec_decode_video2 помечена deprecated
            if (avcodec_decode_video2(pCodecContext, pFrame, &gotPicture, &packet) < 0)
                return -9;

            if (gotPicture) {
                sws_scale(pConvertPictureContext,
                          pFrame->data,
                          pFrame->linesize,
                          0,
                          pCodecContext->height,
                          pFrameRGB->data,
                          pFrameRGB->linesize);
                if (int code = savePpm(pFrameRGB, pCodecContext->width, pCodecContext->height);
                    code < 0)
                    return code;
            }
        }

        av_packet_unref(&packet);
    }

    if (!gotPicture)
        return -10;

    // TODO: можно автоматизировать с помощью RAII на std::shared_ptr с кастомным Deleter'ом
    sws_freeContext(pConvertPictureContext);
    av_free(pPictureBuffer);
    av_frame_free(&pFrameRGB);
    av_frame_free(&pFrame);
    avcodec_close(pCodecContext);
    avformat_close_input(&pFormatContex);

    return 0;
}
