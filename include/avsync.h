#pragma once

#include <chrono>
#include <cmath>

// class AVSync
// {
//  public:
//   void init_clock() { set_clock(NAN); }

//   std::chrono::microseconds get_us()
//   {
//     auto ms = std::chrono::steady_clock::now().time_since_epoch();
//     return std::chrono::duration_cast<std::chrono::microseconds>(now);
//   }

//   double get_clock()
//   {
//     double time = get_us() / 1000000.0;
//     return m_pts_drift + time;
//   }

//   void set_clock_at(double pts, double time)
//   {
//     m_pts = pts;
//     m_pts_drift = pts - time;
//   }

//   void set_clock(double pts)
//   {
//     double time = get_us().count() / 1000000.0;
//     set_clock_at(pts, time);
//   }

//  private:
//   double m_pts{};
//   double m_pts_drift{};
// };

class AVSync
{
 public:
  using clock = std::chrono::steady_clock;
  using duration = clock::duration;
  using time_point = clock::time_point;
  using period = duration::period;

  duration get_clock() const { return clock::now() - m_base_time; }

  void set_clock(duration d)
  {
    m_base_time = clock::now() - d;
    m_duration = d;
  }

  static duration to_duration(double s)
  {
    return duration(static_cast<long long>(s * (period::den / period::num)));
  }

  static double to_double(duration d)
  {
    return (1.0 * period::num / period::den) * d.count();
  }

 private:
  time_point m_base_time;
  duration m_duration;
};
