#include <ftxui_components/text_view_component.hpp>

#include <algorithm>
#include <utility>

#include <ftxui/dom/canvas.hpp>

TextViewComponent::TextViewComponent(TextViewComponentOption option)
    : _draw_content(std::move(option.draw_content)), _title(std::move(option.title)), _on_selected_line_changed(std::move(option.on_selected_line_changed)), _on_selected_line_submitted(std::move(option.on_selected_line_submitted))
{
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

    return body | ftxui::border;
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

std::optional<TextViewPosition> TextViewComponent::text_position_at(int x, int y) const
{
    return _view.text_position_at(_controller.render_data(), x, y);
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

void TextViewComponent::select_line_at(TextViewPosition position, bool keep_visible)
{
    set_selected_line(position.line_index, keep_visible, -1);
}

void TextViewComponent::select_previous()
{
    move_selected_line(-_selector_step);
}

void TextViewComponent::select_next()
{
    move_selected_line(_selector_step);
}

void TextViewComponent::page_selected_up()
{
    if (!_selected_line.has_value())
    {
        return;
    }

    const int page_step = std::max(1, _controller.viewport_line_count() - 1);
    set_selected_line(*_selected_line - page_step, true, -1);
}

void TextViewComponent::page_selected_down()
{
    if (!_selected_line.has_value())
    {
        return;
    }

    const int page_step = std::max(1, _controller.viewport_line_count() - 1);
    set_selected_line(*_selected_line + page_step, true, 1);
}

void TextViewComponent::select_first_line()
{
    set_selected_line(0, true);
}

void TextViewComponent::select_last_line()
{
    set_selected_line(max_selectable_line(), true, -1);
}

void TextViewComponent::submit_selected_line() const
{
    if (_selected_line.has_value() && _on_selected_line_submitted)
    {
        _on_selected_line_submitted(*_selected_line);
    }
}

void TextViewComponent::scroll_up(int amount)
{
    _controller.scroll_up(amount);
}

void TextViewComponent::scroll_down(int amount)
{
    _controller.scroll_down(amount);
}

void TextViewComponent::page_up()
{
    _controller.page_up();
}

void TextViewComponent::page_down()
{
    _controller.page_down();
}

void TextViewComponent::scroll_to_top()
{
    _controller.scroll_to_top();
}

void TextViewComponent::scroll_to_bottom()
{
    _controller.scroll_to_bottom();
}

void TextViewComponent::scroll_left(int amount)
{
    _controller.scroll_left(amount);
}

void TextViewComponent::scroll_right(int amount)
{
    _controller.scroll_right(amount);
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

    _selected_line                = std::clamp(*_selected_line, 0, max_selected_line());
    _selected_line                = align_selected_line(*_selected_line, -1);
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

    const int max_line           = max_selected_line();
    const int max_selectable     = max_selectable_line();
    const int clamped_line       = std::clamp(line_index, 0, max_line);
    const int selector_remainder = clamped_line % _selector_step;
    int aligned_line             = clamped_line - selector_remainder;

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
