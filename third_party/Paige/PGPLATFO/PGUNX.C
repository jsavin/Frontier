/*
 * Headless/UNIX Paige platform glue.
 *
 * This file implements the machine-facing hooks that were historically
 * provided by the Mac (QuickDraw) or Windows (GDI) layers. The headless
 * runtime only needs enough behavior to keep the text engine alive for
 * serialization, so the implementations here intentionally avoid any real
 * drawing or OS integration. They provide deterministic defaults so pack /
 * unpack paths continue to work in the absence of a GUI.
 */

#include "PAIGE.H"
#include "MACHINE.H"
#include "PGMEMMGR.H"
#include "PGUTILS.H"
#include "PGERRORS.H"
#include "PGREGION.H"
#include "PGSELECT.H"
#include "PGSHAPES.H"

#ifndef MAX_OFFSCREEN
#define MAX_OFFSCREEN 48000
#endif
#ifndef MAX_TEXTBLOCK
#define MAX_TEXTBLOCK 4096
#endif
#ifndef DEF_TAB_SPACE
#define DEF_TAB_SPACE 24
#endif
#ifndef DEF_MIN_WIDTH
#define DEF_MIN_WIDTH 16
#endif

#define HEADLESS_RESOLUTION     72
#define HEADLESS_ADVANCE_WIDTH  ((pg_short_t)512)

typedef struct headless_device_state {
    graf_device_ptr previous;
} headless_device_state;

static void headless_init_char(pg_char_ptr dst, pg_short_t value) {
    pgFillBlock(dst, sizeof(pg_char) * 4, 0);
    dst[0] = (pg_char)value;
}

static void headless_init_global_chars(pg_globals_ptr globals) {
    globals->line_wrap_char = 0x0D;
    globals->soft_line_char = 0x0A;
    globals->tab_char = 0x09;
    globals->soft_hyphen_char = 0x1F;
    globals->bs_char = 0x08;
    globals->ff_char = 0x0C;
    globals->container_brk_char = 0x0E;
    globals->left_arrow_char = 0x1C;
    globals->right_arrow_char = 0x1D;
    globals->up_arrow_char = 0x1E;
    globals->down_arrow_char = 0x1F;
    globals->fwd_delete_char = 0x7F;
    globals->text_brk_char = 0x1B;

    headless_init_char(globals->hyphen_char, '-');
    headless_init_char(globals->decimal_char, '.');
    headless_init_char(globals->cr_invis_symbol, 0xA6);
    headless_init_char(globals->lf_invis_symbol, 0xB9);
    headless_init_char(globals->tab_invis_symbol, 0x13);
    headless_init_char(globals->end_invis_symbol, 0xB0);
    headless_init_char(globals->pbrk_invis_symbol, 0xAD);
    headless_init_char(globals->cont_invis_symbol, 0xAD);
    headless_init_char(globals->space_invis_symbol, 0x20);
    headless_init_char(globals->flat_single_quote, '\'');
    headless_init_char(globals->flat_double_quote, '"');
    headless_init_char(globals->left_single_quote, '\'');
    headless_init_char(globals->right_single_quote, '\'');
    headless_init_char(globals->left_double_quote, '"');
    headless_init_char(globals->right_double_quote, '"');
    headless_init_char(globals->elipse_symbol, '.');
    headless_init_char(globals->unknown_char, '?');
    headless_init_char(globals->bullet_char, 0x95);
}

PG_PASCAL (void) pgMachineInit(pg_globals_ptr globals) {
    globals->max_offscreen = MAX_OFFSCREEN;
    globals->max_block_size = MAX_TEXTBLOCK;
    globals->def_tab_space = DEF_TAB_SPACE;
    globals->minimum_line_width = DEF_MIN_WIDTH;
    globals->color_enable = FALSE;
    globals->system_version = 0;
    globals->machine_const = 0;
    globals->offscreen_enable = OFFSCREEN_UNSUCCESSFUL;
    globals->current_port = &globals->offscreen_port;

    globals->def_bk_color.red = globals->def_bk_color.green = globals->def_bk_color.blue = 0xFFFF;
    globals->trans_color = globals->def_bk_color;

    headless_init_global_chars(globals);
    pgInitDefaultDevice(globals, &globals->offscreen_port);
    pgInitDefaultFont(globals, &globals->def_font);
    pgInitDefaultStyle(globals, &globals->def_style, &globals->def_font);
    pgInitDefaultPar(globals, &globals->def_par);
}

PG_PASCAL (void) pgMachineShutdown(const pg_globals_ptr globals) {
    (void)globals;
}

PG_PASCAL (void) pgInitDefaultDevice(const pg_globals_ptr globals, graf_device_ptr device) {
    (void)globals;
    pgFillBlock(device, sizeof(graf_device), 0);
    device->machine_var = USE_NO_DEVICE;
    device->resolution = (HEADLESS_RESOLUTION << 16) | HEADLESS_RESOLUTION;
}

PG_PASCAL (void) pgInitDefaultFont(const pg_globals_ptr globals, font_info_ptr font) {
    (void)globals;
    pgFillBlock(font, sizeof(font_info), 0);
    font->name[0] = 7;
    pgBlockMove("Default", &font->name[1], 7);
    font->environs = FONT_GOOD;
    font->platform = PAIGE_GRAPHICS;
}

PG_PASCAL (void) pgSetGrafDevice(paige_rec_ptr pg, short verb, graf_device_ptr device, color_value_ptr bk_color) {
    pg_globals_ptr globals = pg->globals;

    if (verb == unset_pg_device) {
        if (device && device->machine_ref)
            globals->current_port = (graf_device_ptr)device->machine_ref;
        else
            globals->current_port = &globals->offscreen_port;
        if (device)
            device->machine_ref = 0;
        return;
    }

    if (!device)
        return;

    device->machine_ref = (size_t)globals->current_port;
    globals->current_port = device;
    if (bk_color)
        device->bk_color = *bk_color;
}

PG_PASCAL (void) pgClipGrafDevice(paige_rec_ptr pg, short clip_verb, shape_ref alternate_vis) {
    (void)pg;
    (void)clip_verb;
    (void)alternate_vis;
}

PG_PASCAL (void) pgSetMeasureDevice(paige_rec_ptr pg) {
    (void)pg;
}

PG_PASCAL (void) pgUnsetMeasureDevice(paige_rec_ptr pg) {
    (void)pg;
}

PG_PASCAL (void) pgPrintDeviceChanged(paige_rec_ptr pg) {
    (void)pg;
}

PG_PASCAL (void) pgPrepareOffscreen(paige_rec_ptr pg, rectangle_ptr target_area,
        rectangle_ptr real_bits_target, co_ordinate_ptr offset_adjust,
        long text_offset, point_start_ptr line_start, short draw_mode) {
    (void)pg;
    (void)target_area;
    (void)real_bits_target;
    (void)offset_adjust;
    (void)text_offset;
    (void)line_start;
    (void)draw_mode;
}

PG_PASCAL (pg_boolean) pgFinishOffscreen(paige_rec_ptr pg, long text_offset,
        point_start_ptr line_start, co_ordinate_ptr new_offset,
        rectangle_ptr new_target, short draw_mode) {
    (void)pg;
    (void)text_offset;
    (void)line_start;
    (void)new_offset;
    (void)new_target;
    (void)draw_mode;
    return FALSE;
}

PG_PASCAL (void) pgScaleGrafDevice(paige_rec_ptr pg) {
    (void)pg;
}

PG_PASCAL (pg_region) pgScrollRect(paige_rec_ptr pg, rectangle_ptr rect, long distance_h,
        long distance_v, rectangle_ptr affected_area, short draw_mode) {
    (void)pg;
    (void)rect;
    (void)distance_h;
    (void)distance_v;
    (void)affected_area;
    (void)draw_mode;
    return MEM_NULL;
}

PG_PASCAL (void) pgEraseRect(pg_globals_ptr globals, rectangle_ptr rect,
        pg_scale_ptr scaling, co_ordinate_ptr offset_extra) {
    (void)globals;
    (void)rect;
    (void)scaling;
    (void)offset_extra;
}

static pg_short_t headless_measure(pg_char_ptr data, long length, long slop,
        long num_spaces, pg_text_int PG_FAR *positions) {
    (void)data;
    pg_short_t advance = HEADLESS_ADVANCE_WIDTH;
    pg_short_t total = (pg_short_t)(length * advance);
    total += (pg_short_t)(num_spaces * advance);
    total += (pg_short_t)slop;

    if (positions) {
        long current = 0;
        for (long i = 0; i <= length; ++i) {
            positions[i] = (pg_text_int)current;
            if (i < length)
                current += advance;
        }
    }

    return total;
}

PG_PASCAL (pg_short_t) pgMeasureText(paige_rec_ptr pg, short measure_verb, pg_char_ptr data,
        long length, long slop, long num_spaces, pg_text_int PG_FAR *positions,
        style_walk_ptr walker) {
    (void)pg;
    (void)measure_verb;
    (void)walker;
    return headless_measure(data, length, slop, num_spaces, positions);
}

PG_PASCAL (pg_short_t) pgMeasureText32(paige_rec_ptr pg, short measure_verb, pg_char_ptr data,
        long length, long slop, long num_spaces, pg_text_int PG_FAR *positions,
        style_walk_ptr walker) {
    return pgMeasureText(pg, measure_verb, data, length, slop, num_spaces, positions, walker);
}

PG_PASCAL (pg_short_t) pgMeasureText16(paige_rec_ptr pg, short measure_verb, pg_char_ptr data,
        long length, long slop, long num_spaces, pg_text_int PG_FAR *positions,
        style_walk_ptr walker) {
    return pgMeasureText(pg, measure_verb, data, length, slop, num_spaces, positions, walker);
}

PG_PASCAL (short) pgScalePointSize(paige_rec_ptr pg, style_walk_ptr walker,
        pg_char_ptr text, long length, pg_boolean PG_FAR *did_scale) {
    (void)pg;
    (void)text;
    (void)length;
    if (did_scale)
        *did_scale = FALSE;
    if (walker && walker->cur_style && walker->cur_style->point != 0)
        return (short)(walker->cur_style->point >> 16);
    return 12;
}

PG_PASCAL (short) pgSystemDirection(pg_globals_ptr globals) {
    (void)globals;
    return 0;
}

PG_PASCAL (pg_boolean) pgIsRealFont(pg_globals_ptr globals, font_info_ptr font, pg_boolean use_alternate) {
    (void)globals;
    (void)font;
    (void)use_alternate;
    return TRUE;
}

PG_PASCAL (void) pgOSToPgColor(const pg_plat_color_value PG_FAR *os_color, color_value_ptr pg_color) {
    if (!pg_color)
        return;
    if (os_color) {
        pg_color->red = os_color->red;
        pg_color->green = os_color->green;
        pg_color->blue = os_color->blue;
    }
    else {
        pg_color->red = pg_color->green = pg_color->blue = 0;
    }
    pg_color->alpha = 0;
}

PG_PASCAL (void) pgColorToOS(const color_value_ptr pg_color, pg_plat_color_value PG_FAR *os_color) {
    if (!os_color || !pg_color)
        return;
    os_color->red = pg_color->red;
    os_color->green = pg_color->green;
    os_color->blue = pg_color->blue;
}

PG_PASCAL (pg_fixed) pgPointsizeToScreen(pg_ref pg, pg_fixed pointsize) {
    (void)pg;
    return pointsize;
}

PG_PASCAL (pg_fixed) pgScreenToPointsize(pg_ref pg, pg_fixed screensize) {
    (void)pg;
    return screensize;
}

PG_PASCAL (void) pgTransLiterate(pg_char_ptr text, long length, pg_char_ptr target,
        pg_boolean do_uppercase) {
    for (long i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (do_uppercase) {
            if (ch >= 'a' && ch <= 'z')
                ch = (unsigned char)(ch - 'a' + 'A');
        } else {
            if (ch >= 'A' && ch <= 'Z')
                ch = (unsigned char)(ch - 'A' + 'a');
        }
        target[i] = (pg_char)ch;
    }
}

PG_PASCAL (void) pgOpenPrinter(paige_rec_ptr pg_rec, graf_device_ptr print_dev,
        long first_position, rectangle_ptr page_rect) {
    (void)pg_rec;
    (void)print_dev;
    (void)first_position;
    (void)page_rect;
}

PG_PASCAL (void) pgClosePrinter(paige_rec_ptr pg_rec, graf_device_ptr print_dev) {
    (void)pg_rec;
    (void)print_dev;
}

PG_PASCAL (pg_boolean) pgIsCaretTime(paige_rec_ptr pg) {
    (void)pg;
    return FALSE;
}

PG_PASCAL (short) pgGetCharWidth(paige_rec_ptr pg_rec, style_info_ptr style, pg_char the_char) {
    (void)pg_rec;
    (void)style;
    (void)the_char;
    return HEADLESS_ADVANCE_WIDTH;
}
