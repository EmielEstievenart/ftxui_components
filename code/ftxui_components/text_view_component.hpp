#pragma once

#include <functional>
#include <memory>
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
};

class TextViewComponent : public ftxui::ComponentBase
{
public:
    explicit TextViewComponent(TextViewComponentOption option);

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override;

    [[nodiscard]] bool focused() const;
    [[nodiscard]] TextViewController& controller();
    [[nodiscard]] const TextViewController& controller() const;

private:
    TextViewController _controller;
    TextViewView _view;
    TextViewView::RenderCallback _draw_content;
    std::string _title;
    ftxui::Box _box;
    ftxui::CapturedMouse _captured_mouse;
};

ftxui::Component TextView(TextViewComponentOption option);
