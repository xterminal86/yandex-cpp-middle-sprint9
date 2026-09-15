#include <atomic>
#include <chrono>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <ranges>
#include <semaphore>
#include <stdexec/execution.hpp>
#include <thread>
#include <exec/start_detached.hpp>

namespace ex = stdexec;
namespace rv = std::views;

class LimitedThreadPool {
public:
    explicit LimitedThreadPool(const std::uint32_t max_concurrent)
        : pool_{max_concurrent}, semaphore_(max_concurrent), active_count_(0), max_slots_(max_concurrent) {}

    template <typename Sender>
    void Submit(Sender &&sender) {
        semaphore_.acquire();
        ++active_count_;

        auto sched = pool_.get_scheduler();
        auto task = ex::on(
          sched,
          std::forward<Sender>(sender)) | ex::then([this]()
          {
              --active_count_;
              semaphore_.release();
          }
        );

        exec::start_detached(std::move(task));
    }

    std::uint32_t ActiveCount() const { return active_count_; }

    void WaitAll() {
        for (auto i : rv::iota(0u, max_slots_)) {
            semaphore_.acquire();
        }
        for (auto i : rv::iota(0u, max_slots_)) {
            semaphore_.release();
        }
    }

private:
    std::counting_semaphore<> semaphore_;
    std::atomic<std::uint32_t> active_count_;
    const std::uint32_t max_slots_;
    exec::static_thread_pool pool_;
};

int main() {
    {
        LimitedThreadPool workers{4};  // Максимум 2 одновременные задачи

        std::println("Submitting 10 tasks with limit of 4 concurrent:");
        for (auto i : rv::iota(1, 10)) {
            auto task = ex::just(i * 1000) | ex::then([i, &workers](int delay) {
                            std::println("= Task {} started (active: {})", i, workers.ActiveCount());
                            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                            std::println("= Task {} finished", i);
                        });

            workers.Submit(std::move(task));
        }

        std::println("Number of active tasks is {}. Wait for all to be finished...", workers.ActiveCount());
        workers.WaitAll();
        std::println("All tasks completed");

        std::println("Submitting 10 more tasks and exit LimitedThreadPool scope:");
        for (auto i : rv::iota(1, 10)) {
            auto task = ex::just(i * 1000) | ex::then([i, &workers](int delay) {
                            std::println("= Task {} started (active: {})", i, workers.ActiveCount());
                            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                            std::println("= Task {} finished", i);
                        });

            workers.Submit(std::move(task));
        }
        std::println("Before LimitedThreadPool destruction");
    }

    std::println("All tasks completed");
}
