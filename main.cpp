#include <spdlog/spdlog.h>
#include <filesystem>

#include "ffmpeg/avformat"
#include "ffmpeg/avutil"

#include "demuxthread.h"

int main(int ac, char** av)
{
  SPDLOG_INFO("ffmpeg avutil verion: {}", avutil_version());
  SPDLOG_INFO("ffmpeg avformat verion: {}", avformat_version());

  if (ac != 2)
  {
    SPDLOG_ERROR("please input a video, such as: {} time.mp4", av[0]);
    return -1;
  }

  Demuxthread demux_thrd;
  if (const auto ret = demux_thrd.init(av[1]); ret < 0)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    SPDLOG_ERROR("av_find_best_stream for video error: {}",
                 av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, ret));
    return -1;
  }
  demux_thrd.start();

  using namespace std::chrono_literals;
  std::this_thread::sleep_for(2s);

  demux_thrd.stop();
  demux_thrd.deinit();

  return 0;
}
