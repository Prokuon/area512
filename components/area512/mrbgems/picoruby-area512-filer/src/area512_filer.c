#if defined(PICORB_VM_MRUBYC)

#include "core/filer.h"

#include "area512_hal.h"
#include <stdbool.h>
#include "io-console.h"
#include <mrubyc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// -----------------------------------------------------------------------------
// mruby/c value glue
// -----------------------------------------------------------------------------

static Filer *
get_filer(mrbc_value *v) {
  return *(Filer **)v->instance->data;
}

static void
copy_string(char *destination, int destination_size, mrbc_value *string_value) {
  if (string_value->tt != MRBC_TT_STRING) {
    destination[0] = 0;
    return;
  }

  int length = string_value->string->size;

  if (length > destination_size - 1)
    length = destination_size - 1;

  memcpy(destination, string_value->string->data, length);

  destination[length] = 0;
}

// -----------------------------------------------------------------------------
// Methods
// -----------------------------------------------------------------------------

static void
c_filer_new(mrbc_vm *virtual_machine, mrbc_value *v, int argument_count) {
  (void)argument_count;

  Filer *filer = (Filer *)malloc(sizeof(Filer));

  if (filer == NULL) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(RuntimeError),
      "failed to allocate Filer"
    );

    return;
  }

  memset(filer, 0, sizeof(Filer));
  init_filer_state(filer);

  mrbc_value instance =
    mrbc_instance_new(virtual_machine, v->cls, sizeof(Filer *));

  *(Filer **)instance.instance->data = filer;

  SET_RETURN(instance);
}

static void
mrbc_filer_free(mrbc_value *self) {
  Filer *filer = *(Filer **)self->instance->data;

  if (filer) {
    if (filer->row)
      area512_sprite_delete(filer->row);

    free(filer);
  }
}

static void
c_filer_clear_entries(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {
  (void)virtual_machine;
  (void)argument_count;

  Filer *filer = get_filer(v);
  filer->count = 0;
}

static int
compare_entry_order(const FileEntry *left, const FileEntry *right) {
  if (left->type != right->type)
    return left->type < right->type ? -1 : 1;

  return strcasecmp(left->name, right->name);
}

// add_entry(name, type)
static void
c_filer_add_entry(mrbc_vm *virtual_machine, mrbc_value *v, int argument_count) {
  Filer *filer = get_filer(v);

  if (argument_count < 2) {
    mrbc_raise(
      virtual_machine,
      MRBC_CLASS(ArgumentError),
      "wrong number of arguments (expected 2)"
    );

    return;
  }

  if (filer->count >= MAX_ENTRIES)
    return;

  FileEntry entry;

  copy_string(entry.name, NAME_MAX, &v[1]);

  entry.type =
    (uint8_t)(v[2].tt == MRBC_TT_INTEGER ? v[2].i : ENTRY_TYPE_FILE);

  int position = filer->count;

  while (position > 0
      && compare_entry_order(&filer->entries[position - 1], &entry) > 0) {
    filer->entries[position] = filer->entries[position - 1];
    position--;
  }

  filer->entries[position] = entry;

  filer->count++;
}

static void
c_filer_set_current_directory(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {

  (void)virtual_machine;
  (void)argument_count;

  Filer *filer = get_filer(v);
  copy_string(filer->current_directory, CURRENT_DIRECTORY_MAX, &v[1]);
}

static void
c_filer_set_message(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {

  (void)virtual_machine;
  (void)argument_count;

  Filer *filer = get_filer(v);
  copy_string(filer->message, MESSAGE_MAX, &v[1]);
}

static void
c_filer_set_index(mrbc_vm *virtual_machine, mrbc_value *v, int argument_count) {
  (void)virtual_machine;
  (void)argument_count;

  Filer *filer = get_filer(v);

  if (v[1].tt == MRBC_TT_INTEGER)
    filer->index = (int)v[1].i;

  clamp_index(filer);
}

static void
c_filer_index(mrbc_vm *virtual_machine, mrbc_value *v, int argument_count) {
  (void)virtual_machine;
  (void)argument_count;

  SET_INT_RETURN(get_filer(v)->index);
}

static void
c_filer_count(mrbc_vm *virtual_machine, mrbc_value *v, int argument_count) {
  (void)virtual_machine;
  (void)argument_count;

  SET_INT_RETURN(get_filer(v)->count);
}

static void
c_filer_selected_name(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {

  (void)argument_count;

  Filer *filer = get_filer(v);
  const char *name = "";

  if (filer->count > 0 && filer->index >= 0 && filer->index < filer->count)
    name = filer->entries[filer->index].name;

  mrbc_value result = mrbc_string_new_cstr(virtual_machine, name);

  SET_RETURN(result);
}

static void
c_filer_selected_type(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {

  (void)virtual_machine;
  (void)argument_count;

  Filer *filer = get_filer(v);

  int type =
    (filer->count > 0) ? filer->entries[filer->index].type : ENTRY_TYPE_FILE;

  SET_INT_RETURN(type);
}

static void
c_filer_input_text(
  mrbc_vm *virtual_machine,
  mrbc_value *v,
  int argument_count
) {

  (void)argument_count;

  mrbc_value result =
    mrbc_string_new_cstr(virtual_machine, get_filer(v)->input);

  SET_RETURN(result);
}

static void
c_filer_run(mrbc_vm *virtual_machine, mrbc_value *v, int argument_count) {
  (void)virtual_machine;
  (void)argument_count;

  Filer *filer = get_filer(v);

  area512_filer_setup_ui(filer);
  clamp_index(filer);

  // Raw so Ctrl-C/Ctrl-Z reach the filer as bytes instead of becoming signals.
  io_raw_bang(false);

  int action = run_filer_interaction(filer);

  io_cooked_bang();

  SET_INT_RETURN(action);
}

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------

void
mrbc_area512_filer_init(mrbc_vm *virtual_machine) {
  mrbc_class *class_Filer =
    mrbc_define_class(virtual_machine, "Filer", mrbc_class_object);

  mrbc_define_destructor(class_Filer, mrbc_filer_free);
  mrbc_define_method(virtual_machine, class_Filer, "new", c_filer_new);

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "clear_entries",
    c_filer_clear_entries
  );

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "add_entry",
    c_filer_add_entry
  );

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "cwd=",
    c_filer_set_current_directory
  );

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "message=",
    c_filer_set_message
  );

  mrbc_define_method(virtual_machine, class_Filer, "index=", c_filer_set_index);
  mrbc_define_method(virtual_machine, class_Filer, "index", c_filer_index);
  mrbc_define_method(virtual_machine, class_Filer, "count", c_filer_count);

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "selected_name",
    c_filer_selected_name
  );

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "selected_type",
    c_filer_selected_type
  );

  mrbc_define_method(
    virtual_machine,
    class_Filer,
    "input_text",
    c_filer_input_text
  );

  mrbc_define_method(virtual_machine, class_Filer, "run", c_filer_run);
}

#elif defined(PICORB_VM_MRUBY)

#include <mruby.h>

void
mrb_picoruby_area512_filer_gem_init(mrb_state *state) {
  (void)state;
}

void
mrb_picoruby_area512_filer_gem_final(mrb_state *state) {
  (void)state;
}

#endif
