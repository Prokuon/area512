class SlideShow
  def parse_markdown_rows(markdown_content)
    source_lines = markdown_content.split("\n")
    markdown_rows = []
    code_block = false

    source_lines.each do |source_line|
      if source_line == "```ruby"
        code_block = true
        next
      end
      if source_line == "```" && code_block
        code_block = false
        next
      end

      row_kind = :paragraph
      row_text = source_line

      if code_block
        row_kind = :code
      elsif source_line.start_with?("###")
        row_kind = :heading_3
        row_text = source_line.slice(3, source_line.length - 3).lstrip
      elsif source_line.start_with?("##")
        row_kind = :heading_2
        row_text = source_line.slice(2, source_line.length - 2).lstrip
      elsif source_line.start_with?("#")
        row_kind = :heading_1
        row_text = source_line.slice(1, source_line.length - 1).lstrip
      elsif source_line.start_with?("-")
        row_kind = :list
        row_text = source_line.slice(1, source_line.length - 1).lstrip
      end

      row_segments = if row_kind == :code
        [[:text, row_text]]
      else
        parse_inline_segments(row_text)
      end
      markdown_rows.push([row_kind, row_segments])
    end

    markdown_rows
  end

  def parse_inline_segments(row_text)
    segments = []
    segment_kind = :text
    segment_start_index = 0
    character_index = 0
    inline_code = false

    while character_index < row_text.length
      if row_text.slice(character_index, 1) == "`"
        segment_text = row_text.slice(
          segment_start_index,
          character_index - segment_start_index
        )
        segments.push([segment_kind, segment_text]) if segment_text.length > 0
        inline_code = !inline_code
        segment_kind = inline_code ? :inline_code : :text
        segment_start_index = character_index + 1
      end
      character_index += 1
    end

    segment_text = row_text.slice(
      segment_start_index,
      row_text.length - segment_start_index
    )
    segments.push([segment_kind, segment_text]) if segment_text.length > 0
    segments.push([:text, ""]) if segments.length == 0
    segments
  end
end
