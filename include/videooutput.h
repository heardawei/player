#pragma once

#include <memory>

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
  int main_loop();

 private:
  void refresh_loop_wait_event(SDL_Event &event);
  void refresh_video(int &remaining_time);

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