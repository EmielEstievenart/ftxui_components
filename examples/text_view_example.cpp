#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_view.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>
#include <vector>

int main()
{
    std::vector<std::string> lines;
    lines.reserve(100);
    for (int index = 0; index < 100; ++index)
    {
        lines.push_back("Line " + std::to_string(index + 1) + " - scroll with arrows, PageUp/PageDown, Home/End, and drag to select text.");
    }

    int max_line_width = 0;
    for (const auto& line : lines)
    {
        max_line_width = std::max(max_line_width, static_cast<int>(line.size()));
    }

    TextViewController controller;
    TextViewView view;
    controller.set_content(static_cast<int>(lines.size()), max_line_width, [&lines](int line_index) -> const std::string& { return lines.at(static_cast<std::size_t>(line_index)); });
    controller.set_background_column_range(5, 12, ftxui::Color::Blue);

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto component = ftxui::Renderer([&]
                                     {
                                         controller.update_viewport_line_count(view.viewport_line_count());
                                         controller.update_viewport_col_count(view.viewport_col_count());

                                         const TextViewRenderData data = controller.render_data();
                                         return ftxui::vbox({
                                                    ftxui::text("ftxui_components TextView example") | ftxui::bold,
                                                    ftxui::text("Press Esc to exit. Selected text is copied with Ctrl+C."),
                                                    ftxui::separator(),
                                                    view.render(data,
                                                                [&lines](ftxui::Canvas& canvas, int first_line_index, int line_count, int first_column, int column_count)
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
                                                                }) |
                                                        ftxui::flex,
                                                }) |
                                                ftxui::border;
                                     });

    component |= ftxui::CatchEvent([&](ftxui::Event event)
                                   {
                                       if (event == ftxui::Event::Escape)
                                       {
                                           screen.ExitLoopClosure()();
                                           return true;
                                       }

                                       const TextViewRenderData data = controller.render_data();
                                       const TextViewEventResult result = controller.parse_event(event, [&](const ftxui::Mouse& mouse) { return view.mouse_to_text_position(data, mouse); });
                                       if (result.request_exit)
                                       {
                                           screen.ExitLoopClosure()();
                                       }
                                       return result.handled;
                                   });

    screen.Loop(component);
    return 0;
}
