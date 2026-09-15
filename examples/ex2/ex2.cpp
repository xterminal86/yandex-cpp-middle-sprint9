#include <ranges>
#include <random>
#include <chrono>
#include <print>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/create.hpp>

namespace ex = stdexec;
namespace rv = std::views;

template <class WorkFn>
struct CancellableSender
{
  // This tells the stdexec library that this class / struct is a sender.
  using sender_concept = ex::sender_t;

  // Define all 3 channels.
  using completion_signatures = ex::completion_signatures<
    ex::set_value_t(), ex::set_stopped_t(), ex::set_error_t(std::exception_ptr)
  >;

  exec::static_thread_pool::scheduler scheduler;
  WorkFn work;

  template <class Receiver, class StopToken>
  struct OperationState
  {
    Receiver r;
    exec::static_thread_pool::scheduler scheduler;
    WorkFn work;
    StopToken token;

    void start() noexcept
    {
      if (token.stop_requested())
      {
        ex::set_stopped(std::move(r));
        return;
      }

      work(token);

      if (token.stop_requested())
      {
        ex::set_stopped(std::move(r));
      }
      else
      {
        ex::set_value(std::move(r));
      }
    }
  };

  template <class Receiver>
  auto connect(Receiver r) const
  {
    auto env = ex::get_env(r);
    // token здесь типа stdexec::inplace_stop_token
    auto token = ex::get_stop_token(env);
    return OperationState
    {
      std::move(r),
      scheduler,
      std::move(work),
      std::move(token)
    };
  }
};

inline auto make_cancellable_sender(exec::static_thread_pool::scheduler sched,
                                    auto work)
{
  return CancellableSender{sched, std::move(work)};
}

int main()
{
  exec::static_thread_pool pool(2);  // Пул для нашей работы
  ex::scheduler auto sched = pool.get_scheduler();

  // "Воркер" для посыла cancel токенов по результатам подбрасывания монетки.
  auto timeout_sender = exec::create<
    ex::completion_signatures<ex::set_value_t(), ex::set_stopped_t()>
  >
  (
    [](auto ctx) noexcept
    {
      std::mt19937 gen{std::random_device{}()};
      // с заданной частотой возвращает true
      std::bernoulli_distribution dist{0.5};

      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (dist(gen))
      {
        ex::set_stopped(std::move(ctx.receiver));
      }
      else
      {
        ex::set_value(std::move(ctx.receiver));
      }
    }
  );

  // Основной "воркер".
  auto work = make_cancellable_sender(
    sched,
    // Лямбда получает токен
    [](auto token)
    {
      std::println("\n\nWork started...");
      for (int i = 0; i < 5; ++i)
      {
        if (token.stop_requested())
        {
          std::println("\t> Cancellation was detected.");
          return;  // Выходим досрочно
        }
        std::println("\t> Working... ({} / 5)", i + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
      std::println("Work finished.");
    }
  );

  auto combined_work = ex::when_all(
    // Вроде без разницы.
    //  ex::on(sched, std::move(work))
    //, ex::on(sched, std::move(timeout_sender))
      ex::on(sched, std::move(timeout_sender))
    , ex::on(sched, std::move(work))
  );

  // Запускаем
  for (auto idx : rv::iota(0, 5))
  {
    try
    {
      ex::sync_wait(std::move(combined_work));
    }
    catch (const std::exception &e)
    {
      std::println("sync_wait caught exception: {}", e.what());
    }
  }

  return 0;
}
