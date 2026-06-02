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
     *  纹理创建与销毁（不依赖 clk_term_init）
     *
     *  测什么：正确尺寸、非法尺寸、初始全空、destroy 安全。
     *  create/destroy 是纯内存操作，不需要终端。
     * ================================================================ */

    /* 正常创建 */
    clk_texture tex = clk_texture_create(10, 5);
    TEST("texture_create data non-null", tex.data != NULL);
    TEST("texture_create width", tex.tex_w == 10);
    TEST("texture_create height", tex.tex_h == 5);

    /* 所有 cell 初始化为空 */
    bool all_empty = true;
    for (int i = 0; i < tex.tex_w * tex.tex_h; ++i) {
        if (!tex.data[i].is_empty) {
            all_empty = false;
            break;
        }
    }
    TEST("all cells initially empty", all_empty);

    /* 非法宽/高应返回 data=NULL */
    clk_texture bad_w = clk_texture_create(0, 5);
    clk_texture bad_h = clk_texture_create(5, 0);
    TEST("create w=0 returns no data", bad_w.data == NULL);
    TEST("create h=0 returns no data", bad_h.data == NULL);

    /* destroy 安全 */
    clk_texture tex_d = clk_texture_create(3, 3);
    clk_texture_destroy(&tex_d);
    TEST("destroy sets data to NULL", tex_d.data == NULL);
    TEST("destroy resets size", tex_d.tex_w == 0 && tex_d.tex_h == 0);
    clk_texture_destroy(&tex_d); /* double destroy 不崩 */
    TEST("double destroy doesn't crash", 1);
    clk_texture_destroy(NULL); /* NULL destroy 不崩 */
    TEST("destroy NULL doesn't crash", 1);

    /* ================================================================
     *  set_cell —— 单格写入（不依赖终端）
     *
     *  测什么：正常写入、越界不崩。
     *  set_cell 只操作 tex->data 指针，不需要终端。
     * ================================================================ */

    clk_texture_set_cell(&tex, 2, 1, "X", 42);

    clk_cell* c = &tex.data[2 + 1 * tex.tex_w];
    TEST("set_cell char correct", strcmp(c->cell_tex, "X") == 0);
    TEST("set_cell style_id", c->style_id == 42);
    TEST("set_cell not empty", !c->is_empty);

    /* 越界不崩 */
    clk_texture_set_cell(&tex, -1, 0, "!", 1);
    clk_texture_set_cell(&tex, 100, 0, "!", 1);
    clk_texture_set_cell(&tex, 0, -1, "!", 1);
    clk_texture_set_cell(&tex, 0, 100, "!", 1);
    TEST("out-of-bounds set_cell doesn't crash", 1);

    /* ================================================================
     *  fill_rect —— 矩形填充（不依赖终端）
     *
     *  测什么：填充区正确、区域外不受影响。
     * ================================================================ */

    clk_texture_fill_rect(&tex, 1, 1, 3, 2, "#", 99);

    /* 检查 (1,1) ~ (3,2) 共 6 格 */
    bool rect_ok = true;
    for (int dy = 1; dy <= 2; ++dy)
        for (int dx = 1; dx <= 3; ++dx) {
            c = &tex.data[dx + dy * tex.tex_w];
            if (strcmp(c->cell_tex, "#") != 0 || c->style_id != 99 || c->is_empty)
                rect_ok = false;
        }
    TEST("fill_rect fills correct cells", rect_ok);

    /* (0,0) 在填充区外，应保持初始空状态 */
    c = &tex.data[0];
    TEST("fill_rect doesn't touch (0,0)", c->is_empty);

    /* 非法参数 */
    clk_texture_fill_rect(&tex, 0, 0, 0, 1, "#", 1);
    clk_texture_fill_rect(&tex, 0, 0, 1, 0, "#", 1);
    TEST("fill_rect w=0 or h=0 doesn't crash", 1);

    /* ================================================================
     *  write_string —— 字符串写入（不依赖终端）
     *
     *  测什么：正常写入、空串/NULL 不崩、UTF-8 不拆散。
     * ================================================================ */

    clk_texture_write_string(&tex, 0, 3, "ABC", 7);
    TEST("write_string[0] = A", strcmp(tex.data[0 + 3 * tex.tex_w].cell_tex, "A") == 0);
    TEST("write_string[1] = B", strcmp(tex.data[1 + 3 * tex.tex_w].cell_tex, "B") == 0);
    TEST("write_string[2] = C", strcmp(tex.data[2 + 3 * tex.tex_w].cell_tex, "C") == 0);
    TEST("write_string style", tex.data[0 + 3 * tex.tex_w].style_id == 7);

    /* 空字符串 */
    clk_texture_write_string(&tex, 0, 4, "", 1);
    TEST("empty string doesn't crash", 1);

    /* NULL 字符串 */
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
    clk_texture_set_wide_cell(&tex, tex.tex_w - 1, 0, "X", 99);
    wc = clk_texture_get_cell(&tex, tex.tex_w - 1, 0);
    TEST("set_wide at boundary rejected (still empty)", wc != NULL && wc->is_empty);

    /* set_wide with room — both LEAD and TRAIL written */
    clk_texture_set_wide_cell(&tex, 0, 0, "中", 99);
    wc = clk_texture_get_cell(&tex, 0, 0);
    TEST("set_wide LEAD written", wc != NULL && wc->type == CELL_WIDE_LEAD && !wc->is_empty);
    wc = clk_texture_get_cell(&tex, 1, 0);
    TEST("set_wide TRAIL written", wc != NULL && wc->type == CELL_WIDE_TRAIL && !wc->is_empty);

    /* set_cell overwriting a TRAIL cell — allowed (caller's responsibility) */
    clk_texture_set_cell(&tex, 1, 0, "!", 1);
    wc = clk_texture_get_cell(&tex, 1, 0);
    TEST("set_cell overwrites TRAIL",
         wc != NULL && wc->type == CELL_NORMAL && strcmp(wc->cell_tex, "!") == 0);
    /* LEAD at col 0 is now an orphan — draw should skip it */

    /* clear_cell — individual cells can be cleared regardless of type */
    clk_texture_set_wide_cell(&tex, 0, 0, "文", 99);
    clk_texture_clear_cell(&tex, 0, 0); /* clear LEAD */
    wc = clk_texture_get_cell(&tex, 0, 0);
    TEST("clear LEAD after set_wide", wc != NULL && wc->is_empty);
    wc = clk_texture_get_cell(&tex, 1, 0);
    TEST("TRAIL still present after clearing LEAD", wc != NULL && !wc->is_empty);

    /* ================================================================
     *  clear_cell / clear_all（不依赖终端）
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
     *  setter 函数（不依赖终端）
     * ================================================================ */

    clk_texture_set_pos(&tex, 5, 10);
    TEST("set_pos", tex.posx == 5 && tex.posy == 10);

    clk_texture_set_z_order(&tex, 3);
    TEST("set_z_order", tex.tex_z_order == 3);

    clk_texture_set_invalid(&tex, true);
    TEST("set_invalid true", tex.is_invalid);
    clk_texture_set_invalid(&tex, false);
    TEST("set_invalid false", !tex.is_invalid);

    clk_texture_set_pos_z(&tex, 1, 2, 3);
    TEST("set_pos_z", tex.posx == 1 && tex.posy == 2 && tex.tex_z_order == 3);

    /* NULL 不崩 */
    clk_texture_set_pos(NULL, 0, 0);
    clk_texture_set_z_order(NULL, 0);
    clk_texture_set_invalid(NULL, false);
    clk_texture_set_pos_z(NULL, 0, 0, 0);
    TEST("NULL setters don't crash", 1);

    /* ================================================================
     *  以下测试依赖 clk_term_init（需要真实终端）
     * ================================================================ */

    bool skip = false;
    if (!clk_term_init()) {
        printf("  [SKIP] No terminal — remaining tests skipped\n");
        skip = true;
        goto test_cleanup;
    }

    /* ---- 样式注册表 ---- */
    int red_id = clk_term_register_style((Color24){.rgb = {255, 0, 0}}, (Color24){0}, ATTR_BOLD);
    int green_id = clk_term_register_style((Color24){.rgb = {0, 255, 0}}, (Color24){0}, ATTR_NONE);
    int blue_id = clk_term_register_style((Color24){.rgb = {0, 0, 255}}, (Color24){0}, ATTR_DIM);

    TEST("style id > 0", red_id > 0 && green_id > 0 && blue_id > 0);
    TEST("unique style IDs", red_id != green_id && green_id != blue_id);

    /* 去重 */
    int dedup = clk_term_register_style((Color24){.rgb = {255, 0, 0}}, (Color24){0}, ATTR_BOLD);
    TEST("dedup returns same id", dedup == red_id);

    /* ---- 用真实 style_id 重新测 set_cell 的样式关联 ---- */
    clk_texture_set_cell(&tex, 5, 2, "Y", red_id);
    c = &tex.data[5 + 2 * tex.tex_w];
    TEST("set_cell with real style_id", !c->is_empty && c->style_id == red_id);

    /* ---- render list ---- */
    clk_texture tex2 = clk_texture_create(3, 3);
    if (tex2.data) {
        clk_texture_set_cell(&tex2, 1, 1, "O", green_id);
        TEST("term_add_texture", clk_term_add_texture(&tex2));
    }
    TEST("add NULL returns false", !clk_term_add_texture(NULL));

    clk_texture_destroy(&tex2);

    /* ---- draw 不崩溃（不验证输出） ---- */
    clk_term_draw();
    TEST("clk_term_draw doesn't crash", 1);

    clk_term_close();

    /* ---- 再 init 验证 reset ---- */
    if (clk_term_init()) {
        TEST("re-init after close succeeds", 1);
        clk_term_close();
    } else {
        TEST("re-init after close succeeds", 0); /* ctest 管道里可能一直不可用 */
    }

test_cleanup:
    clk_texture_destroy(&tex);
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
