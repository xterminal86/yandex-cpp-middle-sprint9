#include <print>

#include <exec/create.hpp>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

int main()
{
  exec::static_thread_pool pool{4};
  auto sched = pool.get_scheduler();

  auto pipeline = stdexec::just(std::string{"hello"})
                | stdexec::then(
                    [](std::string s)
                    {
                      auto tid = std::this_thread::get_id();
                      std::println("s + 'world' - {}", tid);
                      return s + ", world";
                    }
                  )
                | stdexec::then(
                    [](std::string s)
                    {
                      auto tid = std::this_thread::get_id();
                      std::println("s.size()    - {}", tid);
                      return s.size();
                    }
                  );

  auto r = stdexec::sync_wait(
    stdexec::on(sched, std::move(pipeline))
  );

  auto [v] = r.value();
  std::println("{}", v);

  return 0;
}
