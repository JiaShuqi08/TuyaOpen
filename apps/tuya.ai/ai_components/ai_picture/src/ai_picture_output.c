/**
 * @file ai_picture_output.c
 * @brief Picture output module implementation.
 *        Accumulates streamed JPEG chunks into a contiguous buffer,
 *        saves the completed picture to the album, and notifies listeners.
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
#define AI_PICTURE_OUTPUT_WIDTH_KEY  "sys.device.img_resize.width"
#define AI_PICTURE_OUTPUT_HEIGHT_KEY "sys.device.img_resize.height"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    bool     is_start;
    uint32_t total_size;
    uint32_t offset;
    uint8_t *acc_buf;
} AI_PICTURE_STREAM_T;

typedef struct {
    uint16_t set_width;
    uint16_t set_height;
} AI_PICTURE_OUTPUT_CTX_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static AI_PICTURE_OUTPUT_CTX_T sg_picture_output;
static AI_PICTURE_STREAM_T     sg_ai_pic_stream;

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

/**
 * @brief Event callback to push output picture dimensions to AI agent custom params
 * @param[in] data unused
 * @return 0 on success
 */
static int __set_output_picture_size_cb(void *data)
{
    (void)data;

    return 0;
}

/**
 * @brief Set the desired output picture dimensions for AI image generation
 * @param[in] width desired width in pixels
 * @param[in] height desired height in pixels
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_set_size(uint16_t width, uint16_t height)
{
    OPERATE_RET rt = OPRT_OK;

    sg_picture_output.set_width  = width;
    sg_picture_output.set_height = height;

    TUYA_CALL_ERR_LOG(tal_event_subscribe(EVENT_AI_SESSION_NEW,
                                          "set_output_picture_size",
                                          __set_output_picture_size_cb,
                                          SUBSCRIBE_TYPE_NORMAL));

    return rt;
}

/**
 * @brief Accumulate a JPEG chunk and save to album when all chunks are received
 * @param[in] data JPEG chunk data
 * @param[in] len chunk length in bytes
 * @param[in] total_len total expected JPEG size in bytes
 * @return OPRT_OK on success
 */
OPERATE_RET ai_picture_output_save_to_album(uint8_t *data, uint32_t len, uint32_t total_len)
{
    OPERATE_RET rt = OPRT_OK;

    if (NULL == data || len == 0 || total_len == 0) {
        PR_ERR("invalid param, data:%p, len:%u, total_len:%u", data, len, total_len);
        return OPRT_INVALID_PARM;
    }

    if (false == sg_ai_pic_stream.is_start) {
        PR_NOTICE("[pic_chain] start accumulating, total_len:%u", total_len);
        sg_ai_pic_stream.acc_buf = (uint8_t *)Malloc((size_t)total_len);
        if (sg_ai_pic_stream.acc_buf == NULL) {
            PR_ERR("[pic_chain] malloc %u bytes failed", total_len);
            return OPRT_MALLOC_FAILED;
        }

        sg_ai_pic_stream.total_size = total_len;
        sg_ai_pic_stream.offset     = 0;
        sg_ai_pic_stream.is_start   = true;
    } else {
        if (sg_ai_pic_stream.total_size != total_len) {
            PR_ERR("get total size:%u is different %u", total_len, sg_ai_pic_stream.total_size);
            __ai_picture_output_accum_reset();
            return OPRT_COM_ERROR;
        }
    }

    if (len > total_len - sg_ai_pic_stream.offset) {
        PR_ERR("chunk overflow: offset=%u len=%u total=%u", sg_ai_pic_stream.offset, len, sg_ai_pic_stream.total_size);
        __ai_picture_output_accum_reset();
        return OPRT_BUFFER_NOT_ENOUGH;
    }

    memcpy(sg_ai_pic_stream.acc_buf + sg_ai_pic_stream.offset, data, (size_t)len);
    sg_ai_pic_stream.offset += len;
    PR_DEBUG("[pic_chain] chunk accumulated, offset:%u/%u", sg_ai_pic_stream.offset, sg_ai_pic_stream.total_size);

    if (sg_ai_pic_stream.offset >= sg_ai_pic_stream.total_size) {
        PR_NOTICE("[pic_chain] all chunks received, total:%u, saving to album", sg_ai_pic_stream.total_size);
        char name[AI_PICTURE_NAME_MAX_LEN + 1] = {0};

        rt = ai_picture_save_to_album(sg_ai_pic_stream.acc_buf, sg_ai_pic_stream.total_size, name);
        if (rt != OPRT_OK) {
            PR_ERR("[pic_chain] save to album failed, rt:%d", rt);
        } else {
            PR_NOTICE("[pic_chain] save to album success, name:%s, notify ACCEPT_PICTURE", name);
            ai_user_event_notify(AI_USER_EVT_ACCEPT_PICTURE, name);
        }

        __ai_picture_output_accum_reset();
    }

    return rt;
}
