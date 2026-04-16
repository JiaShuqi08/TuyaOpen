/**
 * @file app_ui_action.c
 * @brief app_ui_action module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include "ai_ui_manage.h"
#include "ai_picture.h"
#include "ai_picture_input.h"
#include "ai_video_input.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static bool sg_ai_vision_enabled = false;


/***********************************************************
***********************function define**********************
***********************************************************/
static void __display_camera_yuv_fram(TDL_CAMERA_FRAME_T *frame)
{
    AI_UI_VIDEO_T video = {0};

    if (NULL == frame) {
        return;
    }

    video.width  = frame->width;
    video.height = frame->height;
    video.yuv422 = frame->data;
    video.len    = frame->data_len;

    ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_FLUSH, (uint8_t*)&video, sizeof(AI_UI_VIDEO_T));
}


static void __app_ui_action_handle(AI_UI_ACTION_E action, uint8_t *data, uint32_t len)
{
    switch (action) {
    case AI_UI_ACT_OPEN_CAMERA:
        sg_ai_vision_enabled = false;
        ai_video_set_yuv_frame_flush_cb(__display_camera_yuv_fram);
        ai_video_start();
        ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_OPEN, NULL, 0);

        /*get the newest image*/

        break;

    case AI_UI_ACT_TAKE_PHOTO: {
        uint8_t *jpeg = NULL;
        uint32_t jpeg_len = 0;
        ai_video_get_jpeg_frame(&jpeg, &jpeg_len);

        if (jpeg && jpeg_len) {
            char name[AI_PICTURE_NAME_MAX_LEN + 1] = {0};
            ai_picture_save_to_album(jpeg, jpeg_len, name);
            ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_THUMB, jpeg, jpeg_len);

            if (sg_ai_vision_enabled && name[0]) {
                /* Close camera so the user sees the AI response in chat */
                ai_video_stop();
                ai_video_set_yuv_frame_flush_cb(NULL);
                ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
                ai_ui_disp_msg(AI_UI_DISP_USER_IMAGE_LINK, (uint8_t *)name, strlen(name));

                ai_picture_input_recognize(jpeg, jpeg_len);
            }else {
                ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_THUMB, jpeg, jpeg_len);
            }

            ai_video_jpeg_image_free(&jpeg);
        }
    } break;

    case AI_UI_ACT_CLOSE_CAMER:
        ai_video_stop();
        ai_video_set_yuv_frame_flush_cb(NULL);
        ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_CLOSE, NULL, 0);
        break;

    case AI_UI_ACT_CAMERA_AI_ON:
        sg_ai_vision_enabled = true;
        break;

    case AI_UI_ACT_CAMERA_AI_OFF:
        sg_ai_vision_enabled = false;
        break;

    case AI_UI_ACT_OPEN_ALBUM:{
        char *album_name = ai_picture_get_album_name();
        
        if(album_name) {
            ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_OPEN, (uint8_t*)album_name, strlen(album_name));
        }
    } break;

    case AI_UI_ACT_VIEW_PREV_IMG:
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_VIEW_PREV, NULL, 0);
        break;

    case AI_UI_ACT_VIEW_NEXT_IMG:
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_VIEW_NEXT, NULL, 0);
        break;

    case AI_UI_ACT_VIEW_ALL_IMG:
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_VIEW_ALL, NULL, 0);
        break;

    case AI_UI_ACT_DELETE_IMG: {
        char *name = (char *)data;
        if(NULL == name) {
            PR_ERR("image name is null");
            break;
        }

        IMAGE_ALBUM_HANDLE hdl = image_album_find_by_name(ai_picture_get_album_name());
        image_album_delete(hdl, name);
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_RELOAD, NULL, 0);
    } break;

    case AI_UI_ACT_BATCH_DELETE_IMG: {
        if (NULL == data || len < sizeof(AI_UI_BATCH_DELETE_T)) {
            break;
        }
        IMAGE_ALBUM_HANDLE hdl = image_album_find_by_name(ai_picture_get_album_name());
        if (NULL == hdl) {
            PR_ERR("album not found for batch delete");
            break;
        }
        AI_UI_BATCH_DELETE_T *batch = (AI_UI_BATCH_DELETE_T *)data;
        uint32_t i;
        for (i = 0; i < batch->count && i < AI_UI_BATCH_DELETE_MAX; i++) {
            image_album_delete(hdl, batch->names[i]);
        }
    } break;

    case AI_UI_ACT_CLOSE_ALBUM:
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_CLOSE, NULL, 0);
        break;

    case AI_UI_ACT_OPEN_IMG_ATTACH_LIST:
        ai_ui_disp_msg_sync(AI_UI_DISP_ALBUM_SELECT_IMG, NULL, 0);
        break;

    case AI_UI_ACT_ADD_IMG_ATTACH: {
        ai_ui_disp_msg_sync(AI_UI_DISP_ADD_CHAT_ATTACH_IMG, data, strlen((char *)data));
        ai_picture_input_add_from_album((char *)data, NULL);
    } break;

    case AI_UI_ACT_DEL_IMG_ATTACH: {
        ai_picture_input_del_from_album((char *)data);
    } break;

    default:
        break;
    }
}


void app_ui_action_register(void)
{
    ai_ui_action_cb_register(__app_ui_action_handle);
}
