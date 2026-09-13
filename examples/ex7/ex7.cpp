#include <print>
#include <chrono>
#include <thread>

#include <exec/create.hpp>
#include <exec/repeat_until.hpp>
#include <exec/repeat_n.hpp>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

class MySender
{
  public:
    // Сообщаем модели, что данный класс является сендером
    using sender_concept = ex::sender_t;

    // Определяем сигнатуру завершения нашего сендера
    using completion_signatures = ex::completion_signatures<
      // This needs to match with set_value().
        ex::set_value_t(bool)
      , ex::set_stopped_t()
    >;

    // -------------------------------------------------------------------------
    // Состояние операции. Описывает саму операцию и управляет своим завершением,
    // вызывая соответствующие методы ресивера
    template <class Receiver>
    struct OperationState
    {
      Receiver r;

      bool should_stop_ = false;

      OperationState(Receiver rec) : r(std::move(rec)) {}

      OperationState(OperationState&&) = default;
      OperationState(const OperationState&) = delete;

      void start() noexcept
      {
        std::println("MySender");
        std::this_thread::sleep_for(std::chrono::seconds{1});
        ex::set_value(std::move(r), should_stop_);
      }
    };
    // -------------------------------------------------------------------------

    // Метод connect создаёт состояние операции, соединяя сендер и ресивер
    template <class Receiver>
    auto connect(Receiver r) const
    {
      return OperationState<Receiver>{std::move(r)};
    }
};

int main()
{
  exec::static_thread_pool pool{2};
  auto sched = pool.get_scheduler();

  auto pipeline = ex::on(
    sched,
    ex::just()
  )
  | ex::let_value(
    []()
    {
      return MySender{};
    }
  )
  | exec::repeat_until();
  ex::sync_wait(std::move(pipeline));

  // This works.
  //auto pipeline = MySender{} | exec::repeat_until();
  //ex::sync_wait(std::move(pipeline));

  return 0;
}
