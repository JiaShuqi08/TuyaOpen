/**
 * @file ai_picture_input.h
 * @brief ai_picture_input module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#ifndef __AI_PICTURE_INPUT_H__
#define __AI_PICTURE_INPUT_H__

#include "tuya_cloud_types.h"
#include "ai_picture.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_INPUT_MAX_NUM 3

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_picture_input_add_from_album(char *filename);

OPERATE_RET ai_picture_input_del_from_album(char *filename);

OPERATE_RET ai_picture_input_send(void);

uint32_t ai_picture_input_get_num(void);


#ifdef __cplusplus
}
#endif

#endif /* __AI_PICTURE_INPUT_H__ */
