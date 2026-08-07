class SlideShow
  def numeric_markdown_paths
    directory_entries = []
    markdown_paths = []

    directory = Dir.open("/home/tool/slide")
    begin
      while entry_name = directory.read
        directory_entries.push(entry_name) unless entry_name == "." || entry_name == ".."
      end
    ensure
      directory.close if directory
    end

    directory_entries.each do |entry_name|
      next unless entry_name.end_with?(".md")

      number_text = entry_name.slice(0, entry_name.length - 3)
      next unless number_text.to_i.to_s == number_text

      markdown_paths.push("/home/tool/slide/" + entry_name)
    end

    markdown_paths.sort! do |left_markdown_path, right_markdown_path|
      left_markdown_path.split("/")[-1].to_i <=>
        right_markdown_path.split("/")[-1].to_i
    end
    markdown_paths
  end

  def load_pages
    @pages = []
    @error_message = nil
    markdown_path = ""

    numeric_markdown_paths.each do |markdown_path|
      markdown_content = SD.read(markdown_path)
      markdown_rows = parse_markdown_rows(markdown_content)
      @pages.push([markdown_path, markdown_rows])
    end

    if @pages.length == 0
      @error_message = "No numbered .md files"
    end
  rescue => error_message
    @error_message = error_message.message
  end
end
