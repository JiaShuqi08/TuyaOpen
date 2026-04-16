/**
 * @file app_ui_action.c
 * @brief app_ui_action module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "tal_api.h"
#include <string.h>

#if defined(ENABLE_COMP_AI_DISPLAY) && (ENABLE_COMP_AI_DISPLAY == 1)
#include "ai_ui_manage.h"

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
#include "image_album.h"
#include "ai_picture.h"
#include "ai_picture_input.h"
#endif

#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
#include "ai_video_input.h"
#endif

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
#include "tdl_printer_manage.h"
#include "tal_image_jpeg_codec.h"
#include "image_album.h"
#include "ai_picture.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
static OPERATE_RET __print_img_from_album(const char *filename)
{
    TDL_PRINTER_HANDLE printer_hdl = NULL;
    OPERATE_RET rt = tdl_printer_find(PRINTER_NAME, &printer_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("print: printer \"%s\" not found, rt:%d", PRINTER_NAME, rt);
        return rt;
    }

    rt = tdl_printer_open(printer_hdl, NULL);
    if (rt != OPRT_OK) {
        PR_ERR("print: printer open failed, rt:%d", rt);
        return rt;
    }

    TDL_PRINTER_DEV_INFO_T dev_info = {0};
    tdl_printer_get_dev_info(printer_hdl, &dev_info);
    uint16_t print_width = (uint16_t)dev_info.dots_per_line;
    if (0 == print_width) {
        PR_ERR("print: invalid dots_per_line");
        tdl_printer_close(printer_hdl);
        return OPRT_INVALID_PARM;
    }

    IMAGE_ALBUM_HANDLE album_hdl = image_album_find_by_name(ai_picture_get_album_name());
    if (NULL == album_hdl) {
        PR_ERR("print: album not found");
        tdl_printer_close(printer_hdl);
        return OPRT_INVALID_PARM;
    }

    uint8_t *jpeg_data = NULL;
    size_t   jpeg_size = 0;
    rt = image_album_read(album_hdl, filename, 0, &jpeg_data, &jpeg_size);
    if (rt != OPRT_OK || NULL == jpeg_data) {
        PR_ERR("print: image_album_read \"%s\" failed, rt:%d", filename, rt);
        tdl_printer_close(printer_hdl);
        return (rt != OPRT_OK) ? rt : OPRT_INVALID_PARM;
    }

    TAL_IMAGE_JPEG_INFO_T jpeg_info = {0};
    rt = tal_image_jpeg_get_info(jpeg_data, (uint32_t)jpeg_size, &jpeg_info);
    if (rt != OPRT_OK || 0 == jpeg_info.width) {
        PR_ERR("print: jpeg_get_info failed, rt:%d", rt);
        image_album_free_file_data(jpeg_data);
        tdl_printer_close(printer_hdl);
        return (rt != OPRT_OK) ? rt : OPRT_INVALID_PARM;
    }

    uint16_t src_width  = (uint16_t)jpeg_info.width;
    uint16_t src_height = (uint16_t)jpeg_info.height;

    /* Decode directly to printer dimensions (scale down if image is wider) */
    uint16_t out_w = (src_width > print_width) ? print_width : src_width;
    uint16_t out_h = (src_width > print_width)
                     ? (uint16_t)((uint32_t)src_height * print_width / src_width)
                     : src_height;
    if (out_h == 0) out_h = 1;
    uint32_t bitmap_bytes = ((uint32_t)(out_w + 7u) / 8u) * out_h;

    uint8_t *bitmap_buf = (uint8_t *)tal_malloc(bitmap_bytes);
    if (NULL == bitmap_buf) {
        PR_ERR("print: malloc %u bytes for bitmap failed", bitmap_bytes);
        image_album_free_file_data(jpeg_data);
        tdl_printer_close(printer_hdl);
        return OPRT_MALLOC_FAILED;
    }

    TAL_IMAGE_JPEG_OUTPUT_T out = {
        .out_buf      = bitmap_buf,
        .out_buf_size = bitmap_bytes,
        .out_width    = out_w,
        .out_height   = out_h,
    };
    rt = tal_image_jpeg_decode_bitmap(jpeg_data, (uint32_t)jpeg_size, &out, 128);
    image_album_free_file_data(jpeg_data);

    if (rt != OPRT_OK) {
        PR_ERR("print: jpeg_decode_bitmap failed, rt:%d", rt);
        tal_free(bitmap_buf);
        tdl_printer_close(printer_hdl);
        return rt;
    }

    rt = tdl_printer_send_bitmap(printer_hdl, 0, out_w, out_h, bitmap_buf);
    tal_free(bitmap_buf);
    if (rt != OPRT_OK) {
        PR_ERR("print: send_bitmap failed, rt:%d", rt);
    }

    tdl_printer_close(printer_hdl);
    return rt;
}
#endif /* ENABLE_PRINTER */


#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
static bool sg_ai_vision_enabled = false;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
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
#endif /* ENABLE_COMP_AI_VIDEO */


static void __app_ui_action_handle(AI_UI_ACTION_E action, uint8_t *data, uint32_t len)
{
    switch (action) {
#if defined(ENABLE_COMP_AI_VIDEO) && (ENABLE_COMP_AI_VIDEO == 1)
    case AI_UI_ACT_OPEN_CAMERA:
        sg_ai_vision_enabled = false;
        ai_video_set_yuv_frame_flush_cb(__display_camera_yuv_fram);
        ai_video_start();
        ai_ui_disp_msg_sync(AI_UI_DISP_CAMERA_OPEN, NULL, 0);
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
            } else {
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
#endif /* ENABLE_COMP_AI_VIDEO */

#if defined(ENABLE_COMP_AI_PICTURE) && (ENABLE_COMP_AI_PICTURE == 1)
    case AI_UI_ACT_OPEN_ALBUM: {
        char *album_name = ai_picture_get_album_name();
        if (album_name) {
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
        if (NULL == name) {
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

#if defined(ENABLE_PRINTER) && (ENABLE_PRINTER == 1)
    case AI_UI_ACT_PRINT_IMG:
        if (NULL == data || 0 == len) {
            PR_ERR("print: filename is null");
            break;
        }
        {
            OPERATE_RET print_rt = __print_img_from_album((const char *)data);
            int32_t result = (int32_t)print_rt;
            ai_ui_disp_msg(AI_UI_DISP_PRINT_RESULT, (uint8_t *)&result, sizeof(result));
        }
        break;
#endif /* ENABLE_PRINTER */

#endif /* ENABLE_IMAGE_ALBUM */
    default:
        break;
    }
}


void app_ui_action_register(void)
{
    ai_ui_action_cb_register(__app_ui_action_handle);
}

#endif