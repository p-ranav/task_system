#pragma once
#include <atomic>
#include <notification_queue.hpp>
#include <thread>
#include <vector>

class task_system {
  const unsigned _count{std::thread::hardware_concurrency()};
  std::vector<std::thread> _threads;
  std::vector<notification_queue> _q{_count};
  std::atomic<unsigned> _index{0};

  void run(unsigned i) {
    while (true) {
      std::function<void()> f;
      for (unsigned n = 0; n != _count; ++n) {
        if (_q[(i + n) % _count].try_pop(f))
          break;
      }
      if (!f && !_q[i].try_pop(f))
        break;
      f();
    }
  }

public:
  task_system() {
    for (unsigned n = 0; n != _count; ++n) {
      _threads.emplace_back([&, n] { run(n); });
    }
  }

  ~task_system() {
    for (auto &e : _q)
      e.done();
    for (auto &e : _threads)
      e.join();
  }

  template <typename F> void async_(F &&f) {
    auto i = _index++;

    for (unsigned n = 0; n != _count; ++n) {
      if (_q[(i + n) % _count].try_push(std::forward<F>(f)))
        return;
    }

    _q[i % _count].try_push(std::forward<F>(f));
  }
};
