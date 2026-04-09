/**
 * @file tdl_printer_manage.c
 * @brief Printer manager implementation
 * @version 0.3
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tdl_printer_manage.h"
#include "tdl_printer_driver.h"

#include "tal_api.h"
#include "tuya_list.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TDL_PRINTER_MAGIC 0x5450524E

#ifndef CONFIG_PRINTER_POLL_INTERVAL_MS
#define CONFIG_PRINTER_POLL_INTERVAL_MS 500
#endif

#ifndef CONFIG_PRINTER_STACK_SIZE
#define CONFIG_PRINTER_STACK_SIZE 4096
#endif

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef uint8_t TDL_PRINTER_STATUS_T;
#define TDL_PRINTER_STATUS_UNINIT 0x00
#define TDL_PRINTER_STATUS_INITED 0x01

typedef struct {
    LIST_HEAD node;

    int magic;
    char name[TDL_PRINTER_NAME_MAX_LEN];

    TDL_PRINTER_STATUS_T status;
    TDD_PRINTER_HANDLE_T tdd_handle;
    TDD_PRINTER_INTFS_T intfs;
    TDD_PRINTER_DEV_INFO_T dev_info;

    TDL_PRINTER_EVENT_CB event_cb;
    void *event_cb_arg;
    uint32_t poll_interval_ms;
    THREAD_HANDLE monitor_thread;
    BOOL_T monitor_running;
    TDD_PRINTER_DEV_STATUS_T last_dev_status;
} TDL_PRINTER_T, TDL_PRINTER_NODE_T;

typedef struct {
    LIST_HEAD head;
    MUTEX_HANDLE mutex;
} TDL_PRINTER_LIST_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_PRINTER_LIST_T g_printer_list = {.head = LIST_HEAD_INIT(g_printer_list.head), .mutex = NULL};

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Dispatch a single event to the registered callback
 * @param[in] node printer node
 * @param[in] event event type
 * @return none
 */
static void __dispatch_event(TDL_PRINTER_NODE_T *node, TDL_PRINTER_EVENT_E event, void *data)
{
    if (node->event_cb) {
        node->event_cb((TDL_PRINTER_HANDLE)node, event, data, node->event_cb_arg);
    }
}

/**
 * @brief Compare current and previous status, fire events on changes
 * @param[in] node printer node
 * @param[in] cur current device status from TDD
 * @return none
 */
static void __check_and_notify(TDL_PRINTER_NODE_T *node, const TDD_PRINTER_DEV_STATUS_T *cur)
{
    TDD_PRINTER_STATUS_FLAG_T changed = cur->flags ^ node->last_dev_status.flags;
    TDL_PRINTER_EVENT_E event = TDL_PRINTER_EVENT_NONE;

    if (changed & TDD_PRINTER_FLAG_PAPER_OUT) {
        event = (cur->flags & TDD_PRINTER_FLAG_PAPER_OUT) ? TDL_PRINTER_EVENT_PAPER_OUT : TDL_PRINTER_EVENT_PAPER_IN;
        __dispatch_event(node, event, NULL);
    }

    if (changed & TDD_PRINTER_FLAG_OVERHEATED) {
        event = (cur->flags & TDD_PRINTER_FLAG_OVERHEATED) ? TDL_PRINTER_EVENT_OVERHEATED : TDL_PRINTER_EVENT_TEMP_NORMAL;
        __dispatch_event(node, event, NULL);
    }

    if ((changed & TDD_PRINTER_FLAG_ERROR) && (cur->flags & TDD_PRINTER_FLAG_ERROR)) {
        __dispatch_event(node, TDL_PRINTER_EVENT_ERROR, NULL);
    }

    node->last_dev_status = *cur;
}

/**
 * @brief Monitoring thread entry, polls TDD get_status periodically
 * @param[in] args pointer to TDL_PRINTER_NODE_T
 * @return none
 */
static void __printer_monitor_thread(void *args)
{
    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)args;

    if (node->intfs.get_status) {
        node->intfs.get_status(node->tdd_handle, &node->last_dev_status);
    }

    while (node->monitor_running) {
        tal_system_sleep(node->poll_interval_ms);

        if (!node->monitor_running) {
            break;
        }

        if (NULL == node->intfs.get_status) {
            continue;
        }

        TDD_PRINTER_DEV_STATUS_T cur_status;
        if (node->intfs.get_status(node->tdd_handle, &cur_status) == OPRT_OK) {
            __check_and_notify(node, &cur_status);
        }
    }
}

/**
 * @brief Start the status monitoring thread
 * @param[in] node printer node
 * @return OPRT_OK on success
 */
static OPERATE_RET __start_monitor(TDL_PRINTER_NODE_T *node)
{
    node->monitor_running = TRUE;
    memset(&node->last_dev_status, 0, sizeof(TDD_PRINTER_DEV_STATUS_T));

    char thrd_name[TAL_THREAD_MAX_NAME_LEN];
    snprintf(thrd_name, sizeof(thrd_name), "prn_%s", node->name);

    THREAD_CFG_T cfg = {
        .stackDepth = CONFIG_PRINTER_STACK_SIZE,
        .priority   = THREAD_PRIO_3,
        .thrdname   = thrd_name,
    };

    OPERATE_RET rt = tal_thread_create_and_start(&node->monitor_thread,
                                                  NULL, NULL,
                                                  __printer_monitor_thread,
                                                  node, &cfg);
    if (rt != OPRT_OK) {
        node->monitor_running = FALSE;
        PR_ERR("monitor thread create failed: %d", rt);
    }

    return rt;
}

/**
 * @brief Stop the status monitoring thread
 * @param[in] node printer node
 * @return none
 */
static void __stop_monitor(TDL_PRINTER_NODE_T *node)
{
    if (NULL == node->monitor_thread) {
        return;
    }

    node->monitor_running = FALSE;
    tal_thread_delete(node->monitor_thread);
    node->monitor_thread = NULL;
}

/**
 * @brief Find a registered printer by name
 * @param[in] name printer name
 * @param[out] handle output printer handle
 * @return OPRT_OK on success, OPRT_NOT_FOUND if not registered
 */
OPERATE_RET tdl_printer_find(const char *name, TDL_PRINTER_HANDLE *handle)
{
    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    if (NULL == g_printer_list.mutex) {
        return OPRT_COM_ERROR;
    }

    tal_mutex_lock(g_printer_list.mutex);

    struct tuya_list_head *p = NULL;
    TDL_PRINTER_NODE_T *printer = NULL;

    tuya_list_for_each(p, &g_printer_list.head)
    {
        printer = tuya_list_entry(p, TDL_PRINTER_NODE_T, node);
        if (printer->magic != TDL_PRINTER_MAGIC) {
            tal_mutex_unlock(g_printer_list.mutex);
            return OPRT_COM_ERROR;
        }

        if (strcmp(name, printer->name) == 0) {
            *handle = (TDL_PRINTER_HANDLE)printer;
            tal_mutex_unlock(g_printer_list.mutex);
            return OPRT_OK;
        }
    }

    tal_mutex_unlock(g_printer_list.mutex);

    return OPRT_NOT_FOUND;
}

/**
 * @brief Open the printer and optionally start status monitoring
 * @param[in] handle printer handle
 * @param[in] param open parameters (callback, poll interval). NULL if
 *            no event monitoring is needed.
 * @return OPRT_OK on success
 * @note When param is provided with a non-NULL event_cb, a background
 *       thread is created to poll the TDD driver's get_status at the
 *       specified poll_interval_ms. If poll_interval_ms is 0, the
 *       default interval from Kconfig is used. The callback is invoked
 *       from the monitoring thread context on status changes.
 */
OPERATE_RET tdl_printer_open(TDL_PRINTER_HANDLE handle, TDL_PRINTER_OPEN_PARAM_T *param)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_UNINIT) {
        return OPRT_OK;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.open, OPRT_INVALID_PARM);
    TUYA_CALL_ERR_RETURN(node->intfs.open(node->tdd_handle));

    node->status = TDL_PRINTER_STATUS_INITED;

    if (param != NULL && param->event_cb != NULL && node->intfs.get_status != NULL) {
        node->event_cb = param->event_cb;
        node->event_cb_arg = param->event_cb_arg;
        node->poll_interval_ms = (param->poll_interval_ms > 0)
                                     ? param->poll_interval_ms
                                     : CONFIG_PRINTER_POLL_INTERVAL_MS;

        OPERATE_RET rt = __start_monitor(node);
        if (rt != OPRT_OK) {
            PR_WARN("monitor start failed: %d, printing still available", rt);
        }
    }

    return OPRT_OK;
}

/**
 * @brief Start a print job
 * @param[in] handle printer handle
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED if the TDD driver does
 *         not implement start
 */
OPERATE_RET tdl_printer_start(TDL_PRINTER_HANDLE handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        PR_ERR("Printer not opened, status: %d, name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    if (NULL == node->intfs.start) {
        return OPRT_NOT_SUPPORTED;
    }

    return node->intfs.start(node->tdd_handle);
}

/**
 * @brief Send data to the printer
 * @param[in] handle printer handle
 * @param[in] data data buffer
 * @param[in] len data length
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_send(TDL_PRINTER_HANDLE handle, const uint8_t *data, uint32_t len)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(data, OPRT_INVALID_PARM);
    if (0 == len) {
        return OPRT_INVALID_PARM;
    }

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        PR_ERR("Printer handle is not init, current status: %d, printer name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    TUYA_CHECK_NULL_RETURN(node->intfs.write, OPRT_INVALID_PARM);

    return node->intfs.write(node->tdd_handle, data, len);
}

/**
 * @brief End a print job
 * @param[in] handle printer handle
 * @return OPRT_OK on success, OPRT_NOT_SUPPORTED if the TDD driver does
 *         not implement end
 */
OPERATE_RET tdl_printer_end(TDL_PRINTER_HANDLE handle)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        PR_ERR("Printer not opened, status: %d, name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    if (NULL == node->intfs.end) {
        return OPRT_NOT_SUPPORTED;
    }

    return node->intfs.end(node->tdd_handle);
}

/**
 * @brief Get printer device information
 * @param[in] handle printer handle
 * @param[out] info device information output
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_get_dev_info(TDL_PRINTER_HANDLE handle, TDL_PRINTER_DEV_INFO_T *info)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(info, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    info->dots_per_line      = node->dev_info.dots_per_line;
    info->bytes_per_line     = node->dev_info.bytes_per_line;

    return OPRT_OK;
}

/**
 * @brief Feed paper without printing
 * @param[in] handle printer handle
 * @param[in] lines number of dot lines to feed
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_paper_feed(TDL_PRINTER_HANDLE handle, uint32_t lines)
{
    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        PR_ERR("Printer not opened, status: %d, name: %s", node->status, node->name);
        return OPRT_COM_ERROR;
    }

    if (NULL == node->intfs.paper_feed) {
        return OPRT_NOT_SUPPORTED;
    }

    return node->intfs.paper_feed(node->tdd_handle, lines);
}

/**
 * @brief Close the printer and release resources
 * @param[in] handle printer handle
 * @return OPRT_OK on success
 * @note Stops the status monitoring thread if running.
 */
OPERATE_RET tdl_printer_close(TDL_PRINTER_HANDLE handle)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(handle, OPRT_INVALID_PARM);

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)handle;

    if (node->magic != TDL_PRINTER_MAGIC) {
        PR_ERR("Invalid printer handle magic: %d", node->magic);
        return OPRT_COM_ERROR;
    }

    if (node->status != TDL_PRINTER_STATUS_INITED) {
        return OPRT_OK;
    }

    __stop_monitor(node);

    if (node->intfs.close) {
        TUYA_CALL_ERR_RETURN(node->intfs.close(node->tdd_handle));
    }

    node->event_cb = NULL;
    node->event_cb_arg = NULL;
    node->status = TDL_PRINTER_STATUS_UNINIT;

    return OPRT_OK;
}

/**
 * @brief Register a printer driver (called by TDD layer)
 * @param[in] name printer name
 * @param[in] intfs driver interface function table
 * @param[in] tdd_hdl TDD driver private handle
 * @return OPRT_OK on success
 */
OPERATE_RET tdl_printer_driver_register(char *name, TDD_PRINTER_INTFS_T *intfs,
                                        TDD_PRINTER_DEV_INFO_T *dev_info,
                                        TDD_PRINTER_HANDLE_T tdd_hdl)
{
    OPERATE_RET rt = OPRT_OK;

    TUYA_CHECK_NULL_RETURN(name, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(intfs, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(dev_info, OPRT_INVALID_PARM);
    TUYA_CHECK_NULL_RETURN(tdd_hdl, OPRT_INVALID_PARM);

    if (NULL == g_printer_list.mutex) {
        TUYA_CALL_ERR_RETURN(tal_mutex_create_init(&g_printer_list.mutex));
    }

    TDL_PRINTER_NODE_T *node = (TDL_PRINTER_NODE_T *)tal_malloc(sizeof(TDL_PRINTER_NODE_T));
    TUYA_CHECK_NULL_RETURN(node, OPRT_MALLOC_FAILED);
    memset(node, 0, sizeof(TDL_PRINTER_NODE_T));

    INIT_LIST_HEAD(&node->node);

    node->magic = TDL_PRINTER_MAGIC;
    node->status = TDL_PRINTER_STATUS_UNINIT;
    node->tdd_handle = tdd_hdl;
    strncpy(node->name, name, sizeof(node->name) - 1);
    memcpy(&node->intfs, intfs, sizeof(TDD_PRINTER_INTFS_T));
    memcpy(&node->dev_info, dev_info, sizeof(TDD_PRINTER_DEV_INFO_T));

    tal_mutex_lock(g_printer_list.mutex);
    tuya_list_add_tail(&node->node, &g_printer_list.head);
    tal_mutex_unlock(g_printer_list.mutex);

    return rt;
}
