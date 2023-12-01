#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

#include <spdlog/fmt/chrono.h>
#include <spdlog/spdlog.h>

template <typename T>
class LockedQueue
{
  using lock_type = std::mutex;
  using unique_lock = std::unique_lock<lock_type>;
  using lock_guard = std::lock_guard<lock_type>;

 public:
  std::optional<T> pop(
      std::chrono::milliseconds ms = std::chrono::milliseconds(0))
  {
    unique_lock locker(m_lock);
    SPDLOG_TRACE("begin wait for: {}", ms);
    if (!m_condvar.wait_for(locker, ms, [this]() { return !m_queue.empty(); }))
    {
      SPDLOG_TRACE("wait for: {} failed", ms);
      return std::nullopt;
    }
    SPDLOG_TRACE("finished wait for: {}, queue size: {}", ms, m_queue.size());

    auto v = std::move(m_queue.front());
    m_queue.pop_front();
    return v;
  }

  void push(T &val)
  {
    lock_guard locker(m_lock);
    m_queue.push_back(std::forward<T>(val));
    m_condvar.notify_one();
  }

  size_t size()
  {
    lock_guard locker(m_lock);
    return m_queue.size();
  }

 private:
  lock_type m_lock;
  std::condition_variable m_condvar;
  std::deque<T> m_queue;
};
