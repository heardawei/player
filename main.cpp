#include <chrono>
#include <format>
#include <memory>

#include <spdlog/fmt/std.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "ffmpeg/avformat"
#include "ffmpeg/avutil"

#include "audiooutput.h"
#include "codecthread.h"
#include "demuxthread.h"
#include "ffmpeg_utils.h"
#include "lockedqueue.h"
#include "videooutput.h"

#undef main

namespace
{
void init_logging()
{
  const auto& localzone_time = std::chrono::zoned_time(
      std::chrono::current_zone(), std::chrono::system_clock::now());
  const auto& filename = std::format("{:%Y%m%d_%H%M%S}.log", localzone_time);
  auto file_sink =
      std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
  file_sink->set_level(spdlog::level::level_enum::trace);
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::level_enum::debug);
  auto logger = std::make_shared<spdlog::logger>(
      "default", spdlog::sinks_init_list{file_sink, console_sink});
  logger->set_level(spdlog::level::level_enum::trace);
  spdlog::set_default_logger(logger);
  spdlog::flush_every(std::chrono::seconds(1));
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%!] [%t] %v");
}
}  // namespace

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
  init_logging();

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
  auto avsync = std::make_shared<AVSync>();

  auto demux_thread =
      std::make_shared<Demuxthread>(audio_packet_queue, video_packet_queue);
  auto audio_decode_thread =
      std::make_shared<CodecThread>(audio_packet_queue, audio_frame_queue);
  auto video_decode_thread =
      std::make_shared<CodecThread>(video_packet_queue, video_frame_queue);
  auto audio_output = std::make_shared<AudioOutput>(audio_frame_queue, avsync);
  auto video_output = std::make_shared<VideoOutput>(video_frame_queue, avsync);

  if (const auto ret = demux_thread->init(av[1]); ret < 0)
  {
    SPDLOG_ERROR("demux_thread init error: {}", Utils::error_stringify(ret));
    return ret;
  }

  if (const auto ret =
          audio_decode_thread->init(demux_thread->audio_codec_params());
      ret < 0)
  {
    SPDLOG_ERROR("audio_decode_thread init error: {}",
                 Utils::error_stringify(ret));
    return ret;
  }

  if (const auto ret =
          video_decode_thread->init(demux_thread->video_codec_params());
      ret < 0)
  {
    SPDLOG_ERROR("video_decode_thread init error: {}",
                 Utils::error_stringify(ret));
    return ret;
  }

  if (const auto ret = audio_output->init(
          AudioParams::from(*demux_thread->audio_codec_params()),
          demux_thread->audio_stream_time_base());
      ret < 0)
  {
    SPDLOG_ERROR("audio_output init error");
    return ret;
  }

  if (const auto ret =
          video_output->init(demux_thread->video_codec_params()->width,
                             demux_thread->video_codec_params()->height,
                             demux_thread->video_stream_time_base());
      ret < 0)
  {
    SPDLOG_ERROR("video_output init error");
    return ret;
  }

  demux_thread->start();
  audio_decode_thread->start();
  video_decode_thread->start();

  video_output->main_loop();

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
