#include <task_system/task_system.hpp>
#include <string_view>
#include <string>
#include <fstream>
#include <iostream>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>

int main() {
  task_system s(1);

  auto async_file = spdlog::basic_logger_mt<spdlog::async_factory>("async_file_logger", "foo.log");

  auto log = [&async_file](size_t i) {
    async_file->info("Hello World {}", i);
  };

  auto writer1 = std::thread([&] {
    for (auto i = 0; i < 50000; i++) {
      s.schedule(std::bind(log, i));
    }
  });

  auto writer2 = std::thread([&] {
    for (auto i = 50000; i < 100000; i++) {
      s.schedule(std::bind(log, i));
    }
  });

  auto writer3 = std::thread([&] {
    for (auto i = 100000; i < 200000; i++) {
      s.schedule(std::bind(log, i));
    }
  });

  writer1.join();
  writer2.join();
  writer3.join();
}