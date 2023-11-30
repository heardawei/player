#include "demuxthread.h"

#include <libavformat/avformat.h>
#include <spdlog/spdlog.h>

Demuxthread::Demuxthread(/* args */)
    : m_avfmt_ctx(avformat_alloc_context())
{
}

Demuxthread::~Demuxthread() { avformat_free_context(m_avfmt_ctx); }

int Demuxthread::init(std::string_view url)
{
  if (const auto ret =
          avformat_open_input(&m_avfmt_ctx, url.data(), nullptr, nullptr);
      ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("avformat_open_input error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return ret;
  }

  if (const auto ret = avformat_find_stream_info(m_avfmt_ctx, nullptr); ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("avformat_find_stream_info error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return ret;
  }

  av_dump_format(m_avfmt_ctx, 0, url.data(), 0);

  if (const auto ret = av_find_best_stream(
          m_avfmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
      ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("av_find_best_stream for audio error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
  }
  else
  {
    m_audio_stream_idx = ret;
    SPDLOG_INFO("audio stream index: {}", *m_audio_stream_idx);
  }

  if (const auto ret = av_find_best_stream(
          m_avfmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
      ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("av_find_best_stream for video error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
  }
  else
  {
    m_video_stream_idx = ret;
    SPDLOG_INFO("video stream index: {}", *m_video_stream_idx);
  }

  return 0;
}

void Demuxthread::start()
{
  m_thrd = std::jthread([=](std::stop_token token) { run(token); });
}

void Demuxthread::stop()
{
  m_thrd.request_stop();
  m_thrd.join();
}

void Demuxthread::deinit() { avformat_close_input(&m_avfmt_ctx); }

void Demuxthread::run(std::stop_token token)
{
  AVPacket packet;
  while (!token.stop_requested())
  {
    if (const auto ret = av_read_frame(m_avfmt_ctx, &packet); ret < 0)
    {
      if (ret == AVERROR_EOF)
      {
        SPDLOG_INFO("end of file");
        break;
      }
      else
      {
        char estr[AV_ERROR_MAX_STRING_SIZE]{};
        SPDLOG_ERROR("av_read_frame error: {}",
                     av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
        break;
      }
    }
    if (m_audio_stream_idx && packet.stream_index == *m_audio_stream_idx)
    {
      SPDLOG_INFO("audio packet");
    }
    else if (m_video_stream_idx && packet.stream_index == *m_video_stream_idx)
    {
      SPDLOG_INFO("video packet");
    }
    else
    {
      SPDLOG_INFO("other packet");
    }
    av_packet_unref(&packet);
  }
}
