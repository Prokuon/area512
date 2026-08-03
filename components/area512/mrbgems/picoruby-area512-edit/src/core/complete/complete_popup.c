#include "core/complete/complete_popup.h"
#include "area512_hal.h"
#include "core/render/footer.h"
#include "core/render/signature.h"
#include "core/text/utf8.h"
#include "port/area512_editor_host.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define COMPLETE_VISIBLE_ROWS 3

static void
show_complete_status(
  Vim *vim,
  const TiSuggestionList *suggestions,
  int selected_index,
  int popup_horizontal_scroll_column_count
) {

  char position[24];

  int position_length =
    snprintf(
      position,
      sizeof(position),
      "[%d/%d] ",
      selected_index + 1,
      suggestions->count
    );

  VimString message;

  vim_string_init(&message);
  vim_string_append(&message, position, position_length);

  const char *document = suggestions->items[selected_index].document;

  if (document) {
    int document_length = (int)strlen(document);

    int document_start_byte_offset =
      vim_column_to_byte(
        document,
        document_length,
        popup_horizontal_scroll_column_count
      );

    vim_string_append(
      &message,
      document + document_start_byte_offset,
      document_length - document_start_byte_offset
    );
  }

  show_message(vim, message.bytes, message.byte_length);
  vim_string_free(&message);

  if (vim->screen.footer)
    vim->screen.footer(vim->screen.footer_context, vim->active_canvas);
}

static int
count_complete_rows_between(
  const TiSuggestionList *suggestions,
  int from_index,
  int to_index
) {

  int rows = 0;

  for (int index = from_index; index <= to_index; index++)
    rows += count_signature_rows(
      suggestions->items[index].detail,
      (int)strlen(suggestions->items[index].detail)
    );

  return rows;
}

static void
draw_complete_popup(
  Vim *vim,
  const TiSuggestionList *suggestions,
  int selected_index,
  int *window_start,
  int popup_horizontal_scroll_column_count
) {

  int available_rows = vim->screen.height - vim->screen.footer_height;

  int content_budget =
    minimum_signature_value(COMPLETE_VISIBLE_ROWS, available_rows);

  if (content_budget <= 0)
    return;

  if (selected_index < *window_start)
    *window_start = selected_index;

  while (
    count_complete_rows_between(
       suggestions,
       *window_start,
       selected_index
     ) > content_budget
  ) {

    (*window_start)++;
  }

  int suggestion_rows[COMPLETE_VISIBLE_ROWS];
  int visible_count = 0;
  int used_rows = 0;

  for (
    int index = *window_start;
    index < suggestions->count && used_rows < content_budget;
    index++
  ) {

    int rows =
      count_signature_rows(
        suggestions->items[index].detail,
        (int)strlen(suggestions->items[index].detail)
      );

    if (rows > content_budget - used_rows)
      rows = content_budget - used_rows;

    suggestion_rows[visible_count++] = rows;
    used_rows += rows;

    if (visible_count == COMPLETE_VISIBLE_ROWS)
      break;
  }

  int screen_row = vim->screen.height - vim->screen.footer_height - used_rows;

  for (int offset = 0; offset < visible_count; offset++) {
    int suggestion_index = *window_start + offset;

    draw_signature_rows(
      vim,
      screen_row,
      suggestion_rows[offset],
      suggestions->items[suggestion_index].detail,
      (int)strlen(suggestions->items[suggestion_index].detail),
      suggestions->items[suggestion_index].contents_length,
      suggestions->items[suggestion_index].class_name,
      suggestion_index == selected_index,
      popup_horizontal_scroll_column_count
    );

    screen_row += suggestion_rows[offset];
  }

  show_complete_status(
    vim,
    suggestions,
    selected_index,
    popup_horizontal_scroll_column_count
  );
}

static int
calculate_maximum_complete_horizontal_scroll_column_count(
  Vim *vim,
  const TiSuggestionList *suggestions,
  int selected_index,
  int window_start
) {

  int maximum_popup_horizontal_scroll_column_count = 0;

  int available_popup_row_count =
    minimum_signature_value(
      COMPLETE_VISIBLE_ROWS,
      vim->screen.height - vim->screen.footer_height
    );

  int used_signature_row_count = 0;

  for (
    int suggestion_index = window_start;
    suggestion_index < suggestions->count &&
    used_signature_row_count < available_popup_row_count;
    suggestion_index++
  ) {

    const char *signature = suggestions->items[suggestion_index].detail;
    int signature_byte_length = (int)strlen(signature);

    int signature_row_count =
      count_signature_rows(
        signature,
        signature_byte_length
      );

    if (
      signature_row_count > available_popup_row_count - used_signature_row_count
    ) {

      signature_row_count =
        available_popup_row_count - used_signature_row_count;
    }

    if (signature_row_count <= 0)
      break;

    int maximum_signature_horizontal_scroll_column_count =
      calculate_maximum_signature_horizontal_scroll_column_count(
        vim,
        signature,
        signature_byte_length,
        signature_row_count
      );

    if (
      maximum_signature_horizontal_scroll_column_count >
      maximum_popup_horizontal_scroll_column_count
    ) {

      maximum_popup_horizontal_scroll_column_count =
        maximum_signature_horizontal_scroll_column_count;
    }

    used_signature_row_count += signature_row_count;
  }

  const char *document = suggestions->items[selected_index].document;

  if (!document)
    return maximum_popup_horizontal_scroll_column_count;

  int document_width = vim_display_width(document, (int)strlen(document));

  char position[24];

  int position_width =
    snprintf(
      position,
      sizeof(position),
      "[%d/%d] ",
      selected_index + 1,
      suggestions->count
    );

  int visible_width = vim->screen.width - position_width;

  if (visible_width < 1)
    visible_width = 1;

  int maximum_document_horizontal_scroll_column_count =
    document_width - visible_width;

  if (
    maximum_document_horizontal_scroll_column_count >
    maximum_popup_horizontal_scroll_column_count
  ) {

    maximum_popup_horizontal_scroll_column_count =
      maximum_document_horizontal_scroll_column_count;
  }

  return maximum_popup_horizontal_scroll_column_count;
}

static int
read_complete_printable(
  int first_byte,
  char character[4],
  int *character_byte_length
) {

  int byte_length = vim_utf8_byte_length((uint8_t)first_byte);

  if (byte_length < 1)
    byte_length = 1;

  if (byte_length > 4)
    byte_length = 4;

  character[0] = (char)first_byte;

  for (int index = 1; index < byte_length; index++) {
    int continuation_byte = area512_console_getch_block();

    if (continuation_byte < 0)
      return 0;

    character[index] = (char)continuation_byte;
  }

  *character_byte_length = byte_length;
  return 1;
}

static void
remove_complete_prefix(Vim *vim) {
  VimString *line = &BUFFER->lines[BUFFER->cursor_line_index];

  while (BUFFER->cursor_byte_offset > 0) {
    int previous_offset = BUFFER->cursor_byte_offset - 1;
    uint8_t byte = (uint8_t)line->bytes[previous_offset];

    if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
          (byte >= '0' && byte <= '9') || byte == '_' || byte == '!' ||
          byte == '?')) {
      break;
    }

    vim_buffer_put_key(BUFFER, VIM_PUT_BACKSPACE);
  }
}

int
show_complete_popup(
  Vim *vim,
  const TiSuggestionList *suggestions,
  int *next_key,
  char next_character[4],
  int *next_character_byte_length
) {
  if (!vim || !vim->active_canvas || !suggestions || suggestions->count <= 0)
    return 0;

  int selected_index = 0;
  int window_start = 0;
  int popup_horizontal_scroll_column_count = 0;

  draw_complete_popup(
    vim,
    suggestions,
    selected_index,
    &window_start,
    popup_horizontal_scroll_column_count
  );

  for (;;) {
    int key = area512_console_getch_block();

    if (key < 0)
      continue;

    if (key == 14) {
      selected_index = (selected_index + 1) % suggestions->count;

      popup_horizontal_scroll_column_count = 0;

      draw_complete_popup(
        vim,
        suggestions,
        selected_index,
        &window_start,
        popup_horizontal_scroll_column_count
      );

      continue;
    }

    if (key == 10 || key == 13) {
      const TiSuggestion *suggestion = &suggestions->items[selected_index];

      remove_complete_prefix(vim);

      vim_buffer_put_string(
        BUFFER,
        suggestion->contents,
        suggestion->contents_length
      );

      REDRAW(VIM_REDRAW_ALL);

      return 0;
    }

    if (key == 27) {
      char sequence[2];
      int sequence_length = read_escape_sequence(sequence);

      if (sequence_length >= 2 && sequence[0] == '[') {
        if (sequence[1] == 'A') {
          selected_index--;

          if (selected_index < 0)
            selected_index = suggestions->count - 1;

          popup_horizontal_scroll_column_count = 0;

          draw_complete_popup(
            vim,
            suggestions,
            selected_index,
            &window_start,
            popup_horizontal_scroll_column_count
          );

          continue;
        }

        if (sequence[1] == 'B') {
          selected_index = (selected_index + 1) % suggestions->count;
          popup_horizontal_scroll_column_count = 0;
          draw_complete_popup(
            vim,
            suggestions,
            selected_index,
            &window_start,
            popup_horizontal_scroll_column_count
          );
          continue;
        }

        if (sequence[1] == 'C') {
          int maximum_popup_horizontal_scroll_column_count =
            calculate_maximum_complete_horizontal_scroll_column_count(
              vim,
              suggestions,
              selected_index,
              window_start
            );

          if (
            popup_horizontal_scroll_column_count <
            maximum_popup_horizontal_scroll_column_count
          ) {
            popup_horizontal_scroll_column_count++;
          }

          draw_complete_popup(
            vim,
            suggestions,
            selected_index,
            &window_start,
            popup_horizontal_scroll_column_count
          );
          continue;
        }

        if (sequence[1] == 'D') {
          if (popup_horizontal_scroll_column_count > 0)
            popup_horizontal_scroll_column_count--;

          draw_complete_popup(
            vim,
            suggestions,
            selected_index,
            &window_start,
            popup_horizontal_scroll_column_count
          );
          continue;
        }
      }

      REDRAW(VIM_REDRAW_ALL);
      return 0;
    }

    if (key == 8 || key == 127) {
      *next_key = key;
      *next_character_byte_length = 0;
      REDRAW(VIM_REDRAW_ALL);
      return 1;
    }

    if (key >= 32 && read_complete_printable(
                       key,
                       next_character,
                       next_character_byte_length
                     )) {
      *next_key = key;
      REDRAW(VIM_REDRAW_ALL);
      return 1;
    }
  }
}
