/**
 * @file ai_picture_input.c
 * @brief ai_picture_input module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */
#include "string.h"
#include "tal_api.h"
#include "tuya_ai_agent.h"
#include "ai_user_event.h"

#include "ai_picture.h"
#include "ai_picture_input.h"


#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/


/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_INFO_T  sg_pic_input[AI_PICTURE_INPUT_MAX_NUM];
static int8_t             sg_pic_num = 0;

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET ai_picture_input_add_from_album(char *filename)
{
    OPERATE_RET rt = OPRT_OK;

    if (sg_pic_num >= AI_PICTURE_INPUT_MAX_NUM) {
        return OPRT_EXCEED_UPPER_LIMIT;
    }

    TUYA_CALL_ERR_RETURN(ai_picture_retain_locked_from_album(filename, &sg_pic_input[sg_pic_num]));

    PR_DEBUG("ai_picture_input_add name:%s, width:%d, height:%d, file_size:%d",
             sg_pic_input[sg_pic_num].name, sg_pic_input[sg_pic_num].width,
             sg_pic_input[sg_pic_num].height, sg_pic_input[sg_pic_num].len);

    sg_pic_num++;

    return OPRT_OK;
}

OPERATE_RET ai_picture_input_del_from_album(char *filename)
{
    OPERATE_RET rt = OPRT_OK;
    AI_PICTURE_INFO_T *pic = NULL;
    uint8_t idx = 0;

    for(int i=0; i<sg_pic_num; i++) {
        if(0 == strcmp(filename, sg_pic_input[i].name)) {
            pic = &sg_pic_input[i];
            idx = i;
            break;
        }
    }

    if(NULL == pic) {
        return OPRT_NOT_FOUND;
    }

    TUYA_CALL_ERR_RETURN(ai_picture_release_locked_from_album(pic->name));

    uint32_t move_len = (AI_PICTURE_INPUT_MAX_NUM - (idx+1))*sizeof(AI_PICTURE_INFO_T);
    if(move_len) {
        memmove(&sg_pic_input[idx], &sg_pic_input[idx+1], move_len);
    }

    sg_pic_num--;

    return OPRT_OK;
}

static void __ai_picture_input_clear(void)
{
    OPERATE_RET rt = OPRT_OK;

    if(0 == sg_pic_num) {
        return;
    }

    for(int i=0; i<sg_pic_num; i++) {
        AI_PICTURE_INFO_T *pic = &sg_pic_input[i];

        TUYA_CALL_ERR_LOG(ai_picture_release_locked_from_album(pic->name));
    }

    memset(sg_pic_input, 0x00, sizeof(sg_pic_input));
    sg_pic_num = 0;
}

/**
 * @brief Send all valid pictures in input slots to the AI agent.
 */
OPERATE_RET ai_picture_input_send(void)
{
    OPERATE_RET rt = OPRT_OK;
    bool        has_data = false;

    for (uint32_t idx = 0; idx < sg_pic_num; idx++) {
        AI_PICTURE_INFO_T *pic = &sg_pic_input[idx];
        uint64_t timestamp = 0;

        has_data = true;
        timestamp = tal_system_get_millisecond();
        PR_DEBUG("ai_picture_input_send name:%s, file_size:%d", pic->name, pic->len);

        TUYA_CALL_ERR_RETURN(tuya_ai_image_input(timestamp, (uint8_t *)pic->data,
                                                 pic->len, pic->len));
    }

    __ai_picture_input_clear();

    if(has_data) {
        ai_user_event_notify(AI_USER_EVT_SEND_PICTURE_END, NULL);
    }

    return OPRT_OK;
}

/**
 * @brief Get the number of picture slots that have been populated.
 */
uint32_t ai_picture_input_get_num(void)
{
    return sg_pic_num;
}

#ifdef __cplusplus
}
#endif
