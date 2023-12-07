#include "videooutput.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include "ffmpeg_utils.h"

namespace
{
constexpr auto MS_PER_S = std::chrono::milliseconds::period::den /
                          std::chrono::milliseconds::period::num;
constexpr auto REST_DURATION = std::chrono::milliseconds(10);
}  // namespace

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

void VideoOutput::deinit()
{
  SDL_DestroyTexture(m_texture);
  SDL_DestroyRenderer(m_renderer);
  SDL_DestroyWindow(m_window);
}

void VideoOutput::main_loop()
{
  int frames = 0;
  while (true)
  {
    if (const auto &event_opt = pop_user_event();
        event_opt && handle_user_event(*event_opt))
    {
      SPDLOG_INFO("break main loop");
      break;
    }

    const auto &duration_opt = get_next_refresh_duration();
    if (!duration_opt)
    {
      SPDLOG_TRACE("get_next_refresh_duration: {}, take a rest: {}",
                   duration_opt,
                   REST_DURATION);
      std::this_thread::sleep_for(REST_DURATION);
      continue;
    }

    if (const auto &duration = *duration_opt; duration.count() > 0)
    {  // 距离下一帧画面还有duration时长，休息至多REST_DURATION(不能影响用户操作)
      auto d = std::min(duration, REST_DURATION);
      SPDLOG_TRACE(
          "get_next_refresh_duration: {}, take a rest: {}", duration_opt, d);
      std::this_thread::sleep_for(d);
      continue;
    }

    // 刷新画面
    SPDLOG_DEBUG("get_next_refresh_duration: {}, refreshing", duration_opt);
    refresh_video();
    frames++;
  }
  SPDLOG_INFO("played {} frames", frames);
}

std::optional<SDL_Event> VideoOutput::pop_user_event()
{
  SDL_PumpEvents();
  if (!SDL_HasEvents(SDL_FIRSTEVENT, SDL_LASTEVENT))
  {
    SPDLOG_TRACE("SDL_PeepEvents no user event");
    return std::nullopt;
  }

  SDL_Event event;
  if (const auto ret = SDL_PeepEvents(
          &event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
      ret < 0)
  {
    SPDLOG_ERROR("SDL_PeepEvents error: {}", SDL_GetError());
    return std::nullopt;
  }

  SPDLOG_INFO("SDL_PeepEvents 1 user event");
  return event;
}

bool VideoOutput::handle_user_event(const SDL_Event &event) const
{
  switch (event.type)
  {
  case SDL_KEYDOWN:
    if (event.key.keysym.sym == SDLK_ESCAPE)
    {
      SPDLOG_INFO("ESC key down, quit");
      return true;
    }
    break;
  case SDL_QUIT:
    SPDLOG_INFO("SDL_QUIT");
    return true;
  default:
    break;
  }
  return false;
}

std::optional<std::chrono::milliseconds>
VideoOutput::get_next_refresh_duration() const
{
  if (m_queue->empty())
  {
    return std::nullopt;
  }

  auto &frame = m_queue->front();
  assert(frame);

  auto next_frame_pts = std::chrono::milliseconds(
      MS_PER_S * frame->pts * m_time_base.num / m_time_base.den);
  const auto now_pts = std::chrono::duration_cast<std::chrono::milliseconds>(
      m_avsync->get_clock());

  return next_frame_pts - now_pts;
}

void VideoOutput::refresh_video()
{
  auto frame_opt = m_queue->pop();
  assert(frame_opt);
  auto frame = *frame_opt;
  assert(frame);

  auto pts = std::chrono::milliseconds(MS_PER_S * frame->pts * m_time_base.num /
                                       m_time_base.den);
  SPDLOG_DEBUG("video pts: {}", pts);

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
}
