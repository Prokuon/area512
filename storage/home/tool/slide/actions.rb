class SlideShow
  def handle_key(key)
    case key
    when "h" then previous_page
    when "l" then next_page
    when "k" then scroll_up
    when "j" then scroll_down
    when "ESC" then @quit = true
    end
  end

  def next_page
    return if @error_message
    return if @page_index >= @pages.length - 1

    @page_index += 1
    @top_row_index = 0
  end

  def previous_page
    return if @error_message
    return if @page_index == 0

    @page_index -= 1
    @top_row_index = 0
  end

  def scroll_down
    return if @error_message
    return if @top_row_index >= maximum_top_row_index

    @top_row_index += 1
  end

  def scroll_up
    return if @error_message
    return if @top_row_index == 0

    @top_row_index -= 1
  end

  def current_rows
    @pages[@page_index][1]
  end

  def maximum_top_row_index
    [current_rows.length - VISIBLE_ROW_COUNT, 0].max
  end
end
