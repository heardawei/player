#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <thread>

#include <ffmpeg/avformat>

#include "avpacketqueue.h"

class Demuxthread
{
 private:
  /* data */
 public:
  Demuxthread(std::shared_ptr<AVPacketQueue> audio_pkt_queue,
              std::shared_ptr<AVPacketQueue> video_pkt_queue);
  ~Demuxthread();

  int init(std::string_view url);

  void start();
  void stop();

  void deinit();

 private:
  void run(std::stop_token token);

 private:
  AVFormatContext *m_avfmt_ctx{};
  std::optional<int> m_audio_stream_idx{};
  std::optional<int> m_video_stream_idx{};
  std::jthread m_thrd;
  std::shared_ptr<AVPacketQueue> m_audio_pkt_queue;
  std::shared_ptr<AVPacketQueue> m_video_pkt_queue;
};
