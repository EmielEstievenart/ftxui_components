#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>

#include <ftxui_components/text_view_controller.hpp>
#include <ftxui_components/text_view_view.hpp>

struct TextViewComponentOption
{
    int total_line_count = 0;
    int max_line_width   = 0;
    TextViewController::LineAccessor line_at;
    TextViewView::RenderCallback draw_content;
    std::string title;
    std::function<void(TextViewController&)> configure_controller;
    bool selectable = false;
    std::function<void(int)> on_selected_line_changed;
    std::function<void(int)> on_selected_line_submitted;
};

class TextViewComponent : public ftxui::ComponentBase
{
public:
    explicit TextViewComponent(TextViewComponentOption option);

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override;

    [[nodiscard]] bool focused() const;
    [[nodiscard]] std::optional<int> selected_line() const;
    [[nodiscard]] TextViewController& controller();
    [[nodiscard]] const TextViewController& controller() const;

private:
    bool handle_mouse_focus(ftxui::Event event);
    bool handle_selectable_event(ftxui::Event event);
    bool handle_text_view_event(ftxui::Event event);
    void set_selected_line(int line_index, bool keep_visible);
    void move_selected_line(int delta);
    void keep_selected_line_visible();
    void notify_selected_line_changed() const;
    [[nodiscard]] int max_selected_line() const;

    TextViewController _controller;
    TextViewView _view;
    TextViewView::RenderCallback _draw_content;
    std::string _title;
    int _total_line_count = 0;
    bool _selectable      = false;
    std::optional<int> _selected_line;
    std::function<void(int)> _on_selected_line_changed;
    std::function<void(int)> _on_selected_line_submitted;
    ftxui::Box _box;
    ftxui::CapturedMouse _captured_mouse;
};

ftxui::Component TextView(TextViewComponentOption option);
