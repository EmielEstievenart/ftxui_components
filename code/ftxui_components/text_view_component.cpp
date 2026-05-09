#include <ftxui_components/text_view_component.hpp>

#include <algorithm>
#include <utility>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/dom/canvas.hpp>

TextViewComponent::TextViewComponent(TextViewComponentOption option)
    : _draw_content(std::move(option.draw_content))
    , _title(std::move(option.title))
    , _total_line_count(option.total_line_count)
    , _selectable(option.selectable)
    , _selector_step(std::max(1, option.selector_step))
    , _on_selected_line_changed(std::move(option.on_selected_line_changed))
    , _on_selected_line_submitted(std::move(option.on_selected_line_submitted))
{
    _controller.set_content(option.total_line_count, option.max_line_width, std::move(option.line_at));
    if (_selectable && _total_line_count > 0)
    {
        _selected_line = 0;
        notify_selected_line_changed();
    }

    if (option.configure_controller)
    {
        option.configure_controller(_controller);
    }
}

ftxui::Element TextViewComponent::OnRender()
{
    _controller.update_viewport_line_count(_view.viewport_line_count());
    _controller.update_viewport_col_count(_view.viewport_col_count());

    const TextViewRenderData data = _controller.render_data();
    ftxui::Element body           = _view.render(data, _draw_content) | ftxui::flex;

    if (!_title.empty())
    {
        body = ftxui::vbox({
                   ftxui::text(_title),
                   ftxui::separator(),
                   body,
               }) |
               ftxui::flex;
    }

    return body | ftxui::border | ftxui::reflect(_box);
}

bool TextViewComponent::OnEvent(ftxui::Event event)
{
    if (!event.is_mouse() && !Focused())
    {
        return false;
    }

    if (!handle_mouse_focus(event))
    {
        return false;
    }

    return handle_selectable_event(event) || handle_text_view_event(event);
}

bool TextViewComponent::handle_event(ftxui::Event event)
{
    return handle_selectable_event(event) || handle_text_view_event(event);
}

bool TextViewComponent::handle_mouse_focus(ftxui::Event event)
{
    if (!event.is_mouse())
    {
        return true;
    }

    const bool inside = _box.Contain(event.mouse().x, event.mouse().y);
    if (!inside && !_captured_mouse)
    {
        return false;
    }

    if (inside && event.mouse().button == ftxui::Mouse::Left && event.mouse().motion == ftxui::Mouse::Pressed)
    {
        _captured_mouse = CaptureMouse(event);
        TakeFocus();
    }

    return true;
}

bool TextViewComponent::handle_selectable_event(ftxui::Event event)
{
    if (_selectable && _selected_line.has_value())
    {
        const int page_step = std::max(1, _controller.viewport_line_count() - 1);
        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelUp)
        {
            _controller.scroll_up(_selector_step);
            set_selected_line(*_selected_line - _selector_step, false, -1);
            return true;
        }
        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelDown)
        {
            _controller.scroll_down(_selector_step);
            set_selected_line(*_selected_line + _selector_step, false, 1);
            return true;
        }
        if (event == ftxui::Event::ArrowUp)
        {
            move_selected_line(-_selector_step);
            return true;
        }
        if (event == ftxui::Event::ArrowDown)
        {
            move_selected_line(_selector_step);
            return true;
        }
        if (event == ftxui::Event::PageUp)
        {
            set_selected_line(*_selected_line - page_step, true, -1);
            return true;
        }
        if (event == ftxui::Event::PageDown)
        {
            set_selected_line(*_selected_line + page_step, true, 1);
            return true;
        }
        if (event == ftxui::Event::Home)
        {
            set_selected_line(0, true);
            return true;
        }
        if (event == ftxui::Event::End)
        {
            set_selected_line(max_selectable_line(), true, -1);
            return true;
        }
        if (event == ftxui::Event::Return)
        {
            if (_on_selected_line_submitted)
            {
                _on_selected_line_submitted(*_selected_line);
            }
            return true;
        }

        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left && event.mouse().motion == ftxui::Mouse::Pressed)
        {
            const TextViewRenderData data = _controller.render_data();
            const auto position           = _view.mouse_to_text_position(data, event.mouse());
            if (position.has_value())
            {
                set_selected_line(position->line_index, true, -1);
                return true;
            }
        }
    }

    return false;
}

bool TextViewComponent::handle_text_view_event(ftxui::Event event)
{
    const TextViewRenderData data = _controller.render_data();
    const TextViewEventResult result = _controller.parse_event(event, [&](const ftxui::Mouse& mouse) { return _view.mouse_to_text_position(data, mouse); });

    if (event.is_mouse() && event.mouse().motion == ftxui::Mouse::Released)
    {
        _captured_mouse.reset();
    }

    return result.handled;
}

bool TextViewComponent::Focusable() const
{
    return true;
}

bool TextViewComponent::focused() const
{
    return Focused();
}

std::optional<int> TextViewComponent::selected_line() const
{
    return _selected_line;
}

TextViewController& TextViewComponent::controller()
{
    return _controller;
}

const TextViewController& TextViewComponent::controller() const
{
    return _controller;
}

void TextViewComponent::update_content_size(int total_line_count, int max_line_width)
{
    _total_line_count = std::max(0, total_line_count);
    _controller.update_content_size(total_line_count, max_line_width);

    if (!_selectable || _total_line_count <= 0)
    {
        _selected_line.reset();
        return;
    }

    if (!_selected_line.has_value())
    {
        _selected_line = 0;
        notify_selected_line_changed();
        return;
    }

    set_selected_line(*_selected_line, true, -1);
}

void TextViewComponent::set_selectable(bool selectable)
{
    if (_selectable == selectable)
    {
        return;
    }

    _selectable = selectable;
    if (!_selectable || _total_line_count <= 0)
    {
        _selected_line.reset();
        return;
    }

    _selected_line = 0;
    notify_selected_line_changed();
}

void TextViewComponent::set_selector_step(int selector_step)
{
    _selector_step = std::max(1, selector_step);
    if (_selected_line.has_value())
    {
        set_selected_line(*_selected_line, true, -1);
    }
}

void TextViewComponent::set_selected_line(int line_index, bool keep_visible)
{
    set_selected_line(line_index, keep_visible, -1);
}

void TextViewComponent::move_selected_line(int delta)
{
    if (!_selected_line.has_value() || _total_line_count <= 0)
    {
        return;
    }

    set_selected_line(*_selected_line + delta, true, delta < 0 ? -1 : 1);
}

void TextViewComponent::set_selected_line(int line_index, bool keep_visible, int alignment_direction)
{
    if (_total_line_count <= 0)
    {
        _selected_line.reset();
        return;
    }

    _selected_line = align_selected_line(line_index, alignment_direction);
    if (keep_visible)
    {
        keep_selected_line_visible();
    }
    notify_selected_line_changed();
}

void TextViewComponent::keep_selected_line_visible()
{
    if (!_selected_line.has_value() || _total_line_count <= 0)
    {
        return;
    }

    _selected_line = std::clamp(*_selected_line, 0, max_selected_line());
    _selected_line = align_selected_line(*_selected_line, -1);
    const int selected_first_line = *_selected_line;
    const int selected_last_line  = std::min(max_selected_line(), selected_first_line + _selector_step - 1);

    if (selected_first_line < _controller.first_visible_line())
    {
        _controller.center_on_line(selected_first_line);
        return;
    }

    const int reflected_viewport_lines = _view.viewport_line_count();
    const int viewport_lines           = reflected_viewport_lines > 0 ? reflected_viewport_lines : _controller.viewport_line_count();
    const int last_visible_line        = _controller.first_visible_line() + std::max(1, viewport_lines) - 1;
    if (selected_last_line > last_visible_line)
    {
        _controller.center_on_line(selected_last_line);
    }
}

int TextViewComponent::max_selected_line() const
{
    return std::max(0, _total_line_count - 1);
}

int TextViewComponent::max_selectable_line() const
{
    const int max_line = max_selected_line();
    return max_line - (max_line % _selector_step);
}

int TextViewComponent::align_selected_line(int line_index, int alignment_direction) const
{
    if (_total_line_count <= 0)
    {
        return 0;
    }

    const int max_line            = max_selected_line();
    const int max_selectable      = max_selectable_line();
    const int clamped_line        = std::clamp(line_index, 0, max_line);
    const int selector_remainder  = clamped_line % _selector_step;
    int aligned_line              = clamped_line - selector_remainder;

    if (alignment_direction > 0 && selector_remainder != 0)
    {
        aligned_line += _selector_step;
    }

    return std::clamp(aligned_line, 0, max_selectable);
}

void TextViewComponent::notify_selected_line_changed() const
{
    if (_selected_line.has_value() && _on_selected_line_changed)
    {
        _on_selected_line_changed(*_selected_line);
    }
}

ftxui::Component TextView(TextViewComponentOption option)
{
    return std::make_shared<TextViewComponent>(std::move(option));
}
