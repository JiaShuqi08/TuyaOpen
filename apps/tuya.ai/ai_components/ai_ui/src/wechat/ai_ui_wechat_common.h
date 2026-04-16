/**
 * @file ai_ui_wechat_common.h
 * @brief Internal header shared among wechat UI page files.
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_UI_WECHAT_COMMON_H__
#define __AI_UI_WECHAT_COMMON_H__

#include "tuya_cloud_types.h"
#include "ai_ui_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
********************function declaration********************
***********************************************************/

/* Chat page */
void ai_ui_wechat_chat_init(lv_obj_t *parent);
void ai_ui_wechat_chat_register(void);

/* Camera page */
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
void ai_ui_wechat_camera_init(lv_obj_t *parent);
void ai_ui_wechat_camera_register(void);
#endif

/* Album page */
#if defined(ENABLE_IMAGE_ALBUM) && (ENABLE_IMAGE_ALBUM == 1)
void ai_ui_wechat_album_init(lv_obj_t *parent);
void ai_ui_wechat_album_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AI_UI_WECHAT_COMMON_H__ */
