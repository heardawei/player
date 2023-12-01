#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include "ffmpeg/avformat"
#include "ffmpeg/avutil"

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

  if (ac != 2)
  {
    SPDLOG_ERROR("please input a video, such as: {} time.mp4", av[0]);
    return -1;
  }

  auto audio_pkt_queue = std::make_shared<AVPacketQueue>();
  auto video_pkt_queue = std::make_shared<AVPacketQueue>();
  auto demux_thrd =
      std::make_shared<Demuxthread>(audio_pkt_queue, video_pkt_queue);
  if (const auto ret = demux_thrd->init(av[1]); ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("av_find_best_stream for video error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return -1;
  }
  demux_thrd->start();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);

  demux_thrd->stop();
  demux_thrd->deinit();

  return 0;
}