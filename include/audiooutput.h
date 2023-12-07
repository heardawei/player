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
  AVChannelLayout channel_layout{};
  AVSampleFormat format{AV_SAMPLE_FMT_NONE};
  int frame_size{};

  AudioParams& operator=(const SDL_AudioSpec& spec)
  {
    av_channel_layout_default(&channel_layout, spec.channels);
    format = AV_SAMPLE_FMT_S16;
    freq = spec.freq;
    frame_size = spec.samples;
    return *this;
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

  int init(const AVCodecParameters& params, AVRational time_base);
  void deinit();

 public:
  std::shared_ptr<AVFrameQueue> m_queue;
  std::shared_ptr<AVSync> m_avsync;
  AVRational m_time_base;
  AudioParams m_params;

 public:
  SwrContext* m_swr_ctx{};
  uint8_t* m_audio_buf{};
  uint32_t m_audio_buf_size{};
  uint32_t m_audio_buf_index{};
  uint8_t* m_audio_buf1{};
  uint32_t m_audio_buf1_size{};
};
