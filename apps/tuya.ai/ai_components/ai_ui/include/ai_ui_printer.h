/**
 * @file ai_ui_printer.h
 * @brief AI UI printer management — loads album thumbnails and dispatches to
 *        the registered printer UI callbacks.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_PRINTER_H__
#define __AI_UI_PRINTER_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    void (*disp_thumb_list)(AI_UI_IMG_T *item_arr, uint32_t arr_cnt);
    void (*disp_close)(void);
} AI_UI_PRINTER_INTFS_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_ui_printer_register(AI_UI_PRINTER_INTFS_T *intfs);
void        ai_ui_printer_open(void);
void        ai_ui_printer_close(void);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_PRINTER */

#endif /* __AI_UI_PRINTER_H__ */
