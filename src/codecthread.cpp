#include "codecthread.h"

#include "ffmpeg_utils.h"

CodecThread::CodecThread(std::shared_ptr<AVPacketQueue> packet_queue,
                         std::shared_ptr<AVFrameQueue> frame_queue)
    : m_codec_ctx(avcodec_alloc_context3(nullptr))
    , m_packet_queue(packet_queue)
    , m_frame_queue(frame_queue)
{
}

CodecThread::~CodecThread() { avcodec_free_context(&m_codec_ctx); }

int CodecThread::init(const AVCodecParameters *params)
{
  assert(params);
  if (const auto ret = avcodec_parameters_to_context(m_codec_ctx, params);
      ret < 0)
  {
    return ret;
  }

  auto decoder = avcodec_find_decoder(m_codec_ctx->codec_id);
  if (!decoder)
  {
    return AVERROR_DECODER_NOT_FOUND;
  }

  if (const auto ret = avcodec_open2(m_codec_ctx, decoder, nullptr); ret < 0)
  {
    return ret;
  }

  return 0;
}

void CodecThread::start()
{
  m_thread = std::jthread([=](std::stop_token token) { run(token); });
}

void CodecThread::stop()
{
  m_thread.request_stop();
  m_thread.join();
}

void CodecThread::deinit() { avcodec_close(m_codec_ctx); }

void CodecThread::run(std::stop_token token)
{
  int packets = 0;
  int frames = 0;
  while (!token.stop_requested())
  {
    auto opt = m_packet_queue->pop(std::chrono::milliseconds(10));
    if (!opt)
    {
      continue;
    }

    auto pkt = *opt;
    if (const auto ret = avcodec_send_packet(m_codec_ctx, pkt.get()); ret < 0)
    {
      SPDLOG_ERROR("avcodec_send_packet error: {}", Utils::error_stringify(ret));
      return;
    }

    packets++;

    while (!token.stop_requested())
    {
      auto frame = std::shared_ptr<AVFrame>(
          av_frame_alloc(), [](AVFrame *f) { av_frame_free(&f); });
      if (const auto ret = avcodec_receive_frame(m_codec_ctx, frame.get());
          ret == 0)
      {
        m_frame_queue->push(frame);
        frames++;
        continue;
      }
      else if (ret == AVERROR(EAGAIN))
      {
        break;
      }
      else
      {
        SPDLOG_ERROR("avcodec_receive_frame error: {}", Utils::error_stringify(ret));
        return;
      }
    }
  }
  SPDLOG_INFO("decode {} packets -> {} frames", packets, frames);
}
