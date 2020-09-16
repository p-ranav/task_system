#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
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
  std::atomic_bool enqueued_{false}; // Set to true when a task is scheduled

  void run(unsigned i) {
    while (running_) {
      // Wait for the `enqueued` signal
      {
        lock_t lock{mutex_};
        ready_.wait(lock, [this] { return enqueued_.load() || !running_; });
        enqueued_ = false;
      }

      if (!running_) {
        break;
      }

      task t;

      // dequeue task
      bool dequeued = false;
      do {
        for (unsigned n = 0; n != count_; ++n) {
          if (queues_[(i + n) % count_].try_pop(t)) {
            dequeued = true;
            break;
          }
        }
      } while (!dequeued && running_);

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
    running_ = false;
    for (auto &q : queues_)
      q.done();
    for (auto &t : threads_)
      if (t.joinable())
        t.join();
  }

  template <typename T> void schedule(T &&task) {
    auto i = index_++;

    // Enqueue task

    while (running_ && !enqueued_) {
      for (unsigned n = 0; n != count_; ++n) {
        if (queues_[(i + n) % count_].try_push(std::forward<T>(task))) {
          // Send `enqueued` signal to worker threads
          {
            lock_t lock{mutex_};
            enqueued_ = true;
            ready_.notify_one();
          }
          break;
        }
      }
    }
  }
};