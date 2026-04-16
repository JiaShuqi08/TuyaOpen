/**
 * @file tdd_printer_dp48.c
 * @brief DP-48A ESC/POS thermal printer TDD driver implementation
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdd_printer_dp48.h"

#include "tal_log.h"
#include "tal_memory.h"
#include "tal_uart.h"

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    TDD_PRINTER_DP48_CFG_T cfg;
} TDD_PRINTER_DP48_HANDLE_T;

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __tdd_printer_dp48_open(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    OPERATE_RET rt = tal_uart_init(hdl->cfg.port_id, &hdl->cfg.uart_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("DP48 UART init failed: %d", rt);
    }
    return rt;
}

static OPERATE_RET __tdd_printer_dp48_write(TDD_PRINTER_HANDLE_T handle,
                                             const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    int ret = tal_uart_write(hdl->cfg.port_id, data, len);
    if (ret < 0) {
        PR_ERR("DP48 UART write error: %d", ret);
        return ret;
    }

    if ((uint32_t)ret != len) {
        PR_WARN("DP48 UART partial write, expect:%u real:%d", len, ret);
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

static OPERATE_RET __tdd_printer_dp48_close(TDD_PRINTER_HANDLE_T handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    return tal_uart_deinit(hdl->cfg.port_id);
}

OPERATE_RET tdd_printer_dp48_register(char *name, TDD_PRINTER_DP48_CFG_T *cfg)
{
    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(cfg, OPRT_INVALID_PARM);

    TDD_PRINTER_DP48_HANDLE_T *hdl =
        (TDD_PRINTER_DP48_HANDLE_T *)tal_malloc(sizeof(TDD_PRINTER_DP48_HANDLE_T));
    TUYA_CHECK_NULL_RETURN(hdl, OPRT_MALLOC_FAILED);
    memset(hdl, 0, sizeof(TDD_PRINTER_DP48_HANDLE_T));
    memcpy(&hdl->cfg, cfg, sizeof(TDD_PRINTER_DP48_CFG_T));

    TDD_PRINTER_INTFS_T dp48_intfs = {
        .open  = __tdd_printer_dp48_open,
        .write = __tdd_printer_dp48_write,
        .close = __tdd_printer_dp48_close,
    };

    TDD_PRINTER_DEV_INFO_T dev_info = {
        .dots_per_line  = DP48_DOTS_PER_LINE,
        .bytes_per_line = DP48_BYTES_PER_LINE,
        .encoding       = TDD_PRINTER_ENCODING_GBK,
        .protocol       = TDD_PRINTER_PROTOCOL_ESCPOS,
    };

    OPERATE_RET rt = tdl_printer_driver_register(name, &dp48_intfs, &dev_info,
                                                  (TDD_PRINTER_HANDLE_T)hdl);
    if (rt != OPRT_OK) {
        tal_free(hdl);
        hdl = NULL;
        PR_ERR("Failed to register DP48 printer: %d", rt);
    }

    return rt;
}
