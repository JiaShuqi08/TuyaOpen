/**
 * @file ai_picture_album.c
 * @brief ai_picture_album module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "ai_picture.h"
#include "ai_user_event.h"

#include "ai_picture_output.h"

#include "image_album_scan.h"

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    IMAGE_ALBUM_HANDLE       album_hdl;
    IMAGE_ALBUM_SCAN_HANDLE  album_scan_hdl;
    uint32_t                 scan_img_count;
}AI_PICTURE_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_CTX_T sg_picture_ctx;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Trim album to max image count, delete oldest images when exceeded
 * @param[in] album_handle Album handle
 * @param[in] max_cnt Maximum allowed image count
 * @return none
 */
STATIC VOID_T __album_trim_oldest(IMAGE_ALBUM_HANDLE album_handle, UINT32_T max_cnt)
{
    UINT32_T count = 0;

    if (OPRT_OK != image_album_get_committed_count(album_handle, &count)) {
        return;
    }

    while (count > max_cnt) {
        ALBUM_IMAGE_ITEM_T oldest;
        memset(&oldest, 0x00, sizeof(ALBUM_IMAGE_ITEM_T));
        if (OPRT_OK != image_album_get_next_item(album_handle, NULL, AI_PICTURE_GET_STORAGE_TP, &oldest)) {
            break;
        }
        PR_DEBUG("album full(%d/%d), delete oldest: %s", count, max_cnt, oldest.filename);
        image_album_delete(album_handle, oldest.filename);
        count--;
    }
}

OPERATE_RET ai_picture_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    IMAGE_ALBUM_INIT_CFG_T album_init = {
        .storage_mask    = AI_PICTURE_ALBUM_STORAGE_MASK,
        .recover_tp      = AI_PICTURE_ALBUM_RECOVER_TP,
    };

    TUYA_CALL_ERR_RETURN(image_album_init(COMP_AI_PICTURE_ALBUM_NAME,
                                          &album_init,
                                          &sg_picture_ctx.album_hdl));


    TUYA_CALL_ERR_LOG(ai_picture_output_set_size(COMP_AI_PICTURE_DEF_OUTPIUT_WIDTH,\
                                                 COMP_AI_PICTURE_DEF_OUTPIUT_HEIGHT));

    return rt;
}

IMAGE_ALBUM_HANDLE ai_picture_get_album_handle(void)
{
    return sg_picture_ctx.album_hdl;
}

OPERATE_RET ai_picture_save_to_album(uint8_t *picture, uint32_t len, char name[ALBUM_FILENAME_MAX_LEN + 1])
{
    OPERATE_RET rt = OPRT_OK;
    IMAGE_ALBUM_HANDLE album_handle = ai_picture_get_album_handle();

    if(NULL == album_handle) {
        PR_ERR("album handle is null");
        return OPRT_COM_ERROR;
    }

    if(NULL == picture || len == 0) {
        PR_ERR("picture:%p, total_len:%d", picture, len);
        return OPRT_INVALID_PARM;
    }


    char filename[ALBUM_FILENAME_MAX_LEN] = {0};
    SYS_TICK_T timestamp = tal_time_get_posix_ms();
    snprintf(filename, sizeof(filename), "ai_pic_%llu", timestamp);

    ALBUM_IMAGE_SAVE_INFO_T info = {
        .filename  = filename,
        .format    = ALBUM_IMAGE_FORMAT_JPEG,
        .file_data = picture,
        .file_size = len,
        .timestamp = timestamp,
    };

    TUYA_CALL_ERR_RETURN(image_album_save(album_handle, &info));

    if(name) {
        strncpy(name, info.filename, ALBUM_FILENAME_MAX_LEN);
    }

    __album_trim_oldest(album_handle, COMP_AI_PICTURE_ALBUM_MAX_IMAGE_CNT);

    ai_user_event_notify(AI_USER_EVT_SAVE_PICTURE, filename);

    return OPRT_OK;
}

OPERATE_RET ai_picture_retain_locked_from_album(char *img, AI_PICTURE_INFO_T *pic)
{
    OPERATE_RET rt = OPRT_OK;
    IMAGE_ALBUM_HANDLE album_handle = ai_picture_get_album_handle();
    if(NULL == album_handle) {
        PR_ERR("album handle is null");
        return OPRT_COM_ERROR;
    }

    if(NULL == pic) {
        return OPRT_INVALID_PARM;
    }

    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0x00, sizeof(ALBUM_IMAGE_ITEM_T));
    TUYA_CALL_ERR_RETURN(image_album_item_retain_locked(album_handle, (const char *)img,
                                                        AI_PICTURE_GET_STORAGE_TP, &item));

    strncpy(pic->name, item.filename, ALBUM_FILENAME_MAX_LEN);
    pic->width  = item.attr.width;
    pic->height = item.attr.height;
    pic->len    = item.attr.file_size;
    pic->data   = (uint8_t *)item.path;

    return OPRT_OK;
}

OPERATE_RET ai_picture_release_locked_from_album(char *img)
{
    OPERATE_RET rt  = OPRT_OK;
    IMAGE_ALBUM_HANDLE album_handle = ai_picture_get_album_handle();
    if(NULL == album_handle) {
        PR_ERR("album handle is null");
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(image_album_item_release_locked(album_handle, (const char *)img)); 

    return OPRT_OK;
}

OPERATE_RET ai_picture_scan_album_start(void)
{
    OPERATE_RET rt = OPRT_OK;

    if(sg_picture_ctx.album_scan_hdl) {
        PR_ERR("scan not end");
        return OPRT_COM_ERROR;
    }

    TUYA_CALL_ERR_RETURN(image_album_scan_init(COMP_AI_PICTURE_ALBUM_NAME,
                                                AI_PICTURE_GET_STORAGE_TP,
                                               &sg_picture_ctx.album_scan_hdl));

    sg_picture_ctx.scan_img_count = image_album_scan_get_count(sg_picture_ctx.album_scan_hdl);

    return rt;
}

uint32_t ai_picture_get_scan_count(void)
{
    return sg_picture_ctx.scan_img_count;
}

OPERATE_RET ai_picture_scan_next(AI_PICTURE_INFO_T *pic)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == sg_picture_ctx.album_scan_hdl) {
        PR_ERR("scan not end");
        return OPRT_COM_ERROR;
    }

    ALBUM_IMAGE_ITEM_T item;
    memset(&item, 0x00, sizeof(ALBUM_IMAGE_ITEM_T));
    TUYA_CALL_ERR_RETURN(image_album_scan_next(sg_picture_ctx.album_scan_hdl, &item));

    strncpy(pic->name, item.filename, ALBUM_FILENAME_MAX_LEN);
    pic->width  = item.attr.width;
    pic->height = item.attr.height;
    pic->len    = item.attr.file_size;
    pic->data   = (uint8_t *)item.path;

    return rt;

}

OPERATE_RET ai_picture_scan_album_end(void)
{
    OPERATE_RET rt = OPRT_OK;

    if(NULL == sg_picture_ctx.album_scan_hdl) {
        return OPRT_OK;
    }

    TUYA_CALL_ERR_RETURN(image_album_scan_deinit(sg_picture_ctx.album_scan_hdl));
    sg_picture_ctx.album_scan_hdl = NULL;
    sg_picture_ctx.scan_img_count = 0;

    return OPRT_OK;
}