#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include "ffmpeg/avformat"
#include "ffmpeg/avutil"

#include "codecthread.h"
#include "demuxthread.h"
#include "lockedqueue.h"

namespace test
{
void locked_queue_test()
{
  LockedQueue<int> q;
  std::jthread producers[1];
  std::jthread consumers[3];

  using namespace std::chrono_literals;

  for (int n = 0; auto& producer : producers)
  {
    producer = std::jthread(
        [n, &q]()
        {
          for (int v = 0; v < 10; v++)
          {
            SPDLOG_INFO("producer {} push: {}", n, v);
            q.push(v);
            std::this_thread::sleep_for(1s);
          }
        });
    n++;
  }

  for (int n = 0; auto& consumer : consumers)
  {
    consumer = std::jthread(
        [n, &q]()
        {
          while (true)
          {
            SPDLOG_INFO("consumer {} pop: {}", n, q.pop());
          }
        });
    n++;
  }
}
}  // namespace test

int main(int ac, char** av)
{
  SPDLOG_INFO("ffmpeg avutil verion: {}", avutil_version());
  SPDLOG_INFO("ffmpeg avformat verion: {}", avformat_version());
  SPDLOG_INFO("ffmpeg avcodec verion: {}", avcodec_version());

  if (ac != 2)
  {
    SPDLOG_ERROR("please input a video, such as: {} time.mp4", av[0]);
    return -1;
  }

  auto audio_packet_queue = std::make_shared<AVPacketQueue>();
  auto video_packet_queue = std::make_shared<AVPacketQueue>();

  auto audio_frame_queue = std::make_shared<AVFrameQueue>();
  auto video_frame_queue = std::make_shared<AVFrameQueue>();

  auto demux_thread =
      std::make_shared<Demuxthread>(audio_packet_queue, video_packet_queue);
  auto audio_decode_thread =
      std::make_shared<CodecThread>(audio_packet_queue, audio_frame_queue);
  auto video_decode_thread =
      std::make_shared<CodecThread>(video_packet_queue, video_frame_queue);

  if (const auto ret = demux_thread->init(av[1]); ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("demux_thread init error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return ret;
  }

  if (const auto ret =
          audio_decode_thread->init(demux_thread->audio_codec_params());
      ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("audio_decode_thread init error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return ret;
  }

  if (const auto ret =
          video_decode_thread->init(demux_thread->video_codec_params());
      ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("video_decode_thread init error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return ret;
  }

  demux_thread->start();
  audio_decode_thread->start();
  video_decode_thread->start();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(10s);

  video_decode_thread->stop();
  audio_decode_thread->stop();
  demux_thread->stop();

  video_decode_thread->deinit();
  audio_decode_thread->deinit();
  demux_thread->deinit();

  SPDLOG_INFO("audio frames: {}, video frames: {}",
              audio_frame_queue->size(),
              video_frame_queue->size());

  return 0;
}
