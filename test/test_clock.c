#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clk_clock.h"
#include "test_utils.h"

#define TEST_FONT_PATH "assets/test_clock_config.json"

int main(void) {
    if (isatty_fd(fileno(stdout))) {
        g_pass = "  \033[32m[PASS]\033[0m ";
        g_fail = "  \033[31m[FAIL]\033[0m ";
    }

    clk_clock clk;

    /* ================================================================
     *  clk_clock_create —— 无效参数
     *
     *  测什么：NULL clock、NULL time_format、NULL font_path 都返回 false。
     *  为什么测：create 是把所有分配组合在一起的入口，如果参数校验
     *  漏了，内部会崩在 malloc 或 strdup 上。
     * ================================================================ */

    /* NULL clock */
    bool ok = clk_clock_create(NULL, "%H:%M:%S", TEST_FONT_PATH);
    TEST("clock_create NULL clock fails", !ok);

    /* NULL time format */
    ok = clk_clock_create(&clk, NULL, TEST_FONT_PATH);
    TEST("clock_create NULL format fails", !ok);

    /* NULL font path */
    ok = clk_clock_create(&clk, "%H:%M:%S", NULL);
    TEST("clock_create NULL font path fails", !ok);

    /* invalid font path */
    ok = clk_clock_create(&clk, "%H:%M:%S", "nonexistent.json");
    TEST("clock_create missing file fails", !ok);

    /* ================================================================
     *  clk_clock_create —— 正常创建
     *
     *  测什么：合法参数返回 true，内部 texture 非空且尺寸正确。
     *  为什么测：create 是链式操作——分配内存 → 解析 JSON →
     *  注册样式 → 建字形表 → 分配纹理。任何一个环节崩了，
     *  返回值或纹理尺寸就能抓出来。
     * ================================================================ */

    ok = clk_clock_create(&clk, "%H:%M:%S", TEST_FONT_PATH);
    TEST_REQUIRE("clock_create succeeds", ok);

    /* 纹理存在且非空 */
    const clk_texture* tex = clk_clock_get_texture(&clk);
    TEST_REQUIRE("clock_get_texture non-null", tex != NULL);
    TEST("clock texture data non-null", tex->data != NULL);

    /* 纹理尺寸：5 个数字 + 1 个冒号 + 5 个数字 + 1 个冒号 + 5 个数字
     *   8 个字形 × 5 列 = 40 列（每个字形间无间距）
     *   5 行高 */
    int tex_w, tex_h;
    TEST("clock_get_texture_size succeeds",
         clk_clock_get_texture_size(&clk, &tex_w, &tex_h));
    TEST("clock texture width > 0",  tex_w > 0);
    TEST("clock texture height == 5", tex_h == 5);

    /* ================================================================
     *  clk_clock_get_texture / get/set pos —— 纹理访问
     * ================================================================ */

    /* 默认位置应在屏幕居中 */
    int posx, posy;
    TEST("clock_get_texture_pos succeeds",
         clk_clock_get_texture_pos(&clk, &posx, &posy));

    /* 修改位置并验证 */
    TEST("clock_set_texture_pos succeeds",
         clk_clock_set_texture_pos(&clk, 10, 5));
    TEST("clock_get_texture_pos succeeds (2)",
         clk_clock_get_texture_pos(&clk, &posx, &posy));
    TEST("clock_set_texture_pos x == 10", posx == 10);
    TEST("clock_set_texture_pos y == 5",  posy == 5);

    /* NULL 参数保护 */
    TEST("clock_get_texture_pos NULL posx fails",
         !clk_clock_get_texture_pos(&clk, NULL, &posy));
    TEST("clock_get_texture_size NULL arg fails",
         !clk_clock_get_texture_size(&clk, NULL, &tex_h));

    /* ================================================================
     *  clk_clock_update —— 时间更新
     *
     *  测什么：调用后 clk_clock_time 字段被填入合法值。
     *  为什么测：update 调 localtime_s / localtime_r，跨平台写法
     *  不同（Windows 用 localtime_s，Linux 用 localtime_r），
     *  最容易在这里出平台 bug。
     * ================================================================ */

    clk_clock_update(&clk);
    /* 年份不应是 0（tm 被填充了） */
    TEST("clock_update sets year", clk.clk_clock_time.tm_year >= 0);
    /* 小时应在合法范围 */
    TEST("clock_update sets hour", clk.clk_clock_time.tm_hour >= 0 &&
                                     clk.clk_clock_time.tm_hour <= 23);

    /* 更新后纹理内容应有变化——通过检查第一个 cell 不为空来
     * 间接验证 clk_clock_update 触发了纹理刷新 */
    int mid_x = tex_w / 2;
    const clk_cell* c = clk_texture_get_cell(tex, mid_x, 2);
    TEST("clock texture has content after update", c != NULL && !c->is_empty);

    /* ================================================================
     *  clk_clock_change_time_format —— 切换时间格式
     *
     *  测什么：合法格式成功、NULL 格式失败、切完后 update 不崩。
     *  为什么测：时间格式变更需要重新评估纹理大小，如果纹理没
     *  重新分配，新格式超出旧纹理边界就会越界。
     * ================================================================ */

    TEST("clock_change_time_format to %%H:%%M succeeds",
         clk_clock_change_time_format(&clk, "%H:%M"));
    TEST("clock_change_time_format NULL fails",
         !clk_clock_change_time_format(&clk, NULL));

    /* 切格式后 update 不应崩溃 */
    clk_clock_update(&clk);

    /* ================================================================
     *  clk_clock_destroy —— 安全销毁
     *
     *  测什么：一次 destroy 不崩、二次 destroy 不崩、
     *  destroy 后 get_texture 返 NULL。
     *  为什么测：destroy 要释放纹理 data + 字形表内部内存。
     *  double-free 是最危险的内存 bug。
     * ================================================================ */

    clk_clock_destroy(&clk);
    TEST("clock_destroy no crash", 1);

    /* 二次 destroy 不应崩溃 */
    clk_clock_destroy(&clk);
    TEST("clock_destroy double no crash", 1);

    /* destroy 后将 clock 重新 create——验证 destroy 清理干净了 */
    ok = clk_clock_create(&clk, "%H:%M:%S", TEST_FONT_PATH);
    TEST("clock re-create after destroy succeeds", ok);
    clk_clock_destroy(&clk);

test_cleanup:
    printf("\n%d/%d passed\n", test_total - test_failed, test_total);
    return test_failed > 0 ? 1 : 0;
}
