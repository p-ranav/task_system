#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>

namespace task_system {

using lock_t = std::unique_lock<std::mutex>;
using task = std::function<void()>;

class notification_queue {
  std::deque<task> queue_;
  bool done_{false};
  std::mutex mutex_;
  std::condition_variable ready_;

public:
  template <typename T> bool try_push(T &&task) {
    {
      lock_t lock{mutex_, std::try_to_lock};
      if (!lock)
        return false;
      queue_.emplace_back(std::forward<T>(task));
    }
    ready_.notify_one();
    return true;
  }

  bool try_pop(task &task) {
    lock_t lock{mutex_, std::try_to_lock};
    if (!lock || queue_.empty())
      return false;
    task = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  void done() {
    {
      lock_t lock{mutex_};
      done_ = true;
    }
    ready_.notify_all();
  }
};

} // namespace task_system