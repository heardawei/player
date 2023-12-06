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

  static AudioParams from(const SDL_AudioSpec& spec)
  {
    AudioParams params;
    av_channel_layout_default(&params.channel_layout, spec.channels);
    params.format = AV_SAMPLE_FMT_S16;
    params.freq = spec.freq;
    params.frame_size = spec.samples;
    return params;
  }

  static AudioParams from(const AVCodecParameters& codec_params)
  {
    AudioParams audio_params;
    audio_params.channel_layout = codec_params.ch_layout;
    audio_params.format = (enum AVSampleFormat)codec_params.format;
    audio_params.freq = codec_params.sample_rate;
    audio_params.frame_size = codec_params.frame_size;
    return audio_params;
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
  SwrContext* m_swr_ctx{};
  int64_t m_pts{AV_NOPTS_VALUE};
  uint8_t* m_audio_buf{};
  uint32_t m_audio_buf_size{};
  uint32_t m_audio_buf_index{};
  uint8_t* m_audio_buf1{};
  uint32_t m_audio_buf1_size{};
};
