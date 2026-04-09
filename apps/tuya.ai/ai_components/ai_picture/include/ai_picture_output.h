/**
 * @file ai_picture_output.h
 * @brief ai_picture_output module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_PICTURE_OUTPUT_H__
#define __AI_PICTURE_OUTPUT_H__

#include "tuya_cloud_types.h"
#include "ai_picture.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_OUTPUT_MAX_NUM 12


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_picture_output_set_size(uint16_t width, uint16_t height);

OPERATE_RET ai_picture_output_save_to_album(uint8_t *data, uint32_t len, uint32_t total_len);

#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_OUTPUT_H__ */
