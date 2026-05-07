#include <ftxui_components/toast_component.hpp>

#include <algorithm>
#include <utility>

namespace
{
float normalized_progress(std::optional<float> progress)
{
    if (!progress.has_value())
    {
        return 0.0F;
    }

    return std::clamp(*progress, 0.0F, 1.0F);
}

std::optional<std::chrono::steady_clock::time_point> expiry_for(const ToastOption& option,
                                                                std::chrono::steady_clock::time_point now)
{
    if (option.timeout <= std::chrono::milliseconds(0))
    {
        return std::nullopt;
    }

    return now + option.timeout;
}

void apply_completion_timeout(ToastOption& option)
{
    if (option.dismiss_when_done && option.progress.has_value() && normalized_progress(option.progress) >= 1.0F && option.timeout <= std::chrono::milliseconds(0))
    {
        option.timeout = std::chrono::seconds(4);
    }
}

ftxui::Color color_for(ToastLevel level, const ToastStyle& style)
{
    switch (level)
    {
    case ToastLevel::Success:
        return style.success;
    case ToastLevel::Warning:
        return style.warning;
    case ToastLevel::Error:
        return style.error;
    case ToastLevel::Info:
    default:
        return style.info;
    }
}
}

ToastHostComponent::ToastHostComponent(ftxui::Component child, ToastHostOption option)
    : _option(option)
{
    Add(std::move(child));
    _timer = std::thread([this] { timer_loop(); });
}

ToastHostComponent::~ToastHostComponent()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _stopping = true;
    }
    _changed.notify_all();
    if (_timer.joinable())
    {
        _timer.join();
    }
}

ToastId ToastHostComponent::show(ToastOption option)
{
    const auto now = std::chrono::steady_clock::now();
    apply_completion_timeout(option);
    const auto expires_at = expiry_for(option, now);
    ToastId id     = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        id = _next_id++;
        _toasts.push_back({id, std::move(option), now, expires_at});
    }

    notify_changed();
    return id;
}

void ToastHostComponent::update(ToastId id, ToastOption option)
{
    const auto now = std::chrono::steady_clock::now();
    apply_completion_timeout(option);
    const auto expires_at = expiry_for(option, now);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto toast = std::find_if(_toasts.begin(), _toasts.end(), [id](const ToastState& candidate) { return candidate.id == id; });
        if (toast == _toasts.end())
        {
            return;
        }

        toast->option     = std::move(option);
        toast->created_at = now;
        toast->expires_at = expires_at;
    }

    notify_changed();
}

void ToastHostComponent::dismiss(ToastId id)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _toasts.erase(std::remove_if(_toasts.begin(), _toasts.end(), [id](const ToastState& toast) { return toast.id == id; }), _toasts.end());
    }

    notify_changed();
}

void ToastHostComponent::clear()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _toasts.clear();
    }

    notify_changed();
}

ftxui::Element ToastHostComponent::OnRender()
{
    std::vector<ToastState> visible_toasts;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        prune_expired(std::chrono::steady_clock::now());
        visible_toasts = _toasts;
    }

    ftxui::Element child = ChildAt(0)->Render();
    if (visible_toasts.empty())
    {
        return child;
    }

    return ftxui::dbox({
        child,
        render_toasts(visible_toasts),
    });
}

bool ToastHostComponent::OnEvent(ftxui::Event event)
{
    return ChildAt(0)->OnEvent(std::move(event));
}

bool ToastHostComponent::Focusable() const
{
    return !children_.empty() && children_[0]->Focusable();
}

void ToastHostComponent::notify_changed()
{
    _changed.notify_all();
    wake_screen();
}

void ToastHostComponent::wake_screen() const
{
    if (_option.screen != nullptr)
    {
        _option.screen->PostEvent(ftxui::Event::Custom);
    }
}

void ToastHostComponent::timer_loop()
{
    std::unique_lock<std::mutex> lock(_mutex);
    while (!_stopping)
    {
        const auto next_expiry = next_expiry_locked();
        if (!next_expiry.has_value())
        {
            _changed.wait(lock, [this] { return _stopping || next_expiry_locked().has_value(); });
            continue;
        }

        if (_changed.wait_until(lock, *next_expiry, [this, next_expiry] { return _stopping || next_expiry_locked() != next_expiry; }))
        {
            continue;
        }

        prune_expired(std::chrono::steady_clock::now());
        lock.unlock();
        wake_screen();
        lock.lock();
    }
}

void ToastHostComponent::prune_expired(std::chrono::steady_clock::time_point now)
{
    _toasts.erase(std::remove_if(_toasts.begin(),
                                 _toasts.end(),
                                 [now](const ToastState& toast)
                                 {
                                     return toast.expires_at.has_value() && *toast.expires_at <= now;
                                 }),
                  _toasts.end());
}

std::optional<std::chrono::steady_clock::time_point> ToastHostComponent::next_expiry_locked() const
{
    std::optional<std::chrono::steady_clock::time_point> next;
    for (const ToastState& toast : _toasts)
    {
        if (!toast.expires_at.has_value())
        {
            continue;
        }

        if (!next.has_value() || *toast.expires_at < *next)
        {
            next = toast.expires_at;
        }
    }

    return next;
}

ftxui::Element ToastHostComponent::render_toasts(const std::vector<ToastState>& toasts) const
{
    ftxui::Elements rendered;
    const int max_visible = std::max(1, _option.max_visible);
    const auto first      = toasts.size() > static_cast<std::size_t>(max_visible)
                            ? toasts.size() - static_cast<std::size_t>(max_visible)
                            : std::size_t{0};

    for (std::size_t index = first; index < toasts.size(); ++index)
    {
        rendered.push_back(render_toast(toasts[index]) | ftxui::clear_under);
    }

    auto stack = ftxui::vbox(std::move(rendered)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, std::max(20, _option.width));
    return ftxui::vbox({
               ftxui::hbox({ftxui::filler(), stack}),
               ftxui::filler(),
           }) |
           ftxui::flex;
}

ftxui::Element ToastHostComponent::render_toast(const ToastState& toast) const
{
    ftxui::Elements lines;
    const ftxui::Color accent = color_for(toast.option.level, _option.style);
    if (!toast.option.title.empty())
    {
        lines.push_back(ftxui::text(toast.option.title) | ftxui::bold | ftxui::color(accent));
    }
    if (!toast.option.message.empty())
    {
        lines.push_back(ftxui::paragraph(toast.option.message));
    }
    if (toast.option.progress.has_value())
    {
        lines.push_back(ftxui::gauge(normalized_progress(toast.option.progress)) | ftxui::color(accent));
    }

    if (lines.empty())
    {
        lines.push_back(ftxui::text(""));
    }

    return ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::bgcolor(_option.style.background);
}

ftxui::Component ToastHost(ftxui::Component child, ToastHostOption option)
{
    return std::make_shared<ToastHostComponent>(std::move(child), option);
}
