#include "demuxthread.h"

#include <libavformat/avformat.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include "avpacketqueue.h"

Demuxthread::Demuxthread(std::shared_ptr<AVPacketQueue> audio_packet_queue,
                         std::shared_ptr<AVPacketQueue> video_packet_queue)
    : m_format_ctx(avformat_alloc_context())
    , m_audio_packet_queue(audio_packet_queue)
    , m_video_packet_queue(video_packet_queue)
{
}

Demuxthread::~Demuxthread() { avformat_free_context(m_format_ctx); }

int Demuxthread::init(std::string_view url)
{
  if (const auto ret =
          avformat_open_input(&m_format_ctx, url.data(), nullptr, nullptr);
      ret < 0)
  {
    return ret;
  }

  if (const auto ret = avformat_find_stream_info(m_format_ctx, nullptr);
      ret < 0)
  {
    return ret;
  }

  av_dump_format(m_format_ctx, 0, url.data(), 0);

  if (const auto ret = av_find_best_stream(
          m_format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
      ret < 0)
  {
    return ret;
  }
  else
  {
    m_audio_stream_idx = ret;
  }
  SPDLOG_INFO("audio stream index: {}", m_audio_stream_idx);

  if (const auto ret = av_find_best_stream(
          m_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
      ret < 0)
  {
    return ret;
  }
  else
  {
    m_video_stream_idx = ret;
  }
  SPDLOG_INFO("video stream index: {}", m_video_stream_idx);

  return 0;
}

void Demuxthread::start()
{
  m_thread = std::jthread([=](std::stop_token token) { run(token); });
}

void Demuxthread::stop()
{
  m_thread.request_stop();
  m_thread.join();
}

void Demuxthread::deinit() { avformat_close_input(&m_format_ctx); }

const AVCodecParameters *Demuxthread::audio_codec_params() const
{
  if (m_audio_stream_idx)
  {
    return m_format_ctx->streams[*m_audio_stream_idx]->codecpar;
  }
  return nullptr;
}

const AVCodecParameters *Demuxthread::video_codec_params() const
{
  if (m_video_stream_idx)
  {
    return m_format_ctx->streams[*m_video_stream_idx]->codecpar;
  }
  return nullptr;
}

void Demuxthread::run(std::stop_token token)
{
  while (!token.stop_requested())
  {
    auto pkt = std::shared_ptr<AVPacket>(
        av_packet_alloc(), [](AVPacket *pkt) { av_packet_free(&pkt); });
    if (const auto ret = av_read_frame(m_format_ctx, pkt.get()); ret < 0)
    {
      if (ret == AVERROR_EOF)
      {
        SPDLOG_INFO("read finished");
        break;
      }
      else
      {
        SPDLOG_ERROR("av_read_frame error: {}", error_stringify(ret));
        break;
      }
    }

    if (m_audio_stream_idx && pkt->stream_index == *m_audio_stream_idx)
    {
      m_audio_packet_queue->push(pkt);
    }
    else if (m_video_stream_idx && pkt->stream_index == *m_video_stream_idx)
    {
      m_video_packet_queue->push(pkt);
    }
    else
    {
      av_packet_unref(pkt.get());
    }
  }
}

std::string Demuxthread::error_stringify(int error)
{
  char estr[AV_ERROR_MAX_STRING_SIZE]{};
  return av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, error);
}
