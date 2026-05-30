#include <ftxui_components/text_view_component.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
std::vector<std::string> make_lines(const std::string& label, int count)
{
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index)
    {
        lines.push_back(label + " line " + std::to_string(index + 1) + " - Tab/click switches focus; arrows, PageUp/PageDown, Home/End scroll the active view.");
    }
    return lines;
}

int max_line_width(const std::vector<std::string>& lines)
{
    int width = 0;
    for (const auto& line : lines)
    {
        width = std::max(width, static_cast<int>(line.size()));
    }
    return width;
}

TextViewView::RenderCallback draw_lines(const std::vector<std::string>& lines, const std::optional<int>* selected_line = nullptr)
{
    return [&lines, selected_line](ftxui::Canvas& canvas, int first_line_index, int line_count, int first_column, int column_count)
    {
        for (int row = 0; row < line_count; ++row)
        {
            const int line_index    = first_line_index + row;
            const std::string& line = lines.at(static_cast<std::size_t>(first_line_index + row));
            const bool is_selected  = selected_line != nullptr && selected_line->has_value() && **selected_line == line_index;

            if (is_selected)
            {
                for (int col = 0; col < column_count; ++col)
                {
                    canvas.Style(col * 2, row * 4,
                                 [](ftxui::Cell& cell)
                                 {
                                     cell.background_color = ftxui::Color::Yellow;
                                     cell.foreground_color = ftxui::Color::Black;
                                     cell.bold             = true;
                                 });
                }
            }

            if (first_column >= static_cast<int>(line.size()))
            {
                continue;
            }

            const auto count = static_cast<std::size_t>(std::min(column_count, static_cast<int>(line.size()) - first_column));
            if (is_selected)
            {
                canvas.DrawText(0, row * 4, line.substr(static_cast<std::size_t>(first_column), count),
                                [](ftxui::Cell& cell)
                                {
                                    cell.background_color = ftxui::Color::Yellow;
                                    cell.foreground_color = ftxui::Color::Black;
                                    cell.bold             = true;
                                });
            }
            else
            {
                canvas.DrawText(0, row * 4, line.substr(static_cast<std::size_t>(first_column), count));
            }
        }
    };
}

std::shared_ptr<TextViewComponent> make_text_view(const std::string& title, const std::vector<std::string>& lines, ftxui::Color highlight_color, const std::optional<int>* selected_line = nullptr,
                                                  std::function<void(int)> on_selected_line_submitted = {})
{
    TextViewComponentOption option;
    option.draw_content               = draw_lines(lines, selected_line);
    option.title                      = title;
    option.on_selected_line_submitted = std::move(on_selected_line_submitted);
    auto view                         = std::make_shared<TextViewComponent>(std::move(option));
    view->controller().set_background_column_range(5, 16, highlight_color);
    view->set_selectable(selected_line != nullptr);
    view->update_content_size(static_cast<int>(lines.size()), max_line_width(lines));
    return view;
}

ftxui::Element focus_label(const std::string& label, bool active)
{
    return ftxui::text((active ? "Active: " : "Inactive: ") + label) | (active ? ftxui::bold : ftxui::dim);
}
}

int main()
{
    const std::vector<std::string> top_lines    = make_lines("Top", 100);
    const std::vector<std::string> bottom_lines = make_lines("Bottom", 100);
    std::optional<int> bottom_selected_line;
    std::optional<int> bottom_submitted_line;

    auto top_view    = make_text_view("Top text view", top_lines, ftxui::Color::Blue);
    auto bottom_view = make_text_view("Bottom text view", bottom_lines, ftxui::Color::Green, &bottom_selected_line, [&](int line_index) { bottom_submitted_line = line_index; });
    bool top_active  = true;

    auto scroll_active_up = [&]
    {
        if (top_active)
        {
            top_view->user_scroll_up();
        }
        else
        {
            bottom_view->user_select_previous();
        }
    };

    auto scroll_active_down = [&]
    {
        if (top_active)
        {
            top_view->user_scroll_down();
        }
        else
        {
            bottom_view->user_select_next();
        }
    };

    auto screen    = ftxui::ScreenInteractive::Fullscreen();
    auto component = ftxui::Renderer(
        [&]
        {
            bottom_selected_line = bottom_view->selected_line();
            auto top_rendered    = top_view->Render() | ftxui::flex;
            auto bottom_rendered = bottom_view->Render() | ftxui::flex;

            return ftxui::vbox({
                       ftxui::text("ftxui_components TextView focus example") | ftxui::bold,
                       ftxui::text("Tab/click changes the active text view. The bottom view is constrained narrower to demonstrate resizing. Press Esc or q to exit."),
                       ftxui::text("Bottom view select mode: Up/Down moves the selected line, PageUp/PageDown jumps, Enter submits."),
                       ftxui::separator(),
                       focus_label("top", top_active),
                       top_rendered,
                       ftxui::separator(),
                       ftxui::text("Current bottom selection: " + (bottom_selected_line.has_value() ? bottom_lines.at(static_cast<std::size_t>(*bottom_selected_line)) : std::string("none"))),
                       ftxui::text("Submitted bottom selection: " + (bottom_submitted_line.has_value() ? bottom_lines.at(static_cast<std::size_t>(*bottom_submitted_line)) : std::string("none"))),
                       ftxui::hbox({
                           ftxui::filler(),
                           ftxui::vbox({
                               focus_label("bottom", !top_active),
                               bottom_rendered,
                           }) | ftxui::flex |
                               ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 56),
                           ftxui::filler(),
                       }) | ftxui::flex,
                   }) |
                   ftxui::border;
        });

    component |= ftxui::CatchEvent(
        [&](ftxui::Event event)
        {
            if (event == ftxui::Event::Escape || event == ftxui::Event::Character('q'))
            {
                screen.ExitLoopClosure()();
                return true;
            }

            if (event == ftxui::Event::Tab)
            {
                top_active = !top_active;
                return true;
            }

            if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k'))
            {
                scroll_active_up();
                return true;
            }

            if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j'))
            {
                scroll_active_down();
                return true;
            }

            if (event == ftxui::Event::PageUp)
            {
                if (top_active)
                {
                    top_view->user_page_up();
                }
                else
                {
                    bottom_view->user_page_selected_up();
                }
                return true;
            }

            if (event == ftxui::Event::PageDown)
            {
                if (top_active)
                {
                    top_view->user_page_down();
                }
                else
                {
                    bottom_view->user_page_selected_down();
                }
                return true;
            }

            if (event == ftxui::Event::Home)
            {
                if (top_active)
                {
                    top_view->user_scroll_to_top();
                }
                else
                {
                    bottom_view->user_select_first_line();
                }
                return true;
            }

            if (event == ftxui::Event::End)
            {
                if (top_active)
                {
                    top_view->user_scroll_to_bottom();
                }
                else
                {
                    bottom_view->user_select_last_line();
                }
                return true;
            }

            if (event == ftxui::Event::Return && !top_active)
            {
                bottom_view->user_submit_selected_line();
                return true;
            }

            if (event.is_mouse())
            {
                const auto mouse = event.mouse();
                if (mouse.button == ftxui::Mouse::WheelUp)
                {
                    scroll_active_up();
                    return true;
                }

                if (mouse.button == ftxui::Mouse::WheelDown)
                {
                    scroll_active_down();
                    return true;
                }

                if (mouse.button == ftxui::Mouse::Left && mouse.motion == ftxui::Mouse::Pressed)
                {
                    if (top_view->text_position_at(mouse.x, mouse.y).has_value())
                    {
                        top_active = true;
                        return true;
                    }

                    const auto bottom_position = bottom_view->text_position_at(mouse.x, mouse.y);
                    if (bottom_position.has_value())
                    {
                        top_active = false;
                        bottom_view->user_select_line_at(*bottom_position);
                        return true;
                    }
                }
            }

            return false;
        });

    screen.Loop(component);
    return 0;
}
