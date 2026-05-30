#include <ftxui_components/text_view_controller.hpp>

#include <algorithm>

// --- Content management ---

void TextViewController::set_content(int total_line_count, int max_line_width)
{
    _total_line_count = std::max(0, total_line_count);
    _max_line_width   = std::max(0, max_line_width);
    if (_follow_bottom)
    {
        _first_visible_line = max_first_visible_line();
    }
    else
    {
        clamp_scroll_position();
    }
    _first_visible_col = std::min(_first_visible_col, max_first_visible_col());
}

void TextViewController::update_content_size(int total_line_count, int max_line_width)
{
    _total_line_count = std::max(0, total_line_count);
    _max_line_width   = std::max(0, max_line_width);

    if (_follow_bottom)
    {
        _first_visible_line = max_first_visible_line();
    }
    else
    {
        clamp_scroll_position();
    }

    _first_visible_col = std::min(_first_visible_col, max_first_visible_col());
}

// --- Viewport ---

void TextViewController::update_viewport_line_count(int viewport_line_count)
{
    _viewport_line_count = std::max(1, viewport_line_count);
    if (_follow_bottom)
    {
        _first_visible_line = max_first_visible_line();
        return;
    }
    clamp_scroll_position();
}

void TextViewController::update_viewport_col_count(int viewport_col_count)
{
    _viewport_col_count = std::max(1, viewport_col_count);
    _first_visible_col  = std::min(_first_visible_col, max_first_visible_col());
}

// --- Scrolling ---

void TextViewController::scroll_up(int amount)
{
    const int step      = std::max(1, amount);
    _first_visible_line = std::max(0, _first_visible_line - step);
    _follow_bottom      = _first_visible_line >= max_first_visible_line();
}

void TextViewController::scroll_down(int amount)
{
    const int step      = std::max(1, amount);
    _first_visible_line = std::min(max_first_visible_line(), _first_visible_line + step);
    _follow_bottom      = _first_visible_line >= max_first_visible_line();
}

void TextViewController::page_up()
{
    scroll_up(std::max(1, normalize_viewport_line_count() - 1));
}

void TextViewController::page_down()
{
    scroll_down(std::max(1, normalize_viewport_line_count() - 1));
}

void TextViewController::scroll_to_top()
{
    _first_visible_line = 0;
    _follow_bottom      = false;
}

void TextViewController::scroll_to_bottom()
{
    _first_visible_line = max_first_visible_line();
    _follow_bottom      = true;
}

void TextViewController::scroll_left(int amount)
{
    const int step     = std::max(1, amount);
    _first_visible_col = std::max(0, _first_visible_col - step);
}

void TextViewController::scroll_right(int amount)
{
    const int step     = std::max(1, amount);
    _first_visible_col = std::min(max_first_visible_col(), _first_visible_col + step);
}

void TextViewController::center_on_line(int line_index)
{
    _first_visible_line = std::clamp(line_index - normalize_viewport_line_count() / 2, 0, max_first_visible_line());
    _follow_bottom      = _first_visible_line >= max_first_visible_line();
}

// --- Column highlight ---

void TextViewController::set_background_column_range(int col_start, int col_end, ftxui::Color color)
{
    _col_highlight.col_start = col_start;
    _col_highlight.col_end   = col_end;
    _col_highlight.color     = color;
    _col_highlight.active    = true;
}

void TextViewController::clear_background_column_range()
{
    _col_highlight.active = false;
}

// --- Render ---

TextViewRenderData TextViewController::render_data() const
{
    const int safe_viewport = normalize_viewport_line_count();
    const int total         = _total_line_count;
    const int max_first     = std::max(0, total - safe_viewport);
    const int first         = std::clamp(_first_visible_line, 0, max_first);

    const int safe_col_viewport = std::max(1, _viewport_col_count);
    const int first_col         = std::max(0, _first_visible_col);

    TextViewRenderData data;
    data.total_lines         = total;
    data.first_visible_line  = first;
    data.viewport_line_count = safe_viewport;
    data.first_visible_col   = first_col;
    data.max_line_width      = _max_line_width;
    data.viewport_col_count  = safe_col_viewport;
    data.col_highlight       = _col_highlight;

    return data;
}

// --- Accessors ---

int TextViewController::first_visible_line() const
{
    return std::clamp(_first_visible_line, 0, max_first_visible_line());
}

int TextViewController::first_visible_col() const
{
    return std::clamp(_first_visible_col, 0, max_first_visible_col());
}

bool TextViewController::follow_bottom() const
{
    return _follow_bottom;
}

int TextViewController::viewport_line_count() const
{
    return _viewport_line_count;
}

int TextViewController::viewport_col_count() const
{
    return _viewport_col_count;
}

// --- Private ---

int TextViewController::normalize_viewport_line_count() const
{
    return std::max(1, _viewport_line_count);
}

int TextViewController::max_first_visible_line() const
{
    return std::max(0, _total_line_count - normalize_viewport_line_count());
}

int TextViewController::max_first_visible_col() const
{
    return std::max(0, _max_line_width - _viewport_col_count);
}

void TextViewController::clamp_scroll_position()
{
    _first_visible_line = std::clamp(_first_visible_line, 0, max_first_visible_line());
}

