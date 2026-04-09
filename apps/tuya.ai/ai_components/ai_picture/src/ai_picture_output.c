/**
 * @file ai_picture_output.c
 * @version 0.1
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 */

#include "tal_api.h"
#include "tuya_ai_agent.h"
#include "ai_user_event.h"

#include "ai_picture_output.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define AI_PICTURE_OUTPUT_WIDTH_KEY     "sys.device.img_resize.width"
#define AI_PICTURE_OUTPUT_HEIGHT_KEY    "sys.device.img_resize.height"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool      is_start;
    uint32_t  total_size;
    uint32_t  offset;
    uint8_t  *acc_buf;
} AI_PICTURE_STREAM_T;

typedef struct {
    uint16_t                 set_width;
    uint16_t                 set_height;
} AI_PICTURE_OUTPUT_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_OUTPUT_CTX_T  sg_picture_output;
static AI_PICTURE_STREAM_T      sg_ai_pic_stream;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Free partial JPEG accumulator and clear session state
 * @return none
 */
static void __ai_picture_output_accum_reset(void)
{
    if (sg_ai_pic_stream.acc_buf != NULL) {
        Free(sg_ai_pic_stream.acc_buf);
    }
    memset(&sg_ai_pic_stream, 0, sizeof(AI_PICTURE_STREAM_T));
}

static int __set_output_picture_size_cb(void *data)
{
    (void)data;

    OPERATE_RET rt = OPRT_OK;
    AI_CUSTOM_PAR_ITEM_T par_item_arr[2] = {0};

    par_item_arr[0].key = AI_PICTURE_OUTPUT_WIDTH_KEY;
    par_item_arr[0].type = AI_AGENT_PAR_VAL_TYPE_INT;
    par_item_arr[0].value.int_val = sg_picture_output.set_width;
    par_item_arr[0].is_effect_once = false;

    par_item_arr[1].key = AI_PICTURE_OUTPUT_HEIGHT_KEY;
    par_item_arr[1].type = AI_AGENT_PAR_VAL_TYPE_INT;
    par_item_arr[1].value.int_val = sg_picture_output.set_height;
    par_item_arr[1].is_effect_once = false;

    TUYA_CALL_ERR_LOG(tuya_ai_agent_event_custom_param(NULL, \
                                                      CNTSOF(par_item_arr),\
                                                      par_item_arr));
    return 0;
}

OPERATE_RET ai_picture_output_set_size(uint16_t width, uint16_t height)
{
    OPERATE_RET rt = OPRT_OK;

    sg_picture_output.set_width  = width;
    sg_picture_output.set_height = height;

    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_AI_SESSION_NEW, \
                                         "set_output_picture_size",\
                                         __set_output_picture_size_cb,\
                                          SUBSCRIBE_TYPE_NORMAL));

    return rt;
}

OPERATE_RET ai_picture_output_save_to_album(uint8_t *data, uint32_t len, uint32_t total_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == data || len == 0 || total_len == 0) {
        PR_ERR("invalid param, data:%p, len:%u, total_len:%u", data, len, total_len);
        return OPRT_INVALID_PARM;
    }

    if (false == sg_ai_pic_stream.is_start) {
        sg_ai_pic_stream.acc_buf = (uint8_t *)Malloc((size_t)total_len);
        if (sg_ai_pic_stream.acc_buf == NULL) {
            return OPRT_MALLOC_FAILED;
        }

        sg_ai_pic_stream.total_size = total_len;
        sg_ai_pic_stream.offset = 0;
        sg_ai_pic_stream.is_start = true;
    } else {
        if (sg_ai_pic_stream.total_size != total_len) {
            PR_ERR("get total size:%u is different %u", total_len, sg_ai_pic_stream.total_size);
            __ai_picture_output_accum_reset();
            return OPRT_COM_ERROR;
        }
    }

    if (len > total_len - sg_ai_pic_stream.offset) {
        PR_ERR("chunk overflow: offset=%u len=%u total=%u",
               sg_ai_pic_stream.offset, len, sg_ai_pic_stream.total_size);
        __ai_picture_output_accum_reset();
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    memcpy(sg_ai_pic_stream.acc_buf + sg_ai_pic_stream.offset, data, (size_t)len);
    sg_ai_pic_stream.offset += len;

    if (sg_ai_pic_stream.offset >= sg_ai_pic_stream.total_size) {
        char name[ALBUM_FILENAME_MAX_LEN + 1] = {0};

        rt = ai_picture_save_to_album(sg_ai_pic_stream.acc_buf, sg_ai_pic_stream.total_size, name);
        if (rt != OPRT_OK) {
            PR_ERR("ai_picture_save_to_album failed, rt:%d", rt);
        }

        __ai_picture_output_accum_reset();
    }

    return rt;
}
