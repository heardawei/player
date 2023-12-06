#include "videooutput.h"

#include <spdlog/spdlog.h>

#include "ffmpeg_utils.h"

VideoOutput::VideoOutput(std::shared_ptr<AVFrameQueue> queue,
                         std::shared_ptr<AVSync> avsync)
    : m_queue(queue)
    , m_avsync(avsync)
{
}

int VideoOutput::init(int width, int height, AVRational time_base)
{
  m_width = width;
  m_height = height;
  m_time_base = time_base;

  SPDLOG_INFO("wh: {} x {}", width, height);

  if (const auto ret = SDL_Init(SDL_INIT_VIDEO); ret < 0)
  {
    SPDLOG_ERROR("SDL_Init(SDL_INIT_VIDEO) error: {}", SDL_GetError());
    return ret;
  }

  m_window = SDL_CreateWindow("player",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              width,
                              height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  if (!m_window)
  {
    SPDLOG_ERROR("SDL_CreateWindow() error: {}", SDL_GetError());
    return -1;
  }

  m_renderer = SDL_CreateRenderer(m_window, -1, 0);
  if (!m_renderer)
  {
    SPDLOG_ERROR("SDL_CreateRenderer() error: {}", SDL_GetError());
    return -1;
  }

  m_texture = SDL_CreateTexture(m_renderer,
                                SDL_PIXELFORMAT_IYUV,
                                SDL_TEXTUREACCESS_STREAMING,
                                width,
                                height);
  if (!m_texture)
  {
    SPDLOG_ERROR("SDL_CreateTexture() error: {}", SDL_GetError());
    return -1;
  }

  m_yuv_buf_size = width * height * 3 / 2;
  m_yuv_buf = (uint8_t *)malloc(m_yuv_buf_size);
  if (!m_yuv_buf)
  {
    SPDLOG_ERROR("malloc({}) error: {}", m_yuv_buf_size, SDL_GetError());
    return -1;
  }

  return 0;
}

int VideoOutput::main_loop()
{
  SDL_Event event;
  while (true)
  {
    refresh_loop_wait_event(event);

    switch (event.type)
    {
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE)
      {
        SPDLOG_INFO("ESC key down, quit");
        return 0;
      }
      break;
    case SDL_QUIT:
      SPDLOG_INFO("SDL_QUIT");
      break;
    default:
      break;
    }
  }
}

#define REFRESH_DELAY 10
void VideoOutput::refresh_loop_wait_event(SDL_Event &event)
{
  int remaining_time_ms = 0;
  SDL_PumpEvents();
  while (true)
  {
    if (const auto ret = SDL_PeepEvents(
            &event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
        ret < 0)
    {
      SPDLOG_ERROR("SDL_PeepEvents error: {}", SDL_GetError());
      break;
    }
    else if (ret > 0)
    {
      // SPDLOG_INFO("SDL_PeepEvents event: {}", ret);
      break;
    }
    // SPDLOG_INFO("SDL_PeepEvents event: 0");

    if (remaining_time_ms > 0.0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(remaining_time_ms));
    }
    remaining_time_ms = REFRESH_DELAY;

    // 尝试刷新画面
    refresh_video(remaining_time_ms);
    SDL_PumpEvents();
  }
}

void VideoOutput::refresh_video(int &remaining_time_ms)
{
  if (m_queue->empty())
  {
    return;
  }

  auto &frame = m_queue->front();
  if (frame)
  {
    double pts = frame->pts * av_q2d(m_time_base);
    SPDLOG_DEBUG("video pts: {} * ({} / {}) = {}",
                 frame->pts,
                 m_time_base.num,
                 m_time_base.den,
                 pts);

    auto diff = pts - m_avsync->get_clock();
    if (diff > 0)
    {
      remaining_time_ms =
          std::min(remaining_time_ms, static_cast<int>(diff * 1000));
      return;
    }

    m_rect.x = 0;
    m_rect.y = 0;
    m_rect.w = m_width;
    m_rect.h = m_height;
    SDL_UpdateYUVTexture(m_texture,
                         &m_rect,
                         frame->data[0],
                         frame->linesize[0],
                         frame->data[1],
                         frame->linesize[1],
                         frame->data[2],
                         frame->linesize[2]);
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, &m_rect);
    SDL_RenderPresent(m_renderer);
    m_queue->pop();
  }
}
