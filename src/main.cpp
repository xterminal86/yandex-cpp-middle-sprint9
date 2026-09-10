#include <chrono>
#include <memory>
#include <print>
#include <thread>
#include <utility>

#include <SFML/Graphics.hpp>

#include <exec/any_sender_of.hpp>
#include <exec/repeat_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "sfml_display_sender.hpp"
#include "sfml_events_handler.hpp"
#include "types_sfml.hpp"

using namespace std::chrono_literals;
namespace ex = stdexec;

class WaitForFPS
{
  public:
    explicit WaitForFPS(FrameClock &frame_clock, unsigned int target_fps)
        : frame_clock_(frame_clock),
          frame_time_(std::chrono::milliseconds(1000 / target_fps)) {}

    void operator()()
    {
      auto cur_frame_duration = frame_clock_.GetFrameTime();

      if (cur_frame_duration < frame_time_)
      {
        std::this_thread::sleep_for(frame_time_ - cur_frame_duration);
      }
      frame_clock_.Reset();
    }

  private:
    FrameClock& frame_clock_;
    const std::chrono::milliseconds frame_time_ = 1ms;
};

class MandelbrotApp
{
  public:
    MandelbrotApp() :
      compute_pool_{std::max(1u, std::thread::hardware_concurrency())},
      sfml_thread_{1}
      {
        std::println("hardware_concurrency: {}\n",
                     std::thread::hardware_concurrency());
      }

    void Run()
    {
      auto compute_sched = compute_pool_.get_scheduler();
      auto sfml_sched = sfml_thread_.get_scheduler();

      auto initialize =
          ex::on(sfml_sched,
                  ex::just() | ex::then([this]()
                  {
                    state_ = std::make_unique<SfmlState>(  //
                      RenderSettings
                      {
                        .width = 800,
                        .height = 600,
                        .max_iterations = 50,
                        .escape_radius = 2.0
                      }
                    );
                  }));
      ex::sync_wait(std::move(initialize));

      auto process_frame = ex::on(
        sfml_sched,
        ex::just()
        | SfmlEventHandler{state_->window,
                           state_->render_settings,
                           state_->app_state}
      )
      | ex::then([this] { return &state_->fb; })
      | ex::continues_on(compute_sched)
      | mandelbrot::MakeComputeSender(state_->render_settings,
                                      state_->app_state.viewport)
      | ex::continues_on(sfml_sched)
      | render::MakeSfmlDisplaySender(*state_)
      | ex::then([this] { WaitForFPS{state_->frame_clock, 60}(); })
      | ex::then([this] { return state_->app_state.should_exit; });

      //auto repeated_pipeline = std::move(process_frame) | exec::repeat_until();
      //ex::sync_wait(std::move(repeated_pipeline));

      auto repeated_pipeline = std::move(process_frame) |
                               ex::then(
                                 [this]
                                 {
                                   return state_->app_state.should_exit;
                                 }
                               ) |
                               exec::repeat_until();
      ex::sync_wait(std::move(repeated_pipeline));
    }

  private:
    std::unique_ptr<SfmlState> state_;

    exec::static_thread_pool compute_pool_;
    exec::static_thread_pool sfml_thread_;
};

int main()
{
  std::println("=== Mandelbrot Fractal Renderer ===\n");
  std::println("Controls:");
  std::println("  Left Mouse Button  - Zoom In");
  std::println("  Right Mouse Button - Zoom Out");
  std::println("  X                  - Toggle Auto Zoom (infinite zoom to 'Seahorse Valley' point)");
  std::println("  C                  - Reset to Initial View");
  std::println("  Close Window       - Exit\n");

  try
  {
    MandelbrotApp app;
    app.Run();
  }
  catch (const std::exception &e)
  {
    std::println("Error: {}", e.what());
    return 1;
  }

  return 0;
}