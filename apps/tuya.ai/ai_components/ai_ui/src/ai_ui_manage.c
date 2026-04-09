/**
 * @file ai_ui_manage.c
 * @brief AI UI management implementation.
 *
 * This file provides functions for managing AI user interface, including
 * message queue handling, display interface registration, and camera/picture display.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tal_api.h"
#include "tuya_ringbuf.h"
#include <string.h>

#include "ai_ui_icon_font.h"
#include "ai_ui_stream_text.h"
#include "ai_ui_manage.h"

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "ai_picture.h"
#include "tal_image_jpeg_codec.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_MSG_MAX_BUF_LEN  1024


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    AI_UI_DISP_TYPE_E type;
    int               len;
    char             *data;
    SEM_HANDLE        sync_sem;
} AI_UI_MSG_T;

typedef struct {
    AI_UI_ACTION_E action;
    void          *data;
} AI_UI_ACTION_MSG_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_UI_INTFS_T sg_ui_intfs;
static QUEUE_HANDLE  sg_ui_queue_hdl;
static THREAD_HANDLE sg_ui_thrd_hdl;
static AI_UI_ACTION_CB sg_action_cb = NULL;
static QUEUE_HANDLE  sg_action_queue_hdl;
static THREAD_HANDLE sg_action_thrd_hdl;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Handle UI display message based on message type.
 *
 * @param[in] msg_data Pointer to the message data structure.
 * @return none
 * @note Binary payloads (video flush, JPEG) use layouts documented per case; pointers
 *       passed to UI callbacks reference msg_data->data and are only valid for the
 *       duration of the callback.
 */
static void __ui_disp_msg_handle(AI_UI_MSG_T *msg_data)
{
    if (NULL == msg_data) {
        return;
    }

    switch (msg_data->type) {
        case AI_UI_DISP_USER_MSG: {
            if(sg_ui_intfs.disp_user_msg) {
                sg_ui_intfs.disp_user_msg(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_AI_MSG: {
            if(sg_ui_intfs.disp_ai_msg) {
                sg_ui_intfs.disp_ai_msg(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_START: {
            if(sg_ui_intfs.disp_ai_msg_stream_start) {
                sg_ui_intfs.disp_ai_msg_stream_start();
            }

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_start();
#endif
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_DATA: {

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_write(msg_data->data);
#else 
            if(sg_ui_intfs.disp_ai_msg_stream_data) {
                sg_ui_intfs.disp_ai_msg_stream_data(msg_data->data);
            }
#endif
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_END: {
#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_end();
#else 
            if(sg_ui_intfs.disp_ai_msg_stream_end) {
                sg_ui_intfs.disp_ai_msg_stream_end();
            }
#endif
        } 
        break;
        case AI_UI_DISP_AI_MSG_STREAM_INTERRUPT: {
#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
            ai_ui_stream_text_end();
            ai_ui_stream_text_reset();
#else
        if(sg_ui_intfs.disp_ai_msg_stream_end) {
            sg_ui_intfs.disp_ai_msg_stream_end();
        }
#endif
        } 
        break;
        case AI_UI_DISP_SYSTEM_MSG: {
            if(sg_ui_intfs.disp_system_msg) {
                sg_ui_intfs.disp_system_msg(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_EMOTION: {
            if(sg_ui_intfs.disp_emotion) {
                sg_ui_intfs.disp_emotion(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_STATUS: {
            if(sg_ui_intfs.disp_ai_mode_state) {
                sg_ui_intfs.disp_ai_mode_state(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_NOTIFICATION: {
            if(sg_ui_intfs.disp_notification) {
                sg_ui_intfs.disp_notification(msg_data->data);
            }
        } 
        break;
        case AI_UI_DISP_NETWORK: {
            if(sg_ui_intfs.disp_wifi_state) {
                sg_ui_intfs.disp_wifi_state(((AI_UI_WIFI_STATUS_E*)msg_data->data)[0]);
            }
        } 
        break;
        case AI_UI_DISP_CHAT_MODE: {
            if (sg_ui_intfs.disp_ai_chat_mode) {
                sg_ui_intfs.disp_ai_chat_mode(msg_data->data);
            }
        }
        break ;
#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
        case AI_UI_DISP_PICTURE: {
            if (sg_ui_intfs.disp_jpeg_picture && msg_data->data) {
                AI_PICTURE_INFO_T pic = {0};
                OPERATE_RET rt = ai_picture_get_by_name(msg_data->data, &pic);
                if (OPRT_OK == rt && pic.data && pic.len > 0) {
                    sg_ui_intfs.disp_jpeg_picture(pic.data, pic.len);
                } else {
                    PR_ERR("get picture by name failed, rt:%d", rt);
                }
                ai_picture_free_pic_info(&pic);
            }
        }
        break;
#endif
        default:
            if (sg_ui_intfs.disp_other_msg) {
                sg_ui_intfs.disp_other_msg(msg_data->type, (uint8_t *)msg_data->data, msg_data->len);
            }
            break;
    }
}

/**
 * @brief AI chat UI task thread function.
 *
 * @param args Thread argument (unused).
 */
static void __ai_chat_ui_task(void *args)
{
    AI_UI_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
        tal_queue_fetch(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);

        __ui_disp_msg_handle(&msg_data);

        if (msg_data.sync_sem != NULL) {
            tal_semaphore_post(msg_data.sync_sem);
        }

        if (msg_data.data) {
            Free(msg_data.data);
        }
        msg_data.data = NULL;
    }
}

/**
 * @brief AI UI action dispatch task thread function.
 *
 * @param args Thread argument (unused).
 * @return none
 */
static void __ai_chat_ui_action_task(void *args)
{
    AI_UI_ACTION_MSG_T msg_data = {0};

    (void)args;

    for (;;) {
        memset(&msg_data, 0, sizeof(AI_UI_ACTION_MSG_T));
        tal_queue_fetch(sg_action_queue_hdl, &msg_data, SEM_WAIT_FOREVER);

        if (msg_data.action >= AI_UI_ACTION_MAX) {
            continue;
        }

        if (sg_action_cb != NULL) {
            sg_action_cb(msg_data.action, msg_data.data);
        }
    }
}

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
/**
 * @brief Display stream text callback function.
 *
 * @param string Pointer to the text string to display, NULL indicates end of stream.
 */
static void __ai_chat_ui_stream_text_disp(char *string)
{
    if(NULL == string) {
        if(sg_ui_intfs.disp_ai_msg_stream_end) {
            sg_ui_intfs.disp_ai_msg_stream_end();
        }
    }else {
        if(sg_ui_intfs.disp_ai_msg_stream_data) {
            sg_ui_intfs.disp_ai_msg_stream_data(string);
        }
    }
}
#endif

/**
 * @brief Initialize AI UI module.
 *
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_ui_queue_hdl, sizeof(AI_UI_MSG_T), 8));
    TUYA_CALL_ERR_RETURN(tal_queue_create_init(&sg_action_queue_hdl, sizeof(AI_UI_ACTION_MSG_T), 8));

    THREAD_CFG_T thrd_cfg;
    memset(&thrd_cfg, 0x00, sizeof(THREAD_CFG_T));
    thrd_cfg.thrdname = "ai_ui";
    thrd_cfg.priority = THREAD_PRIO_2;
    thrd_cfg.stackDepth = 1024 * 4;

    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_ui_thrd_hdl, NULL, NULL, __ai_chat_ui_task, NULL, &thrd_cfg));

    memset(&thrd_cfg, 0x00, sizeof(THREAD_CFG_T));
    thrd_cfg.thrdname = "ai_ui_action";
    thrd_cfg.priority = THREAD_PRIO_2;
    thrd_cfg.stackDepth = 1024 * 4;
    TUYA_CALL_ERR_RETURN(tal_thread_create_and_start(&sg_action_thrd_hdl, NULL, NULL, __ai_chat_ui_action_task, NULL, &thrd_cfg));

#if defined(ENABLE_AI_UI_TEXT_STREAMING) && (ENABLE_AI_UI_TEXT_STREAMING == 1)
    TUYA_CALL_ERR_RETURN(ai_ui_stream_text_init(__ai_chat_ui_stream_text_disp));
#endif

    if (sg_ui_intfs.disp_init) {
        TUYA_CALL_ERR_RETURN(sg_ui_intfs.disp_init());
    }

    PR_DEBUG("ai chat ui init success");

    return OPRT_OK;
}

/**
 * @brief Display message on UI.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_disp_msg(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len)
{
    AI_UI_MSG_T msg_data;

    memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (char *)Malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; /* "\0" */
    } else {
        msg_data.data = NULL;
    }

    return tal_queue_post(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
}

/**
 * @brief Display message on UI and block until the UI thread has finished dispatch.
 *
 * @param tp Display type indicating the message category.
 * @param data Pointer to the message data.
 * @param len Length of the message data.
 * @return OPERATE_RET Operation result code.
 * @note Uses a binary semaphore: the message is queued like ai_ui_disp_msg, then the
 *       caller waits until the UI worker posts after handling. Do not call from the
 *       UI worker thread (ai_ui) or deadlock will occur.
 */
OPERATE_RET ai_ui_disp_msg_sync(AI_UI_DISP_TYPE_E tp, uint8_t *data, int len)
{
    AI_UI_MSG_T msg_data;
    SEM_HANDLE  sync_sem = NULL;
    OPERATE_RET rt = OPRT_OK;

    memset(&msg_data, 0, sizeof(AI_UI_MSG_T));
    msg_data.type = tp;
    msg_data.len = len;
    if (len && data != NULL) {
        msg_data.data = (char *)Malloc(len + 1);
        if (NULL == msg_data.data) {
            return OPRT_MALLOC_FAILED;
        }
        memcpy(msg_data.data, data, len);
        msg_data.data[len] = 0; /* "\0" */
    }

    rt = tal_semaphore_create_init(&sync_sem, 0, 1);
    if (OPRT_OK != rt) {
        if (msg_data.data) {
            Free(msg_data.data);
        }
        return rt;
    }
    msg_data.sync_sem = sync_sem;

    rt = tal_queue_post(sg_ui_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
    if (OPRT_OK != rt) {
        tal_semaphore_release(sync_sem);
        if (msg_data.data) {
            Free(msg_data.data);
        }
        return rt;
    }

    rt = tal_semaphore_wait_forever(sync_sem);
    tal_semaphore_release(sync_sem);

    return rt;
}

/**
 * @brief Notify that an action menu item was touched (for internal use by UI implementation).
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 */
 void ai_ui_action_cb_register(AI_UI_ACTION_CB action_cb)
 {
    sg_action_cb = action_cb;
 }

/**
 * @brief Notify that an action menu item was touched. Called by UI impl (e.g. wechat); invokes cfg->action_cb if set.
 *
 * @param action Action id: AI_UI_ACTION_TAKE_PHOTO, AI_UI_ACTION_IMAGE_RECOG, etc.
 * @param data Pointer to the data.
 */
void ai_ui_notify_action(AI_UI_ACTION_E action, void *data)
{
    AI_UI_ACTION_MSG_T msg_data;

    if (action >= AI_UI_ACTION_MAX) {
        return;
    }

    if (sg_action_cb == NULL) {
        return;
    }

    memset(&msg_data, 0, sizeof(AI_UI_ACTION_MSG_T));
    msg_data.action = action;
    msg_data.data = data;

    tal_queue_post(sg_action_queue_hdl, &msg_data, SEM_WAIT_FOREVER);
}

/**
 * @brief Register UI interface callbacks.
 *
 * @param intfs Pointer to the UI interface structure containing callback functions.
 * @return OPERATE_RET Operation result code.
 */
OPERATE_RET ai_ui_register(AI_UI_INTFS_T *intfs)
{
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);

    memcpy(&sg_ui_intfs, intfs, sizeof(AI_UI_INTFS_T));

    return OPRT_OK;
}
