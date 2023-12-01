#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <thread>

#include <ffmpeg/avcodec>
#include <ffmpeg/avformat>

#include "avpacketqueue.h"

class Demuxthread
{
 private:
  /* data */
 public:
  Demuxthread(std::shared_ptr<AVPacketQueue> audio_packet_queue,
              std::shared_ptr<AVPacketQueue> video_packet_queue);
  ~Demuxthread();

  int init(std::string_view url);

  void start();
  void stop();

  void deinit();

  const AVCodecParameters *audio_codec_params() const;
  const AVCodecParameters *video_codec_params() const;

 private:
  void run(std::stop_token token);
  std::string error_stringify(int error);

 private:
  AVFormatContext *m_format_ctx{};
  std::optional<int> m_audio_stream_idx{};
  std::optional<int> m_video_stream_idx{};
  std::jthread m_thread;
  std::shared_ptr<AVPacketQueue> m_audio_packet_queue;
  std::shared_ptr<AVPacketQueue> m_video_packet_queue;
};
