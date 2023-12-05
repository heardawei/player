#include "audiooutput.h"

#include <spdlog/spdlog.h>

#include "ffmpeg_utils.h"

void fill_audio_pcm(void* userdata, uint8_t* stream, int len)
{
  // 从frame queue读取解码后的PCM的数据，填充到stream，最大填充len长度到stream。
  // 假设len:4000B, 一个frame有6000B, 一次读取了4000B, 这个frame胜了2000B
  AudioOutput* is = reinterpret_cast<AudioOutput*>(userdata);
  int len1 = 0;
  int audio_size = 0;
  while (len > 0)
  {
    SPDLOG_INFO("while len: {} > 0", len);
    if (is->m_audio_buf_index == is->m_audio_buf_size)
    {
      is->m_audio_buf_index = 0;
      auto opt = is->m_queue->pop(std::chrono::milliseconds(10));
      if (opt)
      {
        auto frame = opt.value().get();

        SPDLOG_INFO("  frame: ");
        SPDLOG_INFO("   - format({}) ", frame->format);
        SPDLOG_INFO("   - sample_rate({}) ", frame->sample_rate);
        SPDLOG_INFO("   - channels({}) ", frame->channels);
        SPDLOG_INFO("   - channel_layout({})", frame->channel_layout);

        // 怎么判断是否重采样
        // 1. PCM数据格式和输出格式不一样
        // 2. PCM数据采样率和输出不一样
        // 3. channel layout?
        if (((frame->format != is->m_params.format) ||
             (frame->sample_rate != is->m_params.freq) ||
             (frame->channel_layout != is->m_params.channel_layout)) &&
            // 每次运行只支持一种采样器
            (!is->m_swr_ctx))
        {
          SPDLOG_INFO("    alloc SwrContext");
          is->m_swr_ctx = swr_alloc_set_opts(nullptr,
                                             is->m_params.channel_layout,
                                             is->m_params.format,
                                             is->m_params.freq,
                                             frame->channel_layout,
                                             (AVSampleFormat)frame->format,
                                             frame->sample_rate,
                                             0,
                                             nullptr);
          if (!is->m_swr_ctx)
          {
            SPDLOG_ERROR("swr_alloc_set_opts error");
            return;
          }
          if (const auto ret = swr_init(is->m_swr_ctx); ret < 0)
          {
            SPDLOG_ERROR("swr_init error: {}", Utils::error_stringify(ret));
            swr_free(&is->m_swr_ctx);
            return;
          }
        }

        if (is->m_swr_ctx)
        {  // 重采样
          SPDLOG_INFO("    resampleing");
          int out_samples =
              frame->nb_samples * is->m_params.freq / frame->sample_rate + 256;
          int out_bytes = av_samples_get_buffer_size(nullptr,
                                                     is->m_params.channels,
                                                     out_samples,
                                                     is->m_params.format,
                                                     0);
          if (out_bytes < 0)
          {
            SPDLOG_ERROR("av_samples_get_buffer_size error: {}",
                         Utils::error_stringify(out_bytes));
            return;
          }
          SPDLOG_INFO(
              "      out_samples: {}, out_bytes: {}", out_samples, out_bytes);

          av_fast_malloc(&is->m_audio_buf1, &is->m_audio_buf1_size, out_bytes);
          const uint8_t** in = (const uint8_t**)frame->extended_data;
          uint8_t** out = &is->m_audio_buf1;
          SPDLOG_INFO(
              "      malloc: {} -> {}", out_bytes, is->m_audio_buf1_size);

          const auto len2 = swr_convert(
              is->m_swr_ctx, out, out_samples, in, frame->nb_samples);
          if (len2 < 0)
          {
            SPDLOG_ERROR("swr_convert error: {}", Utils::error_stringify(len2));
            return;
          }
          is->m_audio_buf = is->m_audio_buf1;
          is->m_audio_buf_size = av_samples_get_buffer_size(
              nullptr, is->m_params.channels, len2, is->m_params.format, 0);
          SPDLOG_INFO("      get sample buffer: {}", is->m_audio_buf_size);
        }
        else
        {  // 不重采样
          SPDLOG_INFO("    none resampleing");
          audio_size = av_samples_get_buffer_size(nullptr,
                                                  frame->channels,
                                                  frame->nb_samples,
                                                  (AVSampleFormat)frame->format,
                                                  1);
          SPDLOG_INFO("    get sample buffer: {}", audio_size);

          av_fast_malloc(&is->m_audio_buf1, &is->m_audio_buf1_size, audio_size);
          is->m_audio_buf = is->m_audio_buf1;
          is->m_audio_buf_size = audio_size;
          SPDLOG_INFO(
              "    malloc: {} -> {}", audio_size, is->m_audio_buf1_size);

          memcpy(is->m_audio_buf, frame->data[0], audio_size);
        }
      }
      else
      {
        is->m_audio_buf = nullptr;
        is->m_audio_buf_size = 512;
        SPDLOG_INFO("    pop 0 frame packet");
      }
    }

    len1 = is->m_audio_buf_size - is->m_audio_buf_index;
    if (len1 > len)
    {
      len1 = len;
    }

    if (!is->m_audio_buf)
    {
      memset(stream, 0, len1);
    }
    else
    {
      memcpy(stream, is->m_audio_buf + is->m_audio_buf_index, len1);
    }

    len -= len1;
    stream += len1;
    is->m_audio_buf_index += len1;
  }
}

AudioOutput::AudioOutput(std::shared_ptr<AVFrameQueue> queue)
    : m_queue(queue)
{
}

AudioOutput::~AudioOutput() {}

int AudioOutput::init(const AudioParams& params)
{
  if (const auto ret = SDL_Init(SDL_INIT_AUDIO); ret < 0)
  {
    SPDLOG_ERROR("SDL_Init(SDL_INIT_AUDIO) error: {}", SDL_GetError());
    return ret;
  }

  SDL_AudioSpec wanted_spec, spec;
  wanted_spec.channels = params.channels;
  wanted_spec.freq = params.freq;
  wanted_spec.format = AUDIO_S16SYS;
  wanted_spec.silence = 0;
  wanted_spec.samples = params.frame_size;
  wanted_spec.callback = fill_audio_pcm;
  wanted_spec.userdata = this;

  // AudioParams params;
  if (const auto ret = SDL_OpenAudio(&wanted_spec, &spec); ret < 0)
  {
    SPDLOG_ERROR("SDL_OpenAudio error: {}", SDL_GetError());
    return ret;
  }

  m_params.channels = spec.channels;
  m_params.format = AV_SAMPLE_FMT_S16;
  m_params.freq = spec.freq;
  m_params.channel_layout = av_get_default_channel_layout(spec.channels);
  m_params.frame_size = params.frame_size;
  SDL_PauseAudio(0);

  SPDLOG_INFO("spec: ");
  SPDLOG_INFO(" - format({}) ", static_cast<int>(m_params.format));
  SPDLOG_INFO(" - sample_rate({}) ", m_params.freq);
  SPDLOG_INFO(" - channels({}) ", m_params.channels);
  SPDLOG_INFO(" - channel_layout({})", m_params.channel_layout);

  return 0;
}

void AudioOutput::deinit()
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}
