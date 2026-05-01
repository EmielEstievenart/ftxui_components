#include <ftxui_components/text_view_component.hpp>

#include <utility>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/dom/canvas.hpp>

TextViewComponent::TextViewComponent(TextViewComponentOption option)
    : _draw_content(std::move(option.draw_content))
    , _title(std::move(option.title))
{
    _controller.set_content(option.total_line_count, option.max_line_width, std::move(option.line_at));
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

TextViewController& TextViewComponent::controller()
{
    return _controller;
}

const TextViewController& TextViewComponent::controller() const
{
    return _controller;
}

ftxui::Component TextView(TextViewComponentOption option)
{
    return std::make_shared<TextViewComponent>(std::move(option));
}
