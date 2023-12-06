#pragma once

#include <SDL2/SDL.h>
#include <ffmpeg/avcodec>
#include <ffmpeg/avformat>
#include <ffmpeg/swresample>

#include "avframequeue.h"
#include "avsync.h"

struct AudioParams
{
  int freq{};  // The number of audio samples per second.
  int channels{};
  uint64_t channel_layout{};
  AVSampleFormat format{AV_SAMPLE_FMT_NONE};
  int frame_size{};

  static AudioParams from(const AVCodecParameters& codec_params)
  {
    return {
        codec_params.sample_rate,
        codec_params.channels,
        codec_params.channel_layout,
        static_cast<AVSampleFormat>(codec_params.format),
        codec_params.frame_size,
    };
  }
};

class AudioOutput
{
 private:
  /* data */
 public:
  AudioOutput(std::shared_ptr<AVFrameQueue> queue,
              std::shared_ptr<AVSync> avsync);
  ~AudioOutput();

  int init(const AudioParams& params, AVRational time_base);
  void deinit();

 public:
  std::shared_ptr<AVFrameQueue> m_queue;
  std::shared_ptr<AVSync> m_avsync;
  AVRational m_time_base;
  AudioParams m_params;

 public:
  SwrContext* m_swr_ctx;
  int64_t m_pts{AV_NOPTS_VALUE};
  uint8_t* m_audio_buf{};
  uint32_t m_audio_buf_size{};
  uint32_t m_audio_buf_index{};
  uint8_t* m_audio_buf1{};
  uint32_t m_audio_buf1_size{};
};
