#ifndef CLK_INPUT_BOX_H
#define CLK_INPUT_BOX_H

#include <stdbool.h>
#include <stddef.h>

#include "clk_key_io.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct clk_input_box clk_input_box;

#define CLK_INPUT_BOX_WIDTH 50
#define CLK_INPUT_BOX_HEIGHT 5

/** Create an overlay text-input box.
 *  @p initial  pre-filled text (NULL = empty)
 *  @p max_length  maximum byte length, excluding NUL */
clk_input_box* clk_input_box_create(const char* initial, size_t max_length);

void clk_input_box_destroy(clk_input_box* box);

/** Expose buffer pointers for clk_key_io_set_input(). */
void clk_input_box_get_buffer(clk_input_box* box, char** buf, size_t* max, size_t** len,
                              size_t** pos);

/** Process a key event from clk_input_get_key_event().
 *  @return true when editing is finished (ENTER or ESC pressed). */
bool clk_input_box_handle_input(clk_input_box* box, clk_key_event event);

/** After finish: true = confirmed (ENTER), false = cancelled (ESC). */
bool clk_input_box_is_confirmed(const clk_input_box* box);

/** Get the edited text.  Valid only after handle_input returns true. */
const char* clk_input_box_get_result(const clk_input_box* box);

/** Draw the box, border, prompt, text, and position the hardware cursor. */
void clk_input_box_render(clk_input_box* box);

void clk_input_box_set_position(clk_input_box* box, int x, int y);
void clk_input_box_set_z_order(clk_input_box* box, int z);
void clk_input_box_add_to_term(clk_input_box* box);
void clk_input_box_remove_from_term(clk_input_box* box);

#ifdef __cplusplus
}
#endif

#endif
