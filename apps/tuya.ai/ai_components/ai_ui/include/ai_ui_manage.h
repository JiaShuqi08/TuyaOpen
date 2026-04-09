/**
 * @file ai_ui_manage.h
 * @brief AI UI management interface definitions.
 *
 * This header provides function declarations and type definitions for managing
 * AI user interface, including display interfaces for messages, emotions, status,
 * camera, and pictures.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#ifndef __AI_CHAT_UI_MANAGE_H__
#define __AI_CHAT_UI_MANAGE_H__

#include "tuya_cloud_types.h"
#include "ai_user_event.h"
#include "lang_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
/* Display network status */
typedef uint8_t AI_UI_WIFI_STATUS_E;
#define AI_UI_WIFI_STATUS_DISCONNECTED 0
#define AI_UI_WIFI_STATUS_GOOD         1
#define AI_UI_WIFI_STATUS_FAIR         2
#define AI_UI_WIFI_STATUS_WEAK         3

typedef enum {
    AI_UI_DISP_USER_MSG,
    AI_UI_DISP_AI_MSG,
    AI_UI_DISP_AI_MSG_STREAM_START,
    AI_UI_DISP_AI_MSG_STREAM_DATA,
    AI_UI_DISP_AI_MSG_STREAM_END,
    AI_UI_DISP_AI_MSG_STREAM_INTERRUPT,
    AI_UI_DISP_AI_PICTURE,
    AI_UI_DISP_SYSTEM_MSG,
    AI_UI_DISP_EMOTION,
    AI_UI_DISP_STATUS,
    AI_UI_DISP_NOTIFICATION,
    AI_UI_DISP_NETWORK,
    AI_UI_DISP_CHAT_MODE,
    AI_UI_DISP_PICTURE,
    AI_UI_DISP_SYS_MAX,
}AI_UI_DISP_TYPE_E;
typedef enum {
    AI_UI_ACTION_OPEN_CAMERA,   
    AI_UI_ACTION_TAKE_PHOTO, 
    AI_UI_ACTION_CLOSE_CAMER, 
    AI_UI_ACTION_OPEN_IMG_LIST,
    AI_UI_ACTION_CLOSE_IMG_LIST,
    AI_UI_ACTION_IMAGE_SELECTED,
    AI_UI_ACTION_IMAGE_UNSELECTED,
    AI_UI_ACTION_MAX
} AI_UI_ACTION_E;

typedef struct {
    OPERATE_RET (*disp_init)(void);
    void (*disp_user_msg)(char* string);
    void (*disp_ai_msg)(char* string);
    void (*disp_ai_msg_stream_start)(void);
    void (*disp_ai_msg_stream_data)(char *string);
    void (*disp_ai_msg_stream_end)(void);
    void (*disp_system_msg)(char *string);
    void (*disp_emotion)(char *emotion);
    void (*disp_ai_mode_state)(char *string);
    void (*disp_notification)(char *string);
    void (*disp_wifi_state)(AI_UI_WIFI_STATUS_E wifi_status);
    void (*disp_ai_chat_mode)(char *string);
    void (*disp_other_msg)(uint32_t type, uint8_t *data, int len );

    OPERATE_RET (*disp_camera_start)(uint16_t width, uint16_t height);
    OPERATE_RET (*disp_camera_flush)(uint8_t *data, uint16_t width, uint16_t height);
    OPERATE_RET (*disp_camera_end)(void);

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    OPERATE_RET (*disp_jpeg_picture)(uint8_t *jpeg, uint32_t len);
#endif

}AI_UI_INTFS_T;
typedef OPERATE_RET (*AI_UI_ACTION_CB)(AI_UI_ACTION_E action, void *data);

/***********************************************************
********************function declaration********************
***********************************************************/
/**
 * @brief Register UI interface callbacks.
 *
 * @param intfs Pointer to the UI interface structure containing callback functions.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs);

/**
 * @brief Initialize AI UI module.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_init(void);

/**
 * @brief Display message on UI.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);

/**
 * @brief Display message on UI and block until the UI thread has finished dispatch.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 * @note Synchronizes via a semaphore after queueing; registered callbacks run on the UI
 *       worker thread. Do not call from that same thread or deadlock will occur.
 */
OPERATE_RET ai_ui_disp_msg_sync(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len);

/**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_action_cb_register(AI_UI_ACTION_CB action_cb);

/**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_notify_action(AI_UI_ACTION_E action, void *data);

#ifdef __cplusplus
}
#endif

#endif /* __AI_CHAT_UI_MANAGE_H__ */
