#include <ftxui_components/text_view_component.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
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

TextViewView::RenderCallback draw_lines(const std::vector<std::string>& lines)
{
    return [&lines](ftxui::Canvas& canvas, int first_line_index, int line_count, int first_column, int column_count)
    {
        for (int row = 0; row < line_count; ++row)
        {
            const std::string& line = lines.at(static_cast<std::size_t>(first_line_index + row));
            if (first_column >= static_cast<int>(line.size()))
            {
                continue;
            }

            const auto count = static_cast<std::size_t>(std::min(column_count, static_cast<int>(line.size()) - first_column));
            canvas.DrawText(0, row * 4, line.substr(static_cast<std::size_t>(first_column), count));
        }
    };
}

std::shared_ptr<TextViewComponent> make_text_view(const std::string& title, const std::vector<std::string>& lines, ftxui::Color highlight_color)
{
    return std::make_shared<TextViewComponent>(TextViewComponentOption {
        static_cast<int>(lines.size()),
        max_line_width(lines),
        [&lines](int line_index) -> const std::string& { return lines.at(static_cast<std::size_t>(line_index)); },
        draw_lines(lines),
        title,
        [highlight_color](TextViewController& controller) { controller.set_background_column_range(5, 16, highlight_color); },
    });
}

ftxui::Element focus_label(const std::string& label, const TextViewComponent& view)
{
    return ftxui::text((view.focused() ? "Active: " : "Inactive: ") + label) | (view.focused() ? ftxui::bold : ftxui::dim);
}
}

int main()
{
    const std::vector<std::string> top_lines    = make_lines("Top", 100);
    const std::vector<std::string> bottom_lines = make_lines("Bottom", 100);

    auto top_view    = make_text_view("Top text view", top_lines, ftxui::Color::Blue);
    auto bottom_view = make_text_view("Bottom text view", bottom_lines, ftxui::Color::Green);

    auto panes = ftxui::Container::Vertical({
        top_view,
        bottom_view,
    });

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto component = ftxui::Renderer(panes,
                                     [&]
                                     {
                                         return ftxui::vbox({
                                                    ftxui::text("ftxui_components TextView focus example") | ftxui::bold,
                                                    ftxui::text("Tab/click changes the active text view. The bottom view is constrained narrower to demonstrate resizing. Press Esc or q to exit."),
                                                    ftxui::separator(),
                                                    focus_label("top", *top_view),
                                                    top_view->Render() | ftxui::flex,
                                                    ftxui::separator(),
                                                    ftxui::hbox({
                                                        ftxui::filler(),
                                                        ftxui::vbox({
                                                            focus_label("bottom", *bottom_view),
                                                            bottom_view->Render() | ftxui::flex,
                                                        }) | ftxui::flex | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 56),
                                                        ftxui::filler(),
                                                    }) | ftxui::flex,
                                                }) |
                                                ftxui::border;
                                     });

    component |= ftxui::CatchEvent([&](ftxui::Event event)
                                   {
                                       if (event == ftxui::Event::Escape || event == ftxui::Event::Character('q'))
                                       {
                                           screen.ExitLoopClosure()();
                                           return true;
                                       }

                                       return false;
                                   });

    screen.Loop(component);
    return 0;
}
