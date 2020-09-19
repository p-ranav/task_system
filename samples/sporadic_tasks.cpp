#include <task_system/task_system.hpp>
#include <iostream>

int main() {
  task_system s(2);

  s.schedule([]() { std::cout << "TaskA\n"; });
  s.schedule([]() { std::cout << "TaskB\n"; });
  s.schedule([]() { std::cout << "TaskC\n"; });
}