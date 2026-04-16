/**
 * @file ai_ui_printer.c
 * @brief AI UI printer management — loads album thumbnails and dispatches to
 *        the registered printer page UI callbacks.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)

#include "tal_api.h"
#include "image_album.h"
#include "image_album_thumb.h"
#include "ai_ui_printer.h"

/***********************************************************
************************macro define************************
***********************************************************/
#if defined(ENABLE_IMAGE_ALBUM_STORAGE_MEM) && (ENABLE_IMAGE_ALBUM_STORAGE_MEM)
#define AI_UI_PRINTER_STORAGE_TP   IMAGE_ALBUM_STORAGE_TP_MEMORY
#elif defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
#define AI_UI_PRINTER_STORAGE_TP   IMAGE_ALBUM_STORAGE_TP_SD
#else
#define AI_UI_PRINTER_STORAGE_TP   IMAGE_ALBUM_STORAGE_TP_CUSTOM
#endif

#define AI_UI_PRINTER_THUMB_W  80
#define AI_UI_PRINTER_THUMB_H  80

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    ALBUM_THUMB_ITER_HANDLE  thumb_iter;
    ALBUM_THUMB_BATCH_T      thumb_batch;
    AI_UI_IMG_T             *thumb_arr;
    uint32_t                 thumb_arr_cnt;
} AI_UI_PRINTER_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_PRINTER_INTFS_T sg_printer_intfs;
static AI_UI_PRINTER_CTX_T   sg_ctx;

/***********************************************************
***********************function define**********************
***********************************************************/

static void __free_resources(void)
{
    if (sg_ctx.thumb_arr) {
        tal_free(sg_ctx.thumb_arr);
        sg_ctx.thumb_arr     = NULL;
        sg_ctx.thumb_arr_cnt = 0;
    }
    if (sg_ctx.thumb_batch.count > 0) {
        image_album_thumb_batch_free(&sg_ctx.thumb_batch);
        memset(&sg_ctx.thumb_batch, 0, sizeof(ALBUM_THUMB_BATCH_T));
    }
    if (sg_ctx.thumb_iter) {
        image_album_thumb_iter_deinit(sg_ctx.thumb_iter);
        sg_ctx.thumb_iter = NULL;
    }
}

OPERATE_RET ai_ui_printer_register(AI_UI_PRINTER_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);
    memcpy(&sg_printer_intfs, intfs, sizeof(AI_UI_PRINTER_INTFS_T));
    return OPRT_OK;
}

void ai_ui_printer_open(void)
{
    __free_resources();

    IMAGE_ALBUM_SORT_OPT_T sort_opt = {
        .key   = IMAGE_ALBUM_SORT_SAVE_SEQ,
        .order = IMAGE_ALBUM_SORT_DESC,
    };

    OPERATE_RET rt = image_album_thumb_iter_init(COMP_AI_PICTURE_ALBUM_NAME,
                                                 AI_UI_PRINTER_STORAGE_TP,
                                                 &sort_opt, &sg_ctx.thumb_iter);
    if (rt != OPRT_OK) {
        PR_WARN("printer: thumb iter init failed, rt:%d", rt);
        if (sg_printer_intfs.disp_thumb_list) {
            sg_printer_intfs.disp_thumb_list(NULL, 0);
        }
        return;
    }

    uint32_t total = image_album_thumb_iter_count(sg_ctx.thumb_iter);
    if (total == 0) {
        image_album_thumb_iter_deinit(sg_ctx.thumb_iter);
        sg_ctx.thumb_iter = NULL;
        if (sg_printer_intfs.disp_thumb_list) {
            sg_printer_intfs.disp_thumb_list(NULL, 0);
        }
        return;
    }

    ALBUM_THUMB_CFG_T cfg = {
        .width  = AI_UI_PRINTER_THUMB_W,
        .height = AI_UI_PRINTER_THUMB_H,
        .fmt    = ALBUM_THUMB_FMT_RGB565,
        .fit    = ALBUM_THUMB_FIT_COVER,
    };

    memset(&sg_ctx.thumb_batch, 0, sizeof(ALBUM_THUMB_BATCH_T));
    rt = image_album_thumb_iter_next(sg_ctx.thumb_iter, &cfg, total, &sg_ctx.thumb_batch);
    if (rt != OPRT_OK) {
        PR_ERR("printer: thumb iter next failed, rt:%d", rt);
        __free_resources();
        if (sg_printer_intfs.disp_thumb_list) {
            sg_printer_intfs.disp_thumb_list(NULL, 0);
        }
        return;
    }

    sg_ctx.thumb_arr = (AI_UI_IMG_T *)tal_malloc(sizeof(AI_UI_IMG_T) * sg_ctx.thumb_batch.count);
    if (sg_ctx.thumb_arr == NULL) {
        PR_ERR("printer: malloc thumb arr failed");
        __free_resources();
        if (sg_printer_intfs.disp_thumb_list) {
            sg_printer_intfs.disp_thumb_list(NULL, 0);
        }
        return;
    }
    memset(sg_ctx.thumb_arr, 0, sizeof(AI_UI_IMG_T) * sg_ctx.thumb_batch.count);

    uint32_t i;
    for (i = 0; i < sg_ctx.thumb_batch.count; i++) {
        sg_ctx.thumb_arr[i].name   = sg_ctx.thumb_batch.items[i].filename;
        sg_ctx.thumb_arr[i].width  = sg_ctx.thumb_batch.items[i].thumb.width;
        sg_ctx.thumb_arr[i].height = sg_ctx.thumb_batch.items[i].thumb.height;
        sg_ctx.thumb_arr[i].data   = sg_ctx.thumb_batch.items[i].thumb.buf;
        sg_ctx.thumb_arr[i].len    = sg_ctx.thumb_batch.items[i].thumb.size;
    }
    sg_ctx.thumb_arr_cnt = sg_ctx.thumb_batch.count;

    if (sg_printer_intfs.disp_thumb_list) {
        sg_printer_intfs.disp_thumb_list(sg_ctx.thumb_arr, sg_ctx.thumb_arr_cnt);
    }
}

void ai_ui_printer_close(void)
{
    __free_resources();
    if (sg_printer_intfs.disp_close) {
        sg_printer_intfs.disp_close();
    }
}

#endif /* ENABLE_PRINTER */
