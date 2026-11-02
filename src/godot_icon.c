/* Official Godot logo paths from assets/launcher.svg (CC BY 4.0).
 * Copyright (c) 2017 Andrea Calabró. See assets/GODOT-LOGO-LICENSE.txt.
 * SVG quadratic curves are represented as equivalent cubic curves. */
#include "godot_icon.h"
#include <string.h>

static const AfPathCommand path_0[] = {
    {.type = AF_PATH_MOVE, .point = {105.0F, 673.0F}},
    {.type = AF_PATH_LINE, .point = {105.0F, 706.0F}},
    {.type = AF_PATH_CUBIC, .point = {919.0F, 706.0F},
     .control1 = {376.3333333F, 942.0F}, .control2 = {647.6666667F, 942.0F}},
    {.type = AF_PATH_LINE, .point = {919.0F, 673.0F}},
    {.type = AF_PATH_CLOSE},
};

static const AfPathCommand path_1[] = {
    {.type = AF_PATH_MOVE, .point = {105.0F, 673.0F}},
    {.type = AF_PATH_LINE, .point = {257.0F, 687.0F}},
    {.type = AF_PATH_CUBIC, .point = {272.0F, 701.0F},
     .control1 = {265.0F, 687.6666667F}, .control2 = {270.0F, 692.3333333F}},
    {.type = AF_PATH_LINE, .point = {276.0F, 768.0F}},
    {.type = AF_PATH_LINE, .point = {408.0F, 778.0F}},
    {.type = AF_PATH_LINE, .point = {416.0F, 717.0F}},
    {.type = AF_PATH_CUBIC, .point = {431.0F, 702.0F},
     .control1 = {417.3333333F, 709.6666667F}, .control2 = {422.3333333F, 704.6666667F}},
    {.type = AF_PATH_LINE, .point = {593.0F, 702.0F}},
    {.type = AF_PATH_CUBIC, .point = {608.0F, 717.0F},
     .control1 = {601.6666667F, 704.6666667F}, .control2 = {606.6666667F, 709.6666667F}},
    {.type = AF_PATH_LINE, .point = {616.0F, 778.0F}},
    {.type = AF_PATH_LINE, .point = {748.0F, 768.0F}},
    {.type = AF_PATH_LINE, .point = {752.0F, 701.0F}},
    {.type = AF_PATH_CUBIC, .point = {767.0F, 687.0F},
     .control1 = {754.0F, 692.3333333F}, .control2 = {759.0F, 687.6666667F}},
    {.type = AF_PATH_LINE, .point = {919.0F, 673.0F}},
    {.type = AF_PATH_LINE, .point = {919.0F, 427.0F}},
    {.type = AF_PATH_CUBIC, .point = {975.0F, 346.0F},
     .control1 = {939.0F, 401.0F}, .control2 = {957.6666667F, 374.0F}},
    {.type = AF_PATH_CUBIC, .point = {892.0F, 238.0F},
     .control1 = {951.6666667F, 306.6666667F}, .control2 = {924.0F, 270.6666667F}},
    {.type = AF_PATH_CUBIC, .point = {810.0F, 285.0F},
     .control1 = {863.3333333F, 251.3333333F}, .control2 = {836.0F, 267.0F}},
    {.type = AF_PATH_CUBIC, .point = {722.0F, 221.0F},
     .control1 = {783.3333333F, 260.3333333F}, .control2 = {754.0F, 239.0F}},
    {.type = AF_PATH_CUBIC, .point = {730.0F, 119.0F},
     .control1 = {726.6666667F, 187.0F}, .control2 = {729.3333333F, 153.0F}},
    {.type = AF_PATH_CUBIC, .point = {607.0F, 77.0F},
     .control1 = {690.6666667F, 100.3333333F}, .control2 = {649.6666667F, 86.3333333F}},
    {.type = AF_PATH_CUBIC, .point = {561.0F, 166.0F},
     .control1 = {589.6666667F, 105.6666667F}, .control2 = {574.3333333F, 135.3333333F}},
    {.type = AF_PATH_CUBIC, .point = {463.0F, 166.0F},
     .control1 = {528.3333333F, 161.3333333F}, .control2 = {495.6666667F, 161.3333333F}},
    {.type = AF_PATH_CUBIC, .point = {417.0F, 77.0F},
     .control1 = {449.6666667F, 135.3333333F}, .control2 = {434.3333333F, 105.6666667F}},
    {.type = AF_PATH_CUBIC, .point = {294.0F, 119.0F},
     .control1 = {374.3333333F, 86.3333333F}, .control2 = {333.3333333F, 100.3333333F}},
    {.type = AF_PATH_CUBIC, .point = {302.0F, 221.0F},
     .control1 = {294.6666667F, 153.0F}, .control2 = {297.3333333F, 187.0F}},
    {.type = AF_PATH_CUBIC, .point = {214.0F, 285.0F},
     .control1 = {270.0F, 239.0F}, .control2 = {240.6666667F, 260.3333333F}},
    {.type = AF_PATH_CUBIC, .point = {132.0F, 238.0F},
     .control1 = {188.0F, 267.0F}, .control2 = {160.6666667F, 251.3333333F}},
    {.type = AF_PATH_CUBIC, .point = {49.0F, 346.0F},
     .control1 = {100.0F, 270.6666667F}, .control2 = {72.3333333F, 306.6666667F}},
    {.type = AF_PATH_CUBIC, .point = {105.0F, 427.0F},
     .control1 = {66.3333333F, 374.0F}, .control2 = {85.0F, 401.0F}},
    {.type = AF_PATH_CLOSE},
    {.type = AF_PATH_MOVE, .point = {105.0F, 706.0F}},
    {.type = AF_PATH_LINE, .point = {105.0F, 745.0F}},
    {.type = AF_PATH_CUBIC, .point = {919.0F, 745.0F},
     .control1 = {105.0F, 1021.0F}, .control2 = {918.0F, 1021.0F}},
    {.type = AF_PATH_LINE, .point = {919.0F, 706.0F}},
    {.type = AF_PATH_LINE, .point = {785.0F, 718.0F}},
    {.type = AF_PATH_LINE, .point = {780.0F, 787.0F}},
    {.type = AF_PATH_CUBIC, .point = {766.0F, 800.0F},
     .control1 = {778.6666667F, 793.6666667F}, .control2 = {774.0F, 798.0F}},
    {.type = AF_PATH_LINE, .point = {604.0F, 811.0F}},
    {.type = AF_PATH_CUBIC, .point = {588.0F, 800.0F},
     .control1 = {596.0F, 811.0F}, .control2 = {590.6666667F, 807.3333333F}},
    {.type = AF_PATH_LINE, .point = {578.0F, 735.0F}},
    {.type = AF_PATH_LINE, .point = {446.0F, 735.0F}},
    {.type = AF_PATH_LINE, .point = {436.0F, 800.0F}},
    {.type = AF_PATH_CUBIC, .point = {420.0F, 811.0F},
     .control1 = {433.3333333F, 807.3333333F}, .control2 = {428.0F, 811.0F}},
    {.type = AF_PATH_LINE, .point = {258.0F, 800.0F}},
    {.type = AF_PATH_CUBIC, .point = {244.0F, 787.0F},
     .control1 = {250.0F, 798.0F}, .control2 = {245.3333333F, 793.6666667F}},
    {.type = AF_PATH_LINE, .point = {239.0F, 718.0F}},
    {.type = AF_PATH_CLOSE},
};

static const AfPathCommand path_2[] = {
    {.type = AF_PATH_MOVE, .point = {483.0F, 600.0F}},
    {.type = AF_PATH_CUBIC, .point = {541.0F, 600.0F},
     .control1 = {483.0F, 634.0F}, .control2 = {541.0F, 634.0F}},
    {.type = AF_PATH_LINE, .point = {541.0F, 514.0F}},
    {.type = AF_PATH_CUBIC, .point = {483.0F, 514.0F},
     .control1 = {541.0F, 480.0F}, .control2 = {483.0F, 480.0F}},
    {.type = AF_PATH_CLOSE},
};

static void draw_path(AfUiContext *ui, const AfPathCommand *commands, uint32_t count,
                      float x, float y, float scale, AfColor fill, int layer) {
    if (count > ui->paths->command_capacity - ui->paths->command_count ||
        ui->paths->path_count >= ui->paths->path_capacity ||
        ui->scene->count >= ui->scene->capacity) {
        ui->overflowed = true;
        return;
    }
    uint32_t start = ui->paths->command_count;
    memcpy(&ui->paths->commands[start], commands, (size_t)count * sizeof(*commands));
    for (uint32_t i = 0; i < count; ++i) {
        AfPathCommand *c = &ui->paths->commands[start + i];
        c->point.x *= scale;
        c->point.y *= scale;
        c->control1.x *= scale;
        c->control1.y *= scale;
        c->control2.x *= scale;
        c->control2.y *= scale;
    }
    ui->paths->command_count += count;
    af_ui_finish_text_path(ui, start, (AfVec2){x, y}, fill, layer);
}

static void eye(AfUiContext *ui, float x, float y, float cx, float cy, float radius,
                 float scale, AfColor fill, int layer) {
    af_ui_add_shape(ui, AF_NODE_ELLIPSE,
                    (AfAffine2){.m00 = 1, .m11 = 1,
                                .tx = x + (cx - radius) * scale,
                                .ty = y + (cy - radius) * scale},
                    (AfVec2){2 * radius * scale, 2 * radius * scale}, fill, 0, layer);
}

void gl_godot_icon(AfUiContext *ui, float x, float y, float size, int layer) {
    const float scale = size / 1024;
    const AfColor white = {1, 1, 1, 1};
    const AfColor blue = {71 / 255.0F, 140 / 255.0F, 191 / 255.0F, 1};
    const AfColor dark = {65 / 255.0F, 64 / 255.0F, 66 / 255.0F, 1};
#define DRAW(path, fill) \
    draw_path(ui, path, (uint32_t)(sizeof(path) / sizeof(path[0])), x, y, scale, fill, layer)
    DRAW(path_0, white);
    DRAW(path_1, blue);
    DRAW(path_2, white);
#undef DRAW
    eye(ui, x, y, 725, 526, 90, scale, white, layer);
    eye(ui, x, y, 299, 526, 90, scale, white, layer);
    eye(ui, x, y, 307, 532, 60, scale, dark, layer);
    eye(ui, x, y, 717, 532, 60, scale, dark, layer);
}

