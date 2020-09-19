#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using lock_t = std::unique_lock<std::mutex>;
using task = std::function<void()>;

class notification_queue {
  std::deque<task> queue_;
  std::mutex mutex_;

public:
  template <typename T> bool try_push(T &&task) {
    {
      lock_t lock{mutex_, std::try_to_lock};
      if (!lock)
        return false;
      queue_.emplace_back(std::forward<T>(task));
    }
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
};

class task_system {
  const unsigned count_;
  std::vector<std::thread> threads_;
  std::vector<notification_queue> queues_{count_};
  std::atomic_size_t index_{0};
  std::atomic_bool running_{false};
  std::mutex mutex_;               // Mutex to protect `enqueued_`
  std::condition_variable ready_;  // Signal to notify task enqueued
  std::atomic_size_t enqueued_{0}; // Incremented when a task is scheduled

  void run(unsigned i) {
    while (running_ || enqueued_ > 0) {
      // Wait for the `enqueued` signal
      {
        lock_t lock{mutex_};
        ready_.wait(lock, [this] { return enqueued_ > 0 || !running_; });
      }

      // dequeue task
      task t;
      bool dequeued{false};

      while (!dequeued) {
        for (unsigned n = 0; n != count_; ++n) {
          if (queues_[(i + n) % count_].try_pop(t)) {
            dequeued = true;
            if (enqueued_ > 0)
              enqueued_ -= 1;
            // execute task
            t();
            break;
          }
        }
        if (!(running_ || enqueued_ > 0))
          break;
      }
    }
  }

public:
  task_system(unsigned threads = std::thread::hardware_concurrency())
      : count_(threads) {
    for (unsigned n = 0; n != count_; ++n) {
      threads_.emplace_back([&, n] { run(n); });
    }
    running_ = true;
  }

  ~task_system() {
    running_ = false;
    ready_.notify_all();
    for (auto &t : threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  template <typename T> bool try_schedule(T &&task) {
    auto i = index_++;
    for (unsigned n = 0; n != count_; ++n) {
      if (queues_[(i + n) % count_].try_push(std::forward<T>(task))) {
        enqueued_ += 1;
        ready_.notify_one();
        return true;
      }
    }
    if (queues_[i % count_].try_push(std::forward<T>(task))) {
      enqueued_ += 1;
      ready_.notify_one();
      return true;
    }
    else {
      return false;
    }
  }

  template <typename T> void schedule(T &&task) {
    while (!try_schedule(task)) {}
  }

};