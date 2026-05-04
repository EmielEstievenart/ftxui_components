#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

enum class ToastLevel
{
    Info,
    Success,
    Warning,
    Error,
};

using ToastId = std::uint64_t;

struct ToastOption
{
    std::string title;
    std::string message;
    ToastLevel level = ToastLevel::Info;
    std::optional<float> progress;
    std::chrono::milliseconds timeout = std::chrono::seconds(3);
    bool dismiss_when_done = true;
};

struct ToastStyle
{
    ftxui::Color info       = ftxui::Color::Blue;
    ftxui::Color success    = ftxui::Color::Green;
    ftxui::Color warning    = ftxui::Color::Yellow;
    ftxui::Color error      = ftxui::Color::Red;
    ftxui::Color background = ftxui::Color::Black;
};

struct ToastHostOption
{
    ftxui::App* screen = nullptr;
    int width          = 48;
    int max_visible    = 3;
    ToastStyle style;
};

class ToastHostComponent : public ftxui::ComponentBase
{
public:
    explicit ToastHostComponent(ftxui::Component child, ToastHostOption option = {});
    ~ToastHostComponent() override;

    ToastId show(ToastOption option);
    void update(ToastId id, ToastOption option);
    void dismiss(ToastId id);
    void clear();

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override;

private:
    struct ToastState
    {
        ToastId id = 0;
        ToastOption option;
        std::chrono::steady_clock::time_point created_at;
        std::optional<std::chrono::steady_clock::time_point> expires_at;
    };

    void notify_changed();
    void wake_screen() const;
    void timer_loop();
    void prune_expired(std::chrono::steady_clock::time_point now);
    [[nodiscard]] std::optional<std::chrono::steady_clock::time_point> next_expiry_locked() const;
    [[nodiscard]] ftxui::Element render_toasts(const std::vector<ToastState>& toasts) const;
    [[nodiscard]] ftxui::Element render_toast(const ToastState& toast) const;

    ToastHostOption _option;
    mutable std::mutex _mutex;
    std::condition_variable _changed;
    std::vector<ToastState> _toasts;
    std::thread _timer;
    bool _stopping = false;
    ToastId _next_id = 1;
};

ftxui::Component ToastHost(ftxui::Component child, ToastHostOption option = {});
