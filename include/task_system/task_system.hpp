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

class task_system {
  const unsigned count_;
  std::vector<std::thread> threads_;
  std::vector<notification_queue> queues_{count_};
  std::atomic<unsigned> index_{0};
  std::atomic_bool running_{false};  // Is the scheduler running?
  std::mutex mutex_;                 // Mutex to protect `enqueued_`
  std::condition_variable ready_;    // Signal to notify task enqueued
  std::atomic_size_t enqueued_{0}; // Incremented when a task is scheduled

  void run(unsigned i) {
    while (true) {
      // Wait for the `enqueued` signal
      {
        lock_t lock{mutex_};
        // std::cout << "Waiting\n";
        ready_.wait(lock, [this] { return enqueued_.load() > 0 || !running_; });
        if (enqueued_ > 0)
          enqueued_ -= 1;
        // std::cout << "Done waiting\n";
      }
      // dequeue task
      task t;
      for (unsigned n = 0; n != count_; ++n)
        if (queues_[(i + n) % count_].try_pop(t))
          break;

      if (!t && !queues_[i].try_pop(t)) continue;

      // execute task
      t();
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
    for (auto &q : queues_)
      q.done();
    for (auto &t : threads_)
      if (t.joinable())
        t.join();
  }

  template <typename T> void schedule(T &&task) {
    // std::cout << "Scheduling [";
    auto i = index_++;
    // std::cout << i << "]\n";

    for (unsigned n = 0; n != count_; ++n) {
      if (queues_[(i + n) % count_].try_push(std::forward<T>(task))) {
        lock_t lock{mutex_};
        enqueued_ += 1;
        ready_.notify_one();
        return;
      }
    }

    queues_[i % count_].try_push(std::forward<T>(task));
    lock_t lock{mutex_};
    enqueued_ += 1;
    ready_.notify_one();
  }
};