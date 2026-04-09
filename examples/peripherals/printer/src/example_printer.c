/**
 * @file example_printer.c
 * @brief Thermal printer example for TuyaOpen (MTP02-DXD)
 * @version 0.4
 * @date 2026-03-31
 * @copyright Copyright (c) 2021-2026 Tuya Inc. All Rights Reserved.
 *
 * Demonstrates thermal printing of a JPEG image via TDL printer API:
 * - Decode JPEG to grayscale
 * - Floyd-Steinberg dithering to 1-bit bitmap
 * - Print on MTP02 thermal printer
 */

#include "tal_api.h"
#include "tkl_output.h"

#include "tdl_printer_manage.h"
#include "tdd_printer_mtp02.h"
#include "tal_image_jpeg_codec.h"
#include "board_com_api.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * External test image
 * --------------------------------------------------------------------------- */
extern const uint8_t test_flower_jpeg[];
extern const uint32_t TEST_FLOWER_JPEG_SIZE;

/* ---------------------------------------------------------------------------
 * File scope variables
 * --------------------------------------------------------------------------- */
STATIC TDL_PRINTER_HANDLE sg_printer_hdl;

/* ---------------------------------------------------------------------------
 * Function implementations
 * --------------------------------------------------------------------------- */

/**
 * @brief Printer event callback for status changes
 * @param[in] handle printer handle
 * @param[in] event event type
 * @param[in] data event-specific data
 * @param[in] arg user argument (unused)
 * @return none
 */
STATIC VOID_T __printer_event_cb(TDL_PRINTER_HANDLE handle, TDL_PRINTER_EVENT_E event,
                                  VOID_T *data, VOID_T *arg)
{
    switch (event) {
    case TDL_PRINTER_EVENT_PAPER_OUT:
        PR_WARN("EVENT: paper out");
        break;
    case TDL_PRINTER_EVENT_PAPER_IN:
        PR_NOTICE("EVENT: paper loaded");
        break;
    case TDL_PRINTER_EVENT_OVERHEATED:
        PR_WARN("EVENT: head overheated");
        break;
    case TDL_PRINTER_EVENT_TEMP_NORMAL:
        PR_NOTICE("EVENT: temperature normal");
        break;
    case TDL_PRINTER_EVENT_ERROR:
        PR_ERR("EVENT: printer error");
        break;
    default:
        break;
    }
}

/**
 * @brief Print a JPEG image on the thermal printer
 * @param[in] jpeg_data JPEG compressed data
 * @param[in] jpeg_size JPEG data length in bytes
 * @return OPRT_OK on success
 */
STATIC OPERATE_RET __print_jpeg_image(const uint8_t *jpeg_data, uint32_t jpeg_size)
{
    OPERATE_RET rt = OPRT_OK;

    TAL_IMAGE_JPEG_INFO_T info;
    TUYA_CALL_ERR_RETURN(tal_image_jpeg_get_info(jpeg_data, jpeg_size, &info));

    if (info.width != MTP02_DOTS_PER_LINE) {
        PR_ERR("image width %u != printer width %u", info.width, MTP02_DOTS_PER_LINE);
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("JPEG: %ux%u, components=%u", info.width, info.height, info.n_components);

    uint32_t bmp_size = (uint32_t)MTP02_BYTES_PER_LINE * info.height;
    uint8_t *bmp_buf = (uint8_t *)tal_malloc(bmp_size);
    if (bmp_buf == NULL) {
        PR_ERR("malloc bitmap buffer failed, need %u", bmp_size);
        return OPRT_MALLOC_FAILED;
    }

    TAL_IMAGE_JPEG_OUTPUT_T decode_out = {
        .out_buf      = bmp_buf,
        .out_buf_size = bmp_size,
        .out_width    = info.width,
        .out_height   = info.height,
    };
    PR_NOTICE("decoding & dithering %ux%u ...", info.width, info.height);
    rt = tal_image_jpeg_decode_bitmap(jpeg_data, jpeg_size, &decode_out, 128);
    if (rt != OPRT_OK) {
        PR_ERR("jpeg decode to bitmap failed: %d", rt);
        tal_free(bmp_buf);
        return rt;
    }

    rt = tdl_printer_start(sg_printer_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("printer start failed: %d", rt);
        tal_free(bmp_buf);
        return rt;
    }

    PR_NOTICE("printing %u lines ...", info.height);
    rt = tdl_printer_send(sg_printer_hdl, bmp_buf, bmp_size);
    if (rt != OPRT_OK) {
        PR_ERR("printer send failed: %d", rt);
    }

    tdl_printer_end(sg_printer_hdl);
    tal_free(bmp_buf);

    return rt;
}

/**
 * @brief Printer demo task
 * @return none
 */
STATIC VOID_T __example_printer_task(VOID_T)
{
    OPERATE_RET rt = OPRT_OK;

    rt = tdl_printer_find(PRINTER_NAME, &sg_printer_hdl);
    if (rt != OPRT_OK) {
        PR_ERR("printer \"%s\" not found: %d", PRINTER_NAME, rt);
        return;
    }

    TDL_PRINTER_OPEN_PARAM_T open_param = {
        .event_cb        = __printer_event_cb,
        .event_cb_arg    = NULL,
        .poll_interval_ms = 0,
    };
    rt = tdl_printer_open(sg_printer_hdl, &open_param);
    if (rt != OPRT_OK) {
        PR_ERR("printer open failed: %d", rt);
        return;
    }
    PR_NOTICE("printer opened");

    rt = __print_jpeg_image(test_flower_jpeg, TEST_FLOWER_JPEG_SIZE);
    if (rt != OPRT_OK) {
        PR_ERR("print image failed: %d", rt);
    } else {
        PR_NOTICE("print job done");
    }

    tdl_printer_close(sg_printer_hdl);
    PR_NOTICE("printer closed");
}

/**
 * @brief Application entry point
 * @return none
 */
void user_main(void)
{
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    board_register_hardware();

    __example_printer_task();

    while (1) {
        tal_system_sleep(1000);
    }
}

/**
 * @brief Linux main entry
 * @param[in] argc argument count
 * @param[in] argv argument values
 * @return none
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief RTOS application thread
 * @param[in] arg task parameters (unused)
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

/**
 * @brief RTOS application entry point
 * @return none
 */
void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = THREAD_PRIO_1;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
