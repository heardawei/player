#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <thread>

#include <ffmpeg/avformat>

class Demuxthread
{
 private:
  /* data */
 public:
  Demuxthread(/* args */);
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
};
