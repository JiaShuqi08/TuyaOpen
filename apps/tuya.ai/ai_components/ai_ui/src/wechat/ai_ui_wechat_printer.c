/**
 * @file ai_ui_wechat_printer.c
 * @brief WeChat-style printer page — thumbnail grid, tap to select and print.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)

#include "tal_api.h"
#include "lvgl.h"
#include "lv_vendor.h"

#include "ai_ui_manage.h"
#include "ai_ui_printer.h"
#include "ai_ui_wechat_common.h"
#include "lang_config.h"

LV_IMG_DECLARE(icon_back_24_24);
LV_IMG_DECLARE(icon_printer_app);

/***********************************************************
************************macro define************************
***********************************************************/
#define CTRL_BAR_HEIGHT  48
#define THUMB_SIZE       80
#define THUMB_GAP        8

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    lv_obj_t *page;
    lv_obj_t *grid;
    lv_obj_t *empty_label;
} AI_UI_WECHAT_PRINTER_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_WECHAT_PRINTER_T sg_printer = {0};

/***********************************************************
***********************function define**********************
***********************************************************/

static void __cancel_btn_cb(lv_event_t *e)
{
    (void)e;
    ai_ui_notify_action(AI_UI_ACT_CLOSE_PRINTER, NULL, 0);
}

static void __item_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    char *name = (char *)lv_obj_get_user_data(target);
    if (name != NULL) {
        ai_ui_notify_action(AI_UI_ACT_PRINT_IMG,
                            (uint8_t *)name,
                            (uint32_t)strlen(name));
    }
    ai_ui_notify_action(AI_UI_ACT_CLOSE_PRINTER, NULL, 0);
}

/* ── printer display callbacks (registered into ai_ui_printer) ── */

static void __disp_thumb_list(AI_UI_IMG_T *item_arr, uint32_t arr_cnt)
{
    lv_vendor_disp_lock();

    lv_obj_clear_flag(sg_printer.page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(sg_printer.grid);

    if (NULL == item_arr || 0 == arr_cnt) {
        lv_obj_clear_flag(sg_printer.empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_vendor_disp_unlock();
        return;
    }

    lv_obj_add_flag(sg_printer.empty_label, LV_OBJ_FLAG_HIDDEN);

    uint32_t i;
    for (i = 0; i < arr_cnt; i++) {
        AI_UI_IMG_T *item = &item_arr[i];

        lv_obj_t *container = lv_obj_create(sg_printer.grid);
        lv_obj_set_size(container, THUMB_SIZE, THUMB_SIZE);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_radius(container, 2, 0);
        lv_obj_set_style_bg_color(container, lv_color_black(), 0);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_user_data(container, (void *)item->name);

        if (item->data != NULL && item->width > 0 && item->height > 0) {
            lv_obj_t *canvas = lv_canvas_create(container);
            lv_canvas_set_buffer(canvas, item->data,
                                 item->width, item->height,
                                 LV_IMG_CF_TRUE_COLOR);
            lv_obj_center(canvas);
        }

        lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(container, __item_click_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_vendor_disp_unlock();
}

static void __disp_close(void)
{
    lv_vendor_disp_lock();
    lv_obj_add_flag(sg_printer.page, LV_OBJ_FLAG_HIDDEN);
    lv_vendor_disp_unlock();
}

/* ── public API ── */

/**
 * @brief Initialize printer page widgets. Called once during UI init
 *        (inside lv_vendor_disp_lock).
 *
 * @param parent Parent LVGL object (screen, full-screen overlay).
 */
void ai_ui_wechat_printer_init(lv_obj_t *parent)
{
    memset(&sg_printer, 0, sizeof(AI_UI_WECHAT_PRINTER_T));

    lv_coord_t page_w = LV_HOR_RES;
    lv_coord_t page_h = LV_VER_RES;

    sg_printer.page = lv_obj_create(parent);
    lv_obj_set_size(sg_printer.page, page_w, page_h);
    lv_obj_set_style_bg_color(sg_printer.page, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(sg_printer.page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sg_printer.page, 0, 0);
    lv_obj_set_style_pad_all(sg_printer.page, 0, 0);
    lv_obj_set_style_radius(sg_printer.page, 0, 0);
    lv_obj_set_scrollbar_mode(sg_printer.page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(sg_printer.page, 0, 0);
    lv_obj_add_flag(sg_printer.page, LV_OBJ_FLAG_HIDDEN);

    /* Back button — top-left */
    lv_obj_t *back_btn = lv_obj_create(sg_printer.page);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_size(back_btn, 36, 36);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *back_img = lv_img_create(back_btn);
    lv_img_set_src(back_img, &icon_back_24_24);
    lv_obj_set_style_img_recolor(back_img, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back_img, LV_OPA_COVER, 0);
    lv_obj_center(back_img);
    lv_obj_add_event_cb(back_btn, __cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    /* Title — top-center */
    lv_obj_t *title = lv_label_create(sg_printer.page);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, PRINT_IMAGE);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    /* Scrollable thumbnail grid */
    sg_printer.grid = lv_obj_create(sg_printer.page);
    lv_obj_set_size(sg_printer.grid, page_w, page_h - CTRL_BAR_HEIGHT);
    lv_obj_align(sg_printer.grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(sg_printer.grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sg_printer.grid, 0, 0);
    lv_obj_set_style_pad_all(sg_printer.grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_row(sg_printer.grid, THUMB_GAP, 0);
    lv_obj_set_style_pad_column(sg_printer.grid, THUMB_GAP, 0);
    lv_obj_set_style_radius(sg_printer.grid, 0, 0);
    lv_obj_set_flex_flow(sg_printer.grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_scrollbar_mode(sg_printer.grid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(sg_printer.grid, LV_DIR_VER);

    /* Empty state label */
    sg_printer.empty_label = lv_label_create(sg_printer.page);
    lv_obj_set_style_text_color(sg_printer.empty_label, lv_color_white(), 0);
    lv_label_set_text(sg_printer.empty_label, NO_IMAGE);
    lv_obj_center(sg_printer.empty_label);
    lv_obj_add_flag(sg_printer.empty_label, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Register printer page callbacks into the printer management layer.
 */
void ai_ui_wechat_printer_register(void)
{
    AI_UI_PRINTER_INTFS_T intfs;
    memset(&intfs, 0, sizeof(AI_UI_PRINTER_INTFS_T));

    intfs.disp_thumb_list = __disp_thumb_list;
    intfs.disp_close      = __disp_close;

    ai_ui_printer_register(&intfs);
}

#endif /* ENABLE_PRINTER */
