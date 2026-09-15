#pragma once

#include <SFML/Graphics.hpp>
#include <stdexec/execution.hpp>

#include "types_core.hpp"

class SfmlEventHandler
{
  public:
    using sender_concept = ex::sender_t;

    using completion_signatures = ex::completion_signatures<
        ex::set_value_t()
      , ex::set_stopped_t()
    >;

    SfmlEventHandler(sf::RenderWindow& window,
                     RenderSettings render_settings,
                     AppState& state)
        : window_{window}, render_settings_{render_settings}, state_{state} {}

  // ---------------------------------------------------------------------------
    template <typename Receiver>
    struct OperationState
    {
      using operation_state_concept = stdexec::operation_state_tag;

      Receiver receiver_;
      sf::RenderWindow& window_;
      RenderSettings render_settings_;
      AppState& state_;

      static constexpr float ZOOM_INTERVAL_MS = 100.0f;

      template <typename R>
      explicit OperationState(
        R&& r,
        sf::RenderWindow& window,
        RenderSettings render_settings,
        AppState& state
      ) : receiver_{std::forward<R>(r)},
          window_{window},
          render_settings_{render_settings},
          state_{state} {}

      void start() noexcept
      {
        HandleEvents();
        HandleAutoZoom();

        if (state_.should_exit)
        {
          ex::set_stopped(std::move(receiver_));
          return;
        }

        ex::set_value(std::move(receiver_));
      }

      private:
        void HandleEvents()
        {
          bool wasEvent = false;
          sf::Event event;
          while (window_.pollEvent(event))
          {
            wasEvent = true;

            switch (event.type)
            {
              case sf::Event::Closed:
                state_.should_exit = true;
                break;

              case sf::Event::KeyPressed:
                HandleKeyPress(event.key);
                break;

              case sf::Event::MouseButtonPressed:
                HandleMousePress(event.mouseButton);
                break;

              case sf::Event::MouseButtonReleased:
                HandleMouseRelease(event.mouseButton);
                break;

              default:
                break;
            }
          }

          if (state_.first_start)
          {
            state_.first_start = false;
            state_.need_rerender = true;
          }
          else
          {
            if (not state_.auto_zoom_enabled and not wasEvent)
            {
              state_.need_rerender = false;
            }
          }
        }

        void HandleKeyPress(const sf::Event::KeyEvent& key)
        {
          switch (key.code)
          {
            case sf::Keyboard::Escape:
              state_.should_exit = true;
              break;

            case sf::Keyboard::X:
              state_.auto_zoom_enabled = !state_.auto_zoom_enabled;
              state_.need_rerender = true;
              if (state_.auto_zoom_enabled)
              {
                state_.zoom_clock.restart();
              }
              break;

            case sf::Keyboard::C:
              state_.viewport = AppState::INITIAL_VIEWPORT;
              state_.auto_zoom_enabled = false;
              state_.need_rerender = true;
              break;

            default:
              state_.need_rerender = false;
              break;
          }
        }

        void HandleMousePress(const sf::Event::MouseButtonEvent &mouse)
        {
          switch (mouse.button)
          {
            case sf::Mouse::Left:
            {
              state_.left_mouse_pressed = true;
              ZoomToPoint(mouse.x, mouse.y, /*zoom_in=*/true);
            }
            break;

            case sf::Mouse::Right:
            {
              state_.right_mouse_pressed = true;
              ZoomToPoint(mouse.x, mouse.y, /*zoom_in=*/false);
            }
            break;

            default:
              state_.need_rerender = false;
              break;
          }
        }

        void HandleMouseRelease(const sf::Event::MouseButtonEvent &mouse)
        {
          switch (mouse.button)
          {
            case sf::Mouse::Left:
              state_.left_mouse_pressed = false;
              break;

            case sf::Mouse::Right:
              state_.right_mouse_pressed = false;
              break;

            default:
              state_.need_rerender = false;
              break;
          }
        }

        void HandleAutoZoom()
        {
          if (!state_.auto_zoom_enabled)
          {
            return;
          }

          if (state_.zoom_clock.getElapsedTime().asMilliseconds()
              < static_cast<int>(ZOOM_INTERVAL_MS))
          {
            return;
          }

          const double target_x = state_.viewport.x_min +
              (state_.viewport.x_max - state_.viewport.x_min) *
                ((AppState::AUTO_ZOOM_TARGET_X - state_.viewport.x_min) /
                state_.viewport.width());

          // Проще: используем реальные координаты вьюпорта, чтобы найти пиксель
          // центра внимания. Пересчитаем пиксель для целевой точки:
          const int px = static_cast<int>(
              (AppState::AUTO_ZOOM_TARGET_X - state_.viewport.x_min) /
              state_.viewport.width() * render_settings_.width);
          const int py = static_cast<int>(
              (AppState::AUTO_ZOOM_TARGET_Y - state_.viewport.y_min) /
              state_.viewport.height() * render_settings_.height);

          ZoomToPoint(px, py, /*zoom_in=*/true);
        }

        void ZoomToPoint(int pixel_x,
                         int pixel_y,
                         bool zoom_in,
                         double factor = 0.8)
        {
          // Ограничение частоты: не чаще ZOOM_INTERVAL_MS
          if (state_.zoom_clock.getElapsedTime().asMilliseconds()
              < static_cast<int>(ZOOM_INTERVAL_MS))
          {
            return;
          }

          const double target_x = state_.viewport.x_min +
              (double(pixel_x) / render_settings_.width) *
              state_.viewport.width();
          const double target_y = state_.viewport.y_min +
              (double(pixel_y) / render_settings_.height) *
              state_.viewport.height();

          const double zoom_factor = zoom_in ? factor : (1.0 / factor);
          const double new_width  = state_.viewport.width()  * zoom_factor;
          const double new_height = state_.viewport.height() * zoom_factor;

          // Курсор остаётся на той же точке фрактала => новые границы:
          state_.viewport.x_min = target_x -
              (double(pixel_x) / render_settings_.width) * new_width;
          state_.viewport.x_max = state_.viewport.x_min + new_width;

          state_.viewport.y_min = target_y -
              (double(pixel_y) / render_settings_.height) * new_height;
          state_.viewport.y_max = state_.viewport.y_min + new_height;

          state_.need_rerender = true;
          state_.zoom_clock.restart();
        }
    };
  // ---------------------------------------------------------------------------

    sf::RenderWindow& window_;
    RenderSettings render_settings_;
    AppState& state_;

    template <typename Receiver>
    auto connect(Receiver receiver) const
    {
      return OperationState<Receiver>{
        std::move(receiver),
        window_,
        render_settings_,
        state_
      };
    }
};
