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
            const int line_index       = first_line_index + row;
            const std::string& line = lines.at(static_cast<std::size_t>(first_line_index + row));
            const bool is_selected     = selected_line != nullptr && selected_line->has_value() && **selected_line == line_index;

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

std::shared_ptr<TextViewComponent> make_text_view(const std::string& title,
                                                  const std::vector<std::string>& lines,
                                                  ftxui::Color highlight_color,
                                                  const std::optional<int>* selected_line = nullptr,
                                                  std::function<void(int)> on_selected_line_submitted = {})
{
    TextViewComponentOption option;
    option.total_line_count          = static_cast<int>(lines.size());
    option.max_line_width            = max_line_width(lines);
    option.line_at                   = [&lines](int line_index) -> const std::string& { return lines.at(static_cast<std::size_t>(line_index)); };
    option.draw_content              = draw_lines(lines, selected_line);
    option.title                     = title;
    option.configure_controller      = [highlight_color](TextViewController& controller) { controller.set_background_column_range(5, 16, highlight_color); };
    option.selectable                = selected_line != nullptr;
    option.on_selected_line_submitted = std::move(on_selected_line_submitted);
    return std::make_shared<TextViewComponent>(std::move(option));
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
    std::optional<int> bottom_selected_line;
    std::optional<int> bottom_submitted_line;

    auto top_view    = make_text_view("Top text view", top_lines, ftxui::Color::Blue);
    auto bottom_view = make_text_view("Bottom text view",
                                      bottom_lines,
                                      ftxui::Color::Green,
                                      &bottom_selected_line,
                                      [&](int line_index) { bottom_submitted_line = line_index; });

    auto panes = ftxui::Container::Vertical({
        top_view,
        bottom_view,
    });

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto component = ftxui::Renderer(panes,
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
                                                    focus_label("top", *top_view),
                                                    top_rendered,
                                                    ftxui::separator(),
                                                    ftxui::text("Current bottom selection: " + (bottom_selected_line.has_value() ? bottom_lines.at(static_cast<std::size_t>(*bottom_selected_line)) : std::string("none"))),
                                                    ftxui::text("Submitted bottom selection: " + (bottom_submitted_line.has_value() ? bottom_lines.at(static_cast<std::size_t>(*bottom_submitted_line)) : std::string("none"))),
                                                    ftxui::hbox({
                                                        ftxui::filler(),
                                                        ftxui::vbox({
                                                            focus_label("bottom", *bottom_view),
                                                            bottom_rendered,
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
