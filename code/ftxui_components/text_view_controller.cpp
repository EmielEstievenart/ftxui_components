#include <ftxui_components/text_view_controller.hpp>

#include <ftxui_components/clipboard.hpp>

#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

bool is_before(const TextViewPosition& lhs, const TextViewPosition& rhs)
{
    return lhs.line_index < rhs.line_index || (lhs.line_index == rhs.line_index && lhs.column < rhs.column);
}

} // namespace

// --- Content management ---

void TextViewController::set_content(int total_line_count, int max_line_width, TextViewController::LineAccessor line_at)
{
    _total_line_count = std::max(0, total_line_count);
    _max_line_width   = std::max(0, max_line_width);
    _line_at          = std::move(line_at);
    clear_selection();
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

// --- Text selection ---

void TextViewController::begin_selection(TextViewPosition position)
{
    _selection_anchor      = clamp_selection_position(position);
    _selection_focus       = _selection_anchor;
    _selection_in_progress = _selection_anchor.has_value();
}

void TextViewController::update_selection(TextViewPosition position)
{
    if (!_selection_in_progress || !_selection_anchor.has_value())
    {
        return;
    }
    _selection_focus = clamp_selection_position(position);
}

void TextViewController::end_selection(std::optional<TextViewPosition> position)
{
    _selection_in_progress = false;
    if (position.has_value() && _selection_anchor.has_value())
    {
        _selection_focus = clamp_selection_position(*position);
    }
}

void TextViewController::clear_selection()
{
    _selection_anchor.reset();
    _selection_focus.reset();
    _selection_in_progress = false;
}

bool TextViewController::selection_in_progress() const
{
    return _selection_in_progress;
}

std::optional<std::pair<TextViewPosition, TextViewPosition>> TextViewController::selection_bounds() const
{
    if (!_selection_anchor.has_value() || !_selection_focus.has_value() || _total_line_count == 0)
    {
        return std::nullopt;
    }

    auto start = clamp_selection_position(*_selection_anchor);
    auto end   = clamp_selection_position(*_selection_focus);
    if (is_before(end, start))
    {
        std::swap(start, end);
    }
    return std::pair(start, end);
}

std::string TextViewController::selection_text() const
{
    const auto bounds = selection_bounds();
    if (!bounds.has_value())
    {
        return {};
    }

    const auto [start, end] = *bounds;
    std::ostringstream output;
    for (int line_index = start.line_index; line_index <= end.line_index; ++line_index)
    {
        const auto& line           = line_at(line_index);
        const int line_start       = (line_index == start.line_index) ? start.column : 0;
        const int line_end         = (line_index == end.line_index) ? end.column : static_cast<int>(line.size());
        const int clamped_start    = std::clamp(line_start, 0, static_cast<int>(line.size()));
        const int clamped_end      = std::clamp(line_end, clamped_start, static_cast<int>(line.size()));
        const auto selection_count = static_cast<std::size_t>(clamped_end - clamped_start);

        output << line.substr(static_cast<std::size_t>(clamped_start), selection_count);
        if (line_index != end.line_index)
        {
            output << '\n';
        }
    }

    return output.str();
}

// --- Clipboard ---

bool TextViewController::copy_selection_to_clipboard() const
{
    return CopyTextToClipboard(selection_text());
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

    const int end = std::min(total, first + safe_viewport);

    // Selection decorations
    const auto selected_range = selection_bounds();
    if (selected_range.has_value())
    {
        for (int line_index = selected_range->first.line_index; line_index <= selected_range->second.line_index; ++line_index)
        {
            if (line_index < first || line_index >= end)
            {
                continue;
            }

            const auto& line          = line_at(line_index);
            const int selection_start = (line_index == selected_range->first.line_index) ? selected_range->first.column : 0;
            const int selection_end   = (line_index == selected_range->second.line_index) ? selected_range->second.column : static_cast<int>(line.size());
            const int clamped_start   = std::clamp(selection_start, 0, static_cast<int>(line.size()));
            const int clamped_end     = std::clamp(selection_end, clamped_start, static_cast<int>(line.size()));
            if (clamped_start == clamped_end)
            {
                continue;
            }

            TextViewStyle style;
            style.inverted = true;

            TextViewRangeDecoration decoration;
            decoration.line_index = line_index;
            decoration.col_start  = clamped_start;
            decoration.col_end    = clamped_end;
            decoration.style      = style;
            data.range_decorations.push_back(decoration);
        }
    }

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

TextViewPosition TextViewController::clamp_selection_position(TextViewPosition position) const
{
    if (_total_line_count == 0)
    {
        return TextViewPosition {0, 0};
    }

    position.line_index    = std::clamp(position.line_index, 0, _total_line_count - 1);
    const auto line_length = static_cast<int>(line_at(position.line_index).size());
    position.column        = std::clamp(position.column, 0, line_length);
    return position;
}

const std::string& TextViewController::line_at(int index) const
{
    if (!_line_at || index < 0 || index >= _total_line_count)
    {
        throw std::out_of_range("TextViewController::line_at index out of range");
    }

    return _line_at(index);
}
