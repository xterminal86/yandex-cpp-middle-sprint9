#include <exec/create.hpp>
#include <stdexec/execution.hpp>

#include <chrono>
#include <optional>
#include <print>
#include <random>
#include <string>
#include <thread>

namespace ex = stdexec;

struct Mesh 
{
  std::string name;
  int polygons{};
};

auto HighQualityFixSender() 
{
  return exec::create<
    ex::completion_signatures<
      ex::set_value_t(Mesh), 
      ex::set_error_t(std::exception_ptr), 
      ex::set_stopped_t()
    >
  >(
    [](auto ctx) noexcept 
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      std::mt19937 gen(std::random_device{}());
      std::uniform_int_distribution<int> dist(0, 2);

      int code = dist(gen);
      if (code == 0) 
      {
        ex::set_value(std::move(ctx.receiver), Mesh{"HQ-Fixed", 120000});
      } 
      else if (code == 1) 
      {
        ex::set_stopped(std::move(ctx.receiver));
      } 
      else 
      {
        ex::set_error(
          std::move(ctx.receiver),
          std::make_exception_ptr(std::runtime_error("HQ algorithm failed"))
        );
      }
    }
  );
}

auto FastFallbackSender() 
{
  return exec::create<
    ex::completion_signatures<
      ex::set_value_t(Mesh), 
      ex::set_error_t(std::exception_ptr), 
      ex::set_stopped_t()
    >
  >(
    [](auto ctx) noexcept 
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      std::mt19937 gen(std::random_device{}());
      std::uniform_int_distribution<int> dist(0, 3);

      int code = dist(gen);
      if (code <= 1) 
      {
        ex::set_value(std::move(ctx.receiver), Mesh{"Fallback-Fixed", 60000});
      } 
      else 
      {
        ex::set_error(
          std::move(ctx.receiver),
          std::make_exception_ptr(std::runtime_error("Fallback failed too"))
        );
      }
    }
  );
}

template <ex::sender Primary, ex::sender Fallback>
auto ProcessModelWithFallback(Primary primary, Fallback fallback) 
{
  return primary | 
  ex::let_error(
    [fallback](std::exception_ptr) 
    {
       // Переходим на fallback-алгоритм
       std::println("Main algorithm failed, switching to the Fallback algorithm");
       return fallback;
    }
  ) |
  ex::then(
    [](Mesh m) 
    {
      std::println("Model processed successfully");
      return m;
    }
  ) |
  ex::upon_error(
    [](std::exception_ptr) 
    {
      std::println("Both algorithms failed");
      return Mesh{};
    }
  ) |
  ex::upon_stopped(
    [] 
    {
      std::println("Processing was cancelled");
      return Mesh{};
    }
  );
}

int main() {
    auto task = ProcessModelWithFallback(HighQualityFixSender(), 
                                         FastFallbackSender()) |
                ex::then(
                  [](Mesh m) 
                  { 
                    std::println("Final model: {} ({} polygons)", 
                                 m.name, m.polygons); 
                  }
                );

    ex::sync_wait(task);
}
