#pragma once

#include <string>

#include <ffmpeg/avutil>

class Utils
{
 public:
  static std::string error_stringify(int error)
  {
    char estr[AV_ERROR_MAX_STRING_SIZE]{};
    return av_make_error_string(estr, AV_ERROR_MAX_STRING_SIZE, error);
  }
};
