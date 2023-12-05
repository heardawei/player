#pragma once

#include <SDL2/SDL.h>
#include <ffmpeg/avcodec>
#include <ffmpeg/avformat>
#include <ffmpeg/swresample>

#include "avframequeue.h"

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
  AudioOutput(std::shared_ptr<AVFrameQueue> queue);
  ~AudioOutput();

  int init(const AudioParams& params);
  void deinit();

 public:
  std::shared_ptr<AVFrameQueue> m_queue;
  AudioParams m_params;

 public:
  SwrContext* m_swr_ctx;
  uint8_t* m_audio_buf{};
  uint32_t m_audio_buf_size{};
  uint32_t m_audio_buf_index{};
  uint8_t* m_audio_buf1{};
  uint32_t m_audio_buf1_size{};
};
