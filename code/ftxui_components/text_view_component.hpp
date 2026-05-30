#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>

#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_view.hpp>

struct TextViewComponentOption
{
    TextViewView::RenderCallback draw_content;           // Renders the text view content area.
    std::string title;                                   // Optional title shown around the text view.
    std::function<void(int)> on_selected_line_changed;   // Called after the selected line changes.
    std::function<void(int)> on_selected_line_submitted; // Called when the selected line is submitted.
};

class TextViewComponent : public ftxui::ComponentBase
{
public:
    explicit TextViewComponent(TextViewComponentOption option);

    ftxui::Element OnRender() override;

    [[nodiscard]] std::optional<int> selected_line() const;
    [[nodiscard]] TextViewController& controller();
    [[nodiscard]] const TextViewController& controller() const;
    [[nodiscard]] std::optional<TextViewPosition> text_position_at(int x, int y) const;
    // Configuration
    void update_content_size(int total_line_count, int max_line_width);
    void set_selectable(bool selectable);
    void set_selector_step(int selector_step);
    void set_selected_line(int line_index, bool keep_visible);

    // User interaction
    void user_select_line_at(TextViewPosition position, bool keep_visible = true);
    void user_select_previous();
    void user_select_next();
    void user_page_selected_up();
    void user_page_selected_down();
    void user_select_first_line();
    void user_select_last_line();
    void user_submit_selected_line() const;
    void user_move_selected_line(int delta);
    void user_scroll_up(int amount = 1);
    void user_scroll_down(int amount = 1);
    void user_page_up();
    void user_page_down();
    void user_scroll_to_top();
    void user_scroll_to_bottom();
    void user_scroll_left(int amount = 1);
    void user_scroll_right(int amount = 1);

private:
    void set_selected_line(int line_index, bool keep_visible, int alignment_direction);
    void keep_selected_line_visible();
    void notify_selected_line_changed() const;
    [[nodiscard]] int max_selected_line() const;
    [[nodiscard]] int max_selectable_line() const;
    [[nodiscard]] int align_selected_line(int line_index, int alignment_direction) const;

    TextViewController _controller;
    TextViewView _view;
    TextViewView::RenderCallback _draw_content;
    std::string _title;
    int _total_line_count = 0;
    bool _selectable      = false;
    int _selector_step    = 1;
    std::optional<int> _selected_line;
    std::function<void(int)> _on_selected_line_changed;
    std::function<void(int)> _on_selected_line_submitted;
};

ftxui::Component TextView(TextViewComponentOption option);
