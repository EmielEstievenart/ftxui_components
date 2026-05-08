#include <ftxui_components/toast_component.hpp>

#include <algorithm>
#include <ftxui/screen/box.hpp>
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

std::optional<std::chrono::steady_clock::time_point> expiry_for(const ToastOption& option, std::chrono::steady_clock::time_point now)
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

int box_width(const ftxui::Box& box)
{
    return box.IsEmpty() ? 0 : box.x_max - box.x_min + 1;
}

int box_height(const ftxui::Box& box)
{
    return box.IsEmpty() ? 0 : box.y_max - box.y_min + 1;
}

void layout_element(ftxui::Element& element, ftxui::Box box)
{
    ftxui::Node::Status status;
    element->Check(&status);
    constexpr int max_iterations = 20;
    while (status.need_iteration && status.iteration < max_iterations)
    {
        element->ComputeRequirement();
        element->SetBox(box);
        status.need_iteration = false;
        ++status.iteration;
        element->Check(&status);
    }
}

int fitted_height(ftxui::Element element, int width)
{
    if (element == nullptr || width <= 0)
    {
        return 0;
    }

    ftxui::Box measurement_box;
    measurement_box.x_min = 0;
    measurement_box.x_max = width - 1;
    measurement_box.y_min = 0;
    measurement_box.y_max = 100000;
    layout_element(element, measurement_box);
    return element->requirement().min_y;
}

ftxui::Element render_toast_option(const ToastOption& toast, const ToastStyle& style)
{
    ftxui::Elements lines;
    const ftxui::Color accent = color_for(toast.level, style);
    if (!toast.title.empty())
    {
        lines.push_back(ftxui::text(toast.title) | ftxui::bold | ftxui::color(accent));
    }
    if (!toast.message.empty())
    {
        lines.push_back(ftxui::paragraph(toast.message));
    }
    if (toast.progress.has_value())
    {
        lines.push_back(ftxui::gauge(normalized_progress(toast.progress)) | ftxui::color(accent));
    }

    if (lines.empty())
    {
        lines.push_back(ftxui::text(""));
    }

    return ftxui::vbox(std::move(lines)) | ftxui::borderRounded | ftxui::bgcolor(style.background);
}
}

ToastHostComponent::ToastHostComponent(ftxui::Component child, ToastHostOption option) : _option(option)
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
    ToastId id            = 0;
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
    _toasts.erase(std::remove_if(_toasts.begin(), _toasts.end(), [now](const ToastState& toast) { return toast.expires_at.has_value() && *toast.expires_at <= now; }), _toasts.end());
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
    class ReflectedToastStack : public ftxui::Node
    {
    public:
        ReflectedToastStack(std::vector<ToastState> toasts, ToastHostOption option) : _toasts(std::move(toasts)), _option(std::move(option)) { }

        void ComputeRequirement() override
        {
            requirement_             = ftxui::Requirement {};
            requirement_.flex_grow_x = 1;
            requirement_.flex_grow_y = 1;
        }

        void SetBox(ftxui::Box box) override
        {
            Node::SetBox(box);

            // Use FTXUI's assigned layout box as the reflected available area.
            children_.clear();
            children_.push_back(build_layout(box));
            layout_element(children_[0], box);
        }

    private:
        std::size_t first_visible_index(ftxui::Box box) const
        {
            const int available_height = box_height(box);
            if (_toasts.empty() || available_height <= 0)
            {
                return _toasts.size();
            }

            const int max_visible     = std::max(1, _option.max_visible);
            const int toast_width     = std::min(std::max(20, _option.width), box_width(box));
            int remaining_height      = available_height;
            std::size_t visible_count = 0;

            for (std::size_t reverse_index = _toasts.size(); reverse_index > 0 && visible_count < static_cast<std::size_t>(max_visible); --reverse_index)
            {
                const int toast_height = std::max(1, fitted_height(render_toast_option(_toasts[reverse_index - 1].option, _option.style), toast_width));
                if (toast_height > remaining_height && visible_count > 0)
                {
                    break;
                }

                ++visible_count;
                remaining_height -= toast_height;
                if (remaining_height <= 0)
                {
                    break;
                }
            }

            return _toasts.size() - visible_count;
        }

        ftxui::Element build_layout(ftxui::Box box) const
        {
            ftxui::Elements rendered;
            const std::size_t first = first_visible_index(box);
            for (std::size_t index = first; index < _toasts.size(); ++index)
            {
                rendered.push_back(render_toast_option(_toasts[index].option, _option.style) | ftxui::clear_under);
            }

            auto stack = ftxui::vbox(std::move(rendered)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, std::max(20, _option.width));
            return ftxui::vbox({
                       ftxui::hbox({ftxui::filler(), stack}),
                       ftxui::filler(),
                   }) |
                   ftxui::flex;
        }

        std::vector<ToastState> _toasts;
        ToastHostOption _option;
    };

    return std::make_shared<ReflectedToastStack>(toasts, _option);
}

ftxui::Element ToastHostComponent::render_toast(const ToastState& toast) const
{
    return render_toast_option(toast.option, _option.style);
}

ftxui::Component ToastHost(ftxui::Component child, ToastHostOption option)
{
    return std::make_shared<ToastHostComponent>(std::move(child), option);
}
