/**
 * @file ai_picture_type.h
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_PICTURE_TYPE_H__
#define __AI_PICTURE_TYPE_H__

#include "tuya_cloud_types.h"
#include "image_album.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_ALBUM_STORAGE_MASK IMAGE_ALBUM_STORAGE_TP_ALL

/**
 * @brief Backend selector for @ref image_album_get_next_item / @ref image_album_item_retain_locked paths.
 *        AI picture uses in-memory payload pointer from the memory backend.
 */
#define AI_PICTURE_GET_STORAGE_TP IMAGE_ALBUM_STORAGE_TP_MEMORY

/**
 * @brief Storage backend used as source of truth for image list recovery at init.
 *        Set to @ref IMAGE_ALBUM_STORAGE_TP_SD when persistent SD storage is enabled,
 *        0 when no persistent storage (image list starts empty on each boot).
 */
#if defined(ENABLE_IMAGE_ALBUM_STORAGE_SD) && (ENABLE_IMAGE_ALBUM_STORAGE_SD)
#define AI_PICTURE_ALBUM_RECOVER_TP IMAGE_ALBUM_STORAGE_TP_SD
#else
#define AI_PICTURE_ALBUM_RECOVER_TP 0
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char      name[ALBUM_FILENAME_MAX_LEN + 1];
    uint16_t  width;
    uint16_t  height;
    uint32_t  len;
    uint8_t  *data;
}AI_PICTURE_INFO_T;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_picture_init(void);

OPERATE_RET ai_picture_save_to_album(uint8_t *picture, uint32_t len, char name[ALBUM_FILENAME_MAX_LEN + 1]);

OPERATE_RET ai_picture_retain_locked_from_album(char *img, AI_PICTURE_INFO_T *pic);

OPERATE_RET ai_picture_release_locked_from_album(char *img);

OPERATE_RET ai_picture_scan_album_start(void);

uint32_t ai_picture_get_scan_count(void);

OPERATE_RET ai_picture_scan_next(AI_PICTURE_INFO_T *pic);

OPERATE_RET ai_picture_scan_album_end(void);


#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_TYPE_H__ */
