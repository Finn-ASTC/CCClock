#include <stdio.h>
#include <string.h>

#include "clk_term.h"
#include "test_utils.h"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    /* ================================================================
     *  Texture create / destroy (no terminal needed)
     *
     *  Tests: correct dimensions, zero-size rejection, all cells
     *  initially empty, destroy safety.  Pure memory operations.
     * ================================================================ */

    /* normal create */
    clk_texture tex = clk_texture_create(10, 5);
    TEST("texture_create data non-null", tex.data != NULL);
    TEST("texture_create width", tex.tex_w == 10);
    TEST("texture_create height", tex.tex_h == 5);

    /* all cells initially empty */
    bool all_empty = true;
    for (int i = 0; i < tex.tex_w * tex.tex_h; ++i) {
        if (!tex.data[i].is_empty) {
            all_empty = false;
            break;
        }
    }
    TEST("all cells initially empty", all_empty);

    /* zero width/height returns data=NULL */
    clk_texture bad_w = clk_texture_create(0, 5);
    clk_texture bad_h = clk_texture_create(5, 0);
    TEST("create w=0 returns no data", bad_w.data == NULL);
    TEST("create h=0 returns no data", bad_h.data == NULL);

    /* destroy safety */
    clk_texture tex_d = clk_texture_create(3, 3);
    clk_texture_destroy(&tex_d);
    TEST("destroy sets data to NULL", tex_d.data == NULL);
    TEST("destroy resets size", tex_d.tex_w == 0 && tex_d.tex_h == 0);
    clk_texture_destroy(&tex_d); /* double destroy no crash */
    TEST("double destroy doesn't crash", 1);
    clk_texture_destroy(NULL); /* NULL destroy no crash */
    TEST("destroy NULL doesn't crash", 1);

    /* ================================================================
     *  clk_texture_init_borrowed — external cell array
     *
     *  Verifies: borrowed texture reads/writes correctly,
     *  destroy() does NOT free external data (owns_data=false).
     *  Otherwise double-free or use-after-free would occur.
     * ================================================================ */

    /* allocate borrowed cell data */
    clk_cell* borrowed_cells = malloc(12 * sizeof(clk_cell));
    memset(borrowed_cells, 0, 12 * sizeof(clk_cell));
    for (int i = 0; i < 12; i++)
        borrowed_cells[i].is_empty = true;

    clk_texture btex;
    clk_texture_init_borrowed(&btex, 3, 4, borrowed_cells);
    TEST("borrowed texture w==3", btex.tex_w == 3);
    TEST("borrowed texture h==4", btex.tex_h == 4);
    TEST("borrowed data points to ours", btex.data == borrowed_cells);

    /* write a cell — verify side-effect on external array */
    clk_texture_write_cell(&btex, 1, 2, "Z", 99);
    TEST("borrowed set_cell reflected externally",
         !borrowed_cells[1 + 2 * 3].is_empty &&
             strcmp(borrowed_cells[1 + 2 * 3].cell_tex, "Z") == 0);

    /* destroy borrowed — must NOT free external data */
    clk_texture_destroy(&btex);
    TEST("borrowed destroy clears data pointer", btex.data == NULL);
    TEST("borrowed destroy doesn't free external data",
         !borrowed_cells[1 + 2 * 3].is_empty); /* data still intact */

    /* double destroy no crash */
    clk_texture_destroy(&btex);
    TEST("borrowed double destroy no crash", 1);

    free(borrowed_cells);

    /* ================================================================
     *  set_cell — single cell write (no terminal needed)
     *
     *  Tests: normal write, out-of-bounds no crash.
     *  Pure tex->data operation, no terminal required.
     * ================================================================ */

    clk_texture_write_cell(&tex, 2, 1, "X", 42);

    clk_cell* c = &tex.data[2 + 1 * tex.tex_w];
    TEST("set_cell char correct", strcmp(c->cell_tex, "X") == 0);
    TEST("set_cell style_id", c->style_id == 42);
    TEST("set_cell not empty", !c->is_empty);

    /* OOB no crash */
    clk_texture_write_cell(&tex, -1, 0, "!", 1);
    clk_texture_write_cell(&tex, 100, 0, "!", 1);
    clk_texture_write_cell(&tex, 0, -1, "!", 1);
    clk_texture_write_cell(&tex, 0, 100, "!", 1);
    TEST("out-of-bounds set_cell doesn't crash", 1);

    /* ================================================================
     *  fill_rect — rectangle fill (no terminal needed)
     *
     *  Tests: correct fill region, cells outside unaffected.
     * ================================================================ */

    clk_texture_fill_rect(&tex, 1, 1, 3, 2, "#", 99);

    /* check cells (1,1) through (3,2), 6 total */
    bool rect_ok = true;
    for (int dy = 1; dy <= 2; ++dy)
        for (int dx = 1; dx <= 3; ++dx) {
            c = &tex.data[dx + dy * tex.tex_w];
            if (strcmp(c->cell_tex, "#") != 0 || c->style_id != 99 || c->is_empty)
                rect_ok = false;
        }
    TEST("fill_rect fills correct cells", rect_ok);

    /* (0,0) is outside, should remain empty */
    c = &tex.data[0];
    TEST("fill_rect doesn't touch (0,0)", c->is_empty);

    /* invalid args */
    clk_texture_fill_rect(&tex, 0, 0, 0, 1, "#", 1);
    clk_texture_fill_rect(&tex, 0, 0, 1, 0, "#", 1);
    TEST("fill_rect w=0 or h=0 doesn't crash", 1);

    /* ================================================================
     *  write_string — string write (no terminal needed)
     *
     *  Tests: normal write, empty/NULL doesn't crash, UTF-8 not split.
     * ================================================================ */

    clk_texture_write_string(&tex, 0, 3, "ABC", 7);
    TEST("write_string[0] = A", strcmp(tex.data[0 + 3 * tex.tex_w].cell_tex, "A") == 0);
    TEST("write_string[1] = B", strcmp(tex.data[1 + 3 * tex.tex_w].cell_tex, "B") == 0);
    TEST("write_string[2] = C", strcmp(tex.data[2 + 3 * tex.tex_w].cell_tex, "C") == 0);
    TEST("write_string style", tex.data[0 + 3 * tex.tex_w].style_id == 7);

    /* empty string */
    clk_texture_write_string(&tex, 0, 4, "", 1);
    TEST("empty string doesn't crash", 1);

    /* NULL string */
    clk_texture_write_string(&tex, 0, 4, NULL, 1);
    TEST("NULL string doesn't crash", 1);

    /* ================================================================
     *  Wide character support (LEAD / TRAIL model)
     *
     *  CJK/emoji characters are stored as two cells: LEAD at (x,y)
     *  and TRAIL at (x+1,y). write_string auto-detects width and
     *  calls set_wide_cell internally. The TRAIL cell is skipped
     *  during rendering — only the LEAD cell produces ANSI output.
     * ================================================================ */

    /* write_string: CJK characters → width 2, col advances by 2 */
    clk_texture_write_string(&tex, 0, 4, "你好", 99);
    const clk_cell* wc = clk_texture_get_cell(&tex, 0, 4);
    TEST("wide '你' LEAD type", wc != NULL && wc->type == CELL_WIDE_LEAD);
    TEST("wide '你' not empty", !wc->is_empty);

    wc = clk_texture_get_cell(&tex, 1, 4);
    TEST("wide TRAIL at col 1", wc != NULL && wc->type == CELL_WIDE_TRAIL);

    wc = clk_texture_get_cell(&tex, 2, 4);
    TEST("wide '好' at col 2 (LEAD)", wc != NULL && wc->type == CELL_WIDE_LEAD);

    /* '好' should be at col 2, not col 1 — col jumped by 2 */
    TEST("wide char jumps 2 columns", strcmp(wc->cell_tex, "好") == 0);

    /* mixed ASCII + CJK */
    clk_texture_write_string(&tex, 0, 4, "A你B", 99);
    wc = clk_texture_get_cell(&tex, 0, 4); /* 'A' NORMAL */
    TEST("mixed 'A' NORMAL", wc != NULL && wc->type == CELL_NORMAL);
    wc = clk_texture_get_cell(&tex, 1, 4); /* '你' LEAD */
    TEST("mixed '你' LEAD", wc != NULL && wc->type == CELL_WIDE_LEAD);
    wc = clk_texture_get_cell(&tex, 2, 4); /* '你' TRAIL */
    TEST("mixed TRAIL after '你'", wc != NULL && wc->type == CELL_WIDE_TRAIL);
    wc = clk_texture_get_cell(&tex, 3, 4); /* 'B' NORMAL */
    TEST("mixed 'B' at col 3",
         wc != NULL && wc->type == CELL_NORMAL && strcmp(wc->cell_tex, "B") == 0);

    /* set_wide_cell boundary — x+1 out of bounds is rejected */
    clk_texture_write_wide_cell(&tex, tex.tex_w - 1, 0, "X", 99);
    wc = clk_texture_get_cell(&tex, tex.tex_w - 1, 0);
    TEST("set_wide at boundary rejected (still empty)", wc != NULL && wc->is_empty);

    /* set_wide with room — both LEAD and TRAIL written */
    clk_texture_write_wide_cell(&tex, 0, 0, "中", 99);
    wc = clk_texture_get_cell(&tex, 0, 0);
    TEST("set_wide LEAD written", wc != NULL && wc->type == CELL_WIDE_LEAD && !wc->is_empty);
    wc = clk_texture_get_cell(&tex, 1, 0);
    TEST("set_wide TRAIL written", wc != NULL && wc->type == CELL_WIDE_TRAIL && !wc->is_empty);

    /* set_cell overwriting a TRAIL cell — allowed (caller's responsibility) */
    clk_texture_write_cell(&tex, 1, 0, "!", 1);
    wc = clk_texture_get_cell(&tex, 1, 0);
    TEST("set_cell overwrites TRAIL",
         wc != NULL && wc->type == CELL_NORMAL && strcmp(wc->cell_tex, "!") == 0);
    /* LEAD at col 0 is now an orphan — draw should skip it */

    /* clear_cell — individual cells can be cleared regardless of type */
    clk_texture_write_wide_cell(&tex, 0, 0, "文", 99);
    clk_texture_clear_cell(&tex, 0, 0); /* clear LEAD */
    wc = clk_texture_get_cell(&tex, 0, 0);
    TEST("clear LEAD after set_wide", wc != NULL && wc->is_empty);
    wc = clk_texture_get_cell(&tex, 1, 0);
    TEST("TRAIL still present after clearing LEAD", wc != NULL && !wc->is_empty);

    /* ================================================================
     *  clear_cell / clear_all (no terminal needed)
     * ================================================================ */

    clk_texture_clear_cell(&tex, 2, 1);
    TEST("clear_cell marks empty", tex.data[2 + 1 * tex.tex_w].is_empty);

    clk_texture_clear_all(&tex);
    all_empty = true;
    for (int i = 0; i < tex.tex_w * tex.tex_h; ++i) {
        if (!tex.data[i].is_empty) {
            all_empty = false;
            break;
        }
    }
    TEST("clear_all makes all cells empty", all_empty);

    /* ================================================================
     *  Sprite — position and z-order
     *
     *  Position and visibility have been split from clk_texture to clk_sprite.
     *  Sprite fields can be read/written directly; only z_order changes
     *  need the setter (because it resets the render list sort flag).
     * ================================================================ */

    clk_sprite sp = {.tex = &tex, .posx = 5, .posy = 10, .z_order = 3};
    TEST("sprite posx", sp.posx == 5);
    TEST("sprite posy", sp.posy == 10);
    TEST("sprite z_order", sp.z_order == 3);
    TEST("sprite not invalid default", !sp.is_invalid);

    sp.is_invalid = true;
    TEST("sprite set invalid", sp.is_invalid);

    clk_sprite_set_z(&sp, 9);
    TEST("sprite_set_z", sp.z_order == 9);

    clk_sprite_set_z(NULL, 0);
    TEST("sprite_set_z NULL no crash", 1);

    /* ================================================================
     *  Tests below require clk_term_init (real terminal needed)
     * ================================================================ */

    bool skip = false;
    if (!clk_term_init()) {
        printf("  [SKIP] No terminal — remaining tests skipped\n");
        skip = true;
        goto test_cleanup;
    }

    /* ---- style registry ---- */
    int red_id = clk_term_register_style((Color24){.rgb = {255, 0, 0}}, (Color24){0}, ATTR_BOLD);
    int green_id = clk_term_register_style((Color24){.rgb = {0, 255, 0}}, (Color24){0}, ATTR_NONE);
    int blue_id = clk_term_register_style((Color24){.rgb = {0, 0, 255}}, (Color24){0}, ATTR_DIM);

    TEST("style id > 0", red_id > 0 && green_id > 0 && blue_id > 0);
    TEST("unique style IDs", red_id != green_id && green_id != blue_id);

    /* dedup */
    int dedup = clk_term_register_style((Color24){.rgb = {255, 0, 0}}, (Color24){0}, ATTR_BOLD);
    TEST("dedup returns same id", dedup == red_id);

    /* ---- register_style_rgb convenience API ---- */
    int rgb_id = clk_term_register_style_rgb(100, 200, 50, 0, 0, 0, "bold italic");
    TEST("register_style_rgb id > 0", rgb_id > 0);
    TEST("register_style_rgb different from red", rgb_id != red_id);

    /* dedup via rgb convenience */
    int rgb_dedup = clk_term_register_style_rgb(100, 200, 50, 0, 0, 0, "bold italic");
    TEST("register_style_rgb dedup", rgb_dedup == rgb_id);

    /* out-of-range colour values → 0 */
    int bad = clk_term_register_style_rgb(-1, 0, 0, 0, 0, 0, NULL);
    TEST("register_style_rgb negative r fails", bad == 0);
    bad = clk_term_register_style_rgb(256, 0, 0, 0, 0, 0, NULL);
    TEST("register_style_rgb >255 r fails", bad == 0);

    /* ---- register_style_hex convenience API ---- */
    int hex_id = clk_term_register_style_hex("#FF8800", "#000000", "bold");
    TEST("register_style_hex id > 0", hex_id > 0);
    TEST("register_style_hex different from red", hex_id != red_id);

    /* dedup */
    int hex_dedup = clk_term_register_style_hex("#FF8800", "#000000", "bold");
    TEST("register_style_hex dedup", hex_dedup == hex_id);

    /* same colour different attrs should be different */
    int hex_diff = clk_term_register_style_hex("#FF8800", "#000000", "dim");
    TEST("register_style_hex different attrs", hex_diff != hex_id && hex_diff > 0);

    /* invalid hex */
    int hex_bad = clk_term_register_style_hex("no-hash", "#000000", NULL);
    TEST("register_style_hex bad format fails", hex_bad == 0);
    hex_bad = clk_term_register_style_hex("#ZZZZZZ", "#000000", NULL);
    TEST("register_style_hex invalid hex chars fails", hex_bad == 0);

    /* ---- re-test set_cell style association with real style_id ---- */
    clk_texture_write_cell(&tex, 5, 2, "Y", red_id);
    c = &tex.data[5 + 2 * tex.tex_w];
    TEST("set_cell with real style_id", !c->is_empty && c->style_id == red_id);

    /* ---- render list ---- */
    clk_texture tex2 = clk_texture_create(3, 3);
    if (tex2.data) {
        clk_texture_write_cell(&tex2, 1, 1, "O", green_id);
        clk_sprite s2 = {.tex = &tex2, .z_order = 0};
        clk_term_add_sprite(&s2);
        TEST("term_add_sprite succeeds", 1);
    }
    {
        clk_term_add_sprite(NULL);
        TEST("add NULL no crash", 1);
    }

    clk_texture_destroy(&tex2);

    /* ---- draw no crash (output not verified) ---- */
    clk_term_draw();
    TEST("clk_term_draw doesn't crash", 1);

    clk_term_close();

    /* ---- re-init reset ---- */
    if (clk_term_init()) {
        TEST("re-init after close succeeds", 1);
        clk_term_close();
    } else {
        TEST("re-init after close succeeds", 0); /* may be unavailable in ctest pipeline */
    }

test_cleanup:
    clk_texture_destroy(&tex);
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
