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

    OPERATE_RET rt = tal_uart_deinit(hdl->cfg.port_id);
    if (rt != OPRT_OK) {
        PR_DEBUG("DP48 pre-open deinit(port %d): rt=%d", hdl->cfg.port_id, rt);
    }

    rt = tal_uart_init(hdl->cfg.port_id, &hdl->cfg.uart_cfg);
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

static OPERATE_RET __tdd_printer_dp48_paper_feed(TDD_PRINTER_HANDLE_T handle, uint32_t lines)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    if (lines == 0) {
        return OPRT_OK;
    }

    TDD_PRINTER_DP48_HANDLE_T *hdl = (TDD_PRINTER_DP48_HANDLE_T *)handle;

    /* ESC d n — feed n lines */
    uint8_t n = (lines > 255u) ? 255u : (uint8_t)lines;
    uint8_t cmd[] = {0x1B, 0x64, n};

    int ret = tal_uart_write(hdl->cfg.port_id, cmd, sizeof(cmd));
    return (ret == (int)sizeof(cmd)) ? OPRT_OK : OPRT_COM_ERROR;
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

    /* TAL requires a non-zero rx ring buffer even for TX-only devices */
    if (hdl->cfg.uart_cfg.rx_buffer_size == 0) {
        hdl->cfg.uart_cfg.rx_buffer_size = 64;
    }

    TDD_PRINTER_INTFS_T dp48_intfs = {
        .open        = __tdd_printer_dp48_open,
        .write       = __tdd_printer_dp48_write,
        .close       = __tdd_printer_dp48_close,
        .paper_feed  = __tdd_printer_dp48_paper_feed,
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
