#pragma once

#include <memory>
#include <thread>

#include <ffmpeg/avcodec>

#include "avframequeue.h"
#include "avpacketqueue.h"

class CodecThread
{
 private:
  /* data */
 public:
  CodecThread(std::shared_ptr<AVPacketQueue> packet_queue,
              std::shared_ptr<AVFrameQueue> frame_queue);
  ~CodecThread();

  int init(const AVCodecParameters *params);

  void start();
  void stop();

  void deinit();

 private:
  void run(std::stop_token token);

 private:
  AVCodecContext *m_codec_ctx{};
  std::jthread m_thread;
  std::shared_ptr<AVPacketQueue> m_packet_queue;
  std::shared_ptr<AVFrameQueue> m_frame_queue;
};
