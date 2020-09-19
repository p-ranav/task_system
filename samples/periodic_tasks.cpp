#include <task_system/task_system.hpp>
#include <iostream>

int main() {
  ts::task_system s;

  auto timer_a = std::thread([&s]() {
    while (true) {
      s.schedule([] { std::cout << "Task A\n"; });
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });

  auto timer_b = std::thread([&s]() {
    while (true) {
      s.schedule([] { std::cout << "Task B\n"; });
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });

  auto timer_c = std::thread([&s]() {
    while (true) {
      s.schedule([] { std::cout << "Task C\n"; });
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  });

  timer_a.join();
  timer_b.join();
  timer_c.join();
}