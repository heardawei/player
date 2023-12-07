#pragma once

#include <chrono>
#include <memory>
#include <optional>

#include <SDL2/SDL.h>
#include <ffmpeg/avutil>

#include "avframequeue.h"
#include "avsync.h"

class VideoOutput
{
 public:
  VideoOutput(std::shared_ptr<AVFrameQueue> queue,
              std::shared_ptr<AVSync> avsync);

  int init(int width, int height, AVRational time_base);
  void deinit();
  void main_loop();

 private:
  std::optional<SDL_Event> pop_user_event();
  bool handle_user_event(const SDL_Event &event) const;
  std::optional<std::chrono::milliseconds> get_next_refresh_duration() const;
  void refresh_video();

 private:
  std::shared_ptr<AVFrameQueue> m_queue;
  std::shared_ptr<AVSync> m_avsync;
  AVRational m_time_base;
  SDL_Event m_event;
  SDL_Rect m_rect;
  SDL_Window *m_window{};
  SDL_Renderer *m_renderer{};
  SDL_Texture *m_texture{};
  int m_width{};
  int m_height{};

  uint8_t *m_yuv_buf{};
  int m_yuv_buf_size{};
  SDL_mutex *m_mutex{};
};