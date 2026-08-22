// Core filer declarations: layout constants, the filer state struct, and
// the ACTION_* codes run() returns to the Ruby shell (must stay in sync with
// the Ruby side). VM-independent: no mruby/c here.
#pragma once

#include <stdint.h>

#define FILER_CHAR_WIDTH 6
#define ROW_HEIGHT 13
#define HALF_ROW_HEIGHT 6
#define FILER_FONT_SIZE 12

#define NAME_MAX 40
#define MESSAGE_MAX 96
#define CURRENT_DIRECTORY_MAX 256
#define MAX_ENTRIES 256
#define LINE_MAX 96

#define ENTRY_TYPE_UP 0
#define ENTRY_TYPE_DIR 1
#define ENTRY_TYPE_FILE 2

#define ACTION_OPEN_DIR 1
#define ACTION_UP 2
#define ACTION_RUN_RUBY 3
#define ACTION_COMPILE 4
#define ACTION_COMPILE_ALL 5
#define ACTION_RUN_DIR 6
#define ACTION_EDIT 7
#define ACTION_NEW_FILE 8
#define ACTION_NEW_DIR 9
#define ACTION_DELETE 10
#define ACTION_REBOOT 11
#define ACTION_MOVE 12
#define ACTION_VIEW_MARKDOWN 13
#define ACTION_RUN_PYTHON 14
#define ACTION_COPY 15

#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_RIGHT 1003
#define KEY_LEFT 1004
#define KEY_COMPILE 1006
#define KEY_COMPILE_ALL 1007
#define KEY_EDIT 1008
#define KEY_RUN_DIR 1009
#define KEY_DELETE 1010
#define KEY_NEW_FILE 1011
#define KEY_NEW_DIR 1012
#define KEY_REBOOT 1013
#define KEY_MOVE 1014
#define KEY_COPY 1015

typedef struct {
  char name[NAME_MAX];
  uint8_t type;
} FileEntry;

typedef struct {
  void *row;
  int width, height;
  int content_x, content_right, columns, edge_columns;
  int list_top, rows_visible, bar1_y, bar2_y, close_y;
  int panel_x, panel_right, list_columns_panel;
  int panel_top_row;
  int index, top, count, full_redraw;
  char current_directory[CURRENT_DIRECTORY_MAX];
  char message[MESSAGE_MAX];
  char input[NAME_MAX];
  FileEntry entries[MAX_ENTRIES];
} Filer;

void init_filer_state(Filer *filer);
void clamp_index(Filer *filer);
void move_cursor(Filer *filer, int delta);
void jump_to(Filer *filer, int offset);
FileEntry *fetch_selected_entry(Filer *filer);
int is_selected_editable(Filer *filer);
int is_selected_markdown_file(Filer *filer);
int is_selected_ruby_file(Filer *filer);
int is_selected_python_file(Filer *filer);
int is_selected_source_file(Filer *filer);

void fit_string(
  char *destination,
  int destination_size,
  const char *source,
  int width
);

int area512_filer_read_key(void);
int read_raw_text_key(void);
void area512_filer_setup_ui(Filer *filer);
void area512_filer_teardown_ui(Filer *filer);

int read_text_input(Filer *filer, const char *label);
int read_yes_no_confirmation(Filer *filer, const char *question);
int run_filer_interaction(Filer *filer);
void draw_all(Filer *filer);
