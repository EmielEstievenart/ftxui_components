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
    , _on_selected_line_rendered(std::move(option.on_selected_line_rendered))
    , _on_selected_line_submitted(std::move(option.on_selected_line_submitted))
{
    _controller.set_content(option.total_line_count, option.max_line_width, std::move(option.line_at));
    if (_selectable && _total_line_count > 0)
    {
        _selected_line = 0;
        notify_selected_line_rendered();
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

    if (_selectable && _selected_line.has_value() && _on_selected_line_rendered)
    {
        notify_selected_line_rendered();
    }

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

    if (event.is_mouse())
    {
        const bool inside = _box.Contain(event.mouse().x, event.mouse().y);
        if (!inside && !_captured_mouse)
        {
            return false;
        }

        if (inside && event.mouse().motion == ftxui::Mouse::Pressed)
        {
            _captured_mouse = CaptureMouse(event);
            TakeFocus();
        }
    }

    if (_selectable && _selected_line.has_value())
    {
        const int page_step = std::max(1, _controller.viewport_line_count() - 1);
        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelUp)
        {
            _controller.scroll_up(1);
            set_selected_line(*_selected_line - 1, false);
            return true;
        }
        if (event.is_mouse() && event.mouse().button == ftxui::Mouse::WheelDown)
        {
            _controller.scroll_down(1);
            set_selected_line(*_selected_line + 1, false);
            return true;
        }
        if (event == ftxui::Event::ArrowUp)
        {
            move_selected_line(-1);
            return true;
        }
        if (event == ftxui::Event::ArrowDown)
        {
            move_selected_line(1);
            return true;
        }
        if (event == ftxui::Event::PageUp)
        {
            move_selected_line(-page_step);
            return true;
        }
        if (event == ftxui::Event::PageDown)
        {
            move_selected_line(page_step);
            return true;
        }
        if (event == ftxui::Event::Home)
        {
            set_selected_line(0, true);
            return true;
        }
        if (event == ftxui::Event::End)
        {
            set_selected_line(max_selected_line(), true);
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
                set_selected_line(position->line_index, true);
                return true;
            }
        }
    }

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

void TextViewComponent::move_selected_line(int delta)
{
    if (!_selected_line.has_value() || _total_line_count <= 0)
    {
        return;
    }

    set_selected_line(*_selected_line + delta, true);
}

void TextViewComponent::set_selected_line(int line_index, bool keep_visible)
{
    if (_total_line_count <= 0)
    {
        _selected_line.reset();
        return;
    }

    _selected_line = std::clamp(line_index, 0, max_selected_line());
    if (keep_visible)
    {
        keep_selected_line_visible();
    }
    notify_selected_line_rendered();
}

void TextViewComponent::keep_selected_line_visible()
{
    if (!_selected_line.has_value() || _total_line_count <= 0)
    {
        return;
    }

    _selected_line = std::clamp(*_selected_line, 0, max_selected_line());
    if (*_selected_line < _controller.first_visible_line())
    {
        _controller.center_on_line(*_selected_line);
        return;
    }

    const int last_visible_line = _controller.first_visible_line() + std::max(1, _controller.viewport_line_count()) - 1;
    if (*_selected_line > last_visible_line)
    {
        _controller.center_on_line(*_selected_line);
    }
}

int TextViewComponent::max_selected_line() const
{
    return std::max(0, _total_line_count - 1);
}

void TextViewComponent::notify_selected_line_rendered() const
{
    if (_selected_line.has_value() && _on_selected_line_rendered)
    {
        _on_selected_line_rendered(*_selected_line);
    }
}

ftxui::Component TextView(TextViewComponentOption option)
{
    return std::make_shared<TextViewComponent>(std::move(option));
}
