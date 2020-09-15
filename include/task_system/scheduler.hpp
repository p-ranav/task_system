#pragma once
#include <atomic>
#include <task_system/notification_queue.hpp>
#include <thread>
#include <vector>

namespace task_system {

class scheduler {
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
        std::unique_lock<std::mutex> lock{mutex_};
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
  scheduler(unsigned threads = std::thread::hardware_concurrency())
      : count_(threads) {
    for (unsigned n = 0; n != count_; ++n) {
      threads_.emplace_back([&, n] { run(n); });
    }
    running_ = true;
  }

  ~scheduler() {
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
            std::unique_lock<std::mutex> lock{mutex_};
            enqueued_ = true;
            ready_.notify_one();
          }
          break;
        }
      }
    }
  }
};

} // namespace task_system