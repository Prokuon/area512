#include "core/hover/hover_popup.h"
#include "area512_hal.h"
#include "core/render/footer.h"
#include "core/render/signature.h"
#include "core/text/utf8.h"
#include "port/area512_editor_host.h"
#include <string.h>

static void
show_hover_document(
  Vim *vim,
  const char *document,
  int popup_horizontal_scroll_column_count
) {

  int document_byte_length = (int)strlen(document);

  int document_start_byte_offset =
    vim_column_to_byte(
      document,
      document_byte_length,
      popup_horizontal_scroll_column_count
    );

  show_message(
    vim,
    document + document_start_byte_offset,
    document_byte_length - document_start_byte_offset
  );

  if (vim->screen.footer)
    vim->screen.footer(vim->screen.footer_context, vim->active_canvas);
}

static int
calculate_maximum_hover_horizontal_scroll_column_count(
  Vim *vim,
  const char *signature,
  int signature_byte_length,
  int signature_row_count,
  const char *document
) {

  int maximum_popup_horizontal_scroll_column_count =
    calculate_maximum_signature_horizontal_scroll_column_count(
      vim,
      signature,
      signature_byte_length,
      signature_row_count
    );

  if (!document)
    return maximum_popup_horizontal_scroll_column_count;

  int maximum_document_horizontal_scroll_column_count =
    vim_display_width(document, (int)strlen(document)) - vim->screen.width;

  if (
    maximum_document_horizontal_scroll_column_count >
    maximum_popup_horizontal_scroll_column_count
  ) {

    maximum_popup_horizontal_scroll_column_count =
      maximum_document_horizontal_scroll_column_count;
  }

  return maximum_popup_horizontal_scroll_column_count;
}

void
show_hover_popup(
  Vim *vim,
  const char *signature,
  int signature_byte_length,
  int name_byte_length,
  const char *document
) {

  if (!vim || !vim->active_canvas || !signature)
    return;

  int available_row_count =
    vim->screen.height - vim->screen.footer_height;

  if (available_row_count <= 0)
    return;

  int signature_row_count =
    count_signature_rows(signature, signature_byte_length);

  if (signature_row_count > available_row_count)
    signature_row_count = available_row_count;

  int screen_row =
    vim->screen.height - vim->screen.footer_height - signature_row_count;

  int popup_horizontal_scroll_column_count = 0;

  draw_signature_rows(
    vim,
    screen_row,
    signature_row_count,
    signature,
    signature_byte_length,
    name_byte_length,
    NULL,
    0,
    popup_horizontal_scroll_column_count
  );

  if (document)
    show_hover_document(
      vim,
      document,
      popup_horizontal_scroll_column_count
    );

  for (;;) {
    int key = area512_console_getch_block();

    if (key < 0)
      continue;

    if (key == 27) {
      char sequence[2];
      int sequence_length = read_escape_sequence(sequence);

      if (sequence_length >= 2 && sequence[0] == '[') {
        if (sequence[1] == 'C') {

          int maximum_popup_horizontal_scroll_column_count =
            calculate_maximum_hover_horizontal_scroll_column_count(
              vim,
              signature,
              signature_byte_length,
              signature_row_count,
              document
            );

          if (
            popup_horizontal_scroll_column_count <
            maximum_popup_horizontal_scroll_column_count
          ) {

            popup_horizontal_scroll_column_count++;
          }

          draw_signature_rows(
            vim,
            screen_row,
            signature_row_count,
            signature,
            signature_byte_length,
            name_byte_length,
            NULL,
            0,
            popup_horizontal_scroll_column_count
          );

          if (document)
            show_hover_document(
              vim,
              document,
              popup_horizontal_scroll_column_count
            );

          continue;
        }

        if (sequence[1] == 'D') {
          if (popup_horizontal_scroll_column_count > 0)
            popup_horizontal_scroll_column_count--;

          draw_signature_rows(
            vim,
            screen_row,
            signature_row_count,
            signature,
            signature_byte_length,
            name_byte_length,
            NULL,
            0,
            popup_horizontal_scroll_column_count
          );

          if (document)
            show_hover_document(
              vim,
              document,
              popup_horizontal_scroll_column_count
            );

          continue;
        }
      }
    }

    break;
  }

  REDRAW(VIM_REDRAW_ALL);
}
