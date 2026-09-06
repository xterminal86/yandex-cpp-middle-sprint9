#include <print>

#include <exec/create.hpp>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace ex = stdexec;

int main()
{
  exec::static_thread_pool pool{4};
  auto scheduler = pool.get_scheduler();

  struct InitialValue 
  {
    int val;
  };

  auto s =
      ex::on(scheduler, ex::just(InitialValue{10})) |
      ex::bulk(
        std::execution::par, 
        5, 
        [](int i, InitialValue init) 
        { 
          std::println("i={}  ->  {}", i, init.val + i); 
        }
      ) |
      ex::then(
        [](InitialValue init) 
        { 
          std::println("Все задачи для InitialValue={} были завершены", 
                       init.val); 
        }
      );

  ex::sync_wait(std::move(s));
  
  return 0;
}
