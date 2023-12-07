#include "audiooutput.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include "ffmpeg_utils.h"

namespace
{
constexpr auto MS_PER_S = std::chrono::milliseconds::period::den /
                          std::chrono::milliseconds::period::num;
}  // namespace

void fill_audio_pcm(void* userdata, uint8_t* stream, int len)
{
  // 从frame queue读取解码后的PCM的数据，填充到stream，最大填充len长度到stream。
  // 假设len:4000B, 一个frame有6000B, 一次读取了4000B, 这个frame胜了2000B
  AudioOutput* is = reinterpret_cast<AudioOutput*>(userdata);
  int len1{};
  int audio_size{};
  std::optional<int64_t> opt_pts;

  while (len > 0)
  {
    SPDLOG_TRACE("while len: {} > 0", len);
    if (is->m_audio_buf_index == is->m_audio_buf_size)
    {
      is->m_audio_buf_index = 0;
      SPDLOG_TRACE("audio frame queue size: {}", is->m_queue->size());
      auto opt = is->m_queue->pop(std::chrono::milliseconds(10));
      if (opt)
      {
        auto frame = opt.value().get();

        SPDLOG_TRACE("  frame: ");
        SPDLOG_TRACE("   - format({}) ", frame->format);
        SPDLOG_TRACE("   - sample_rate({}) ", frame->sample_rate);
        SPDLOG_TRACE("   - channels({}) ", frame->ch_layout.nb_channels);

        opt_pts = frame->pts;

        // 怎么判断是否重采样
        // 1. PCM数据格式和输出格式不一样
        // 2. PCM数据采样率和输出不一样
        // 3. channel layout?
        // 4. 每次运行只支持一种采样器
        if (((frame->format != is->m_params.format) ||
             (frame->sample_rate != is->m_params.freq) ||
             av_channel_layout_compare(&frame->ch_layout,
                                       &is->m_params.channel_layout)) &&
            (!is->m_swr_ctx))
        {
          SPDLOG_TRACE("    alloc SwrContext");

          if (const auto ret =
                  swr_alloc_set_opts2(&is->m_swr_ctx,
                                      &is->m_params.channel_layout,
                                      is->m_params.format,
                                      is->m_params.freq,
                                      &frame->ch_layout,
                                      (AVSampleFormat)frame->format,
                                      frame->sample_rate,
                                      0,
                                      nullptr);
              ret < 0)
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
          SPDLOG_TRACE("    resampleing");
          const uint8_t** in = (const uint8_t**)frame->extended_data;
          uint8_t** out = &is->m_audio_buf1;
          int out_samples =
              frame->nb_samples * is->m_params.freq / frame->sample_rate + 256;
          int out_bytes = av_samples_get_buffer_size(
              nullptr,
              is->m_params.channel_layout.nb_channels,
              out_samples,
              is->m_params.format,
              0);
          if (out_bytes < 0)
          {
            SPDLOG_ERROR("av_samples_get_buffer_size error: {}",
                         Utils::error_stringify(out_bytes));
            return;
          }
          SPDLOG_TRACE(
              "      out_samples: {}, out_bytes: {}", out_samples, out_bytes);

          av_fast_malloc(&is->m_audio_buf1, &is->m_audio_buf1_size, out_bytes);
          SPDLOG_TRACE(
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
              nullptr,
              is->m_params.channel_layout.nb_channels,
              len2,
              is->m_params.format,
              1);
          SPDLOG_TRACE("      get sample buffer: {}", is->m_audio_buf_size);
        }
        else
        {  // 不重采样
          SPDLOG_TRACE("    none resampleing");
          audio_size = av_samples_get_buffer_size(nullptr,
                                                  frame->ch_layout.nb_channels,
                                                  frame->nb_samples,
                                                  (AVSampleFormat)frame->format,
                                                  1);
          SPDLOG_TRACE("    get sample buffer: {}", audio_size);

          av_fast_malloc(&is->m_audio_buf1, &is->m_audio_buf1_size, audio_size);
          is->m_audio_buf = is->m_audio_buf1;
          is->m_audio_buf_size = audio_size;
          SPDLOG_TRACE(
              "    malloc: {} -> {}", audio_size, is->m_audio_buf1_size);

          memcpy(is->m_audio_buf, frame->data[0], audio_size);
        }
      }
      else
      {
        is->m_audio_buf = nullptr;
        is->m_audio_buf_size = 512;
        SPDLOG_TRACE("    pop 0 frame packet");
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

  if (opt_pts)
  {
    auto pts = std::chrono::milliseconds(
        MS_PER_S * opt_pts.value() * is->m_time_base.num / is->m_time_base.den);
    is->m_avsync->set_clock(pts);
    SPDLOG_DEBUG("audio pts: {}", pts);
  }
}

AudioOutput::AudioOutput(std::shared_ptr<AVFrameQueue> queue,
                         std::shared_ptr<AVSync> avsync)
    : m_queue(queue)
    , m_avsync(avsync)
{
}

AudioOutput::~AudioOutput() {}

int AudioOutput::init(const AVCodecParameters& params, AVRational time_base)
{
  m_time_base = time_base;

  if (const auto ret = SDL_Init(SDL_INIT_AUDIO); ret < 0)
  {
    SPDLOG_ERROR("SDL_Init(SDL_INIT_AUDIO) error: {}", SDL_GetError());
    return ret;
  }

  SDL_AudioSpec spec;
  spec.channels = params.ch_layout.nb_channels;
  spec.freq = params.sample_rate;
  spec.format = AUDIO_S16SYS;
  spec.silence = 0;
  spec.samples = params.frame_size;  // 采样数量
  spec.callback = fill_audio_pcm;
  spec.userdata = this;

  // AudioParams params;
  if (const auto ret = SDL_OpenAudio(&spec, nullptr); ret < 0)
  {
    SPDLOG_ERROR("SDL_OpenAudio error: {}", SDL_GetError());
    return ret;
  }

  if (spec.format != AUDIO_S16SYS)
  {
    SPDLOG_ERROR("音频不支持AUDIO_S16SYS格式输出");
    return -1;
  }

  m_params = spec;

  SDL_PauseAudio(0);

  SPDLOG_TRACE("spec: ");
  SPDLOG_TRACE(" - format({}) ", static_cast<int>(m_params.format));
  SPDLOG_TRACE(" - sample_rate({}) ", m_params.freq);
  SPDLOG_TRACE(" - channels({}) ", m_params.channel_layout.nb_channels);

  return 0;
}

void AudioOutput::deinit()
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}
