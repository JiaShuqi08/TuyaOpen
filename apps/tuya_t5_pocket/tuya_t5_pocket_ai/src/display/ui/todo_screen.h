/**
 * @file todo_screen.h
 * @brief Header file for the todo list screen
 *
 * This file contains the declarations for the todo list screen which provides
 * a comprehensive todo list management interface with keyboard shortcuts.
 *
 * The todo list includes:
 * - Add, edit, delete, and mark complete tasks
 * - Keyboard shortcut support with clear hints
 * - Customizable shortcuts configuration
 * - Persistent storage support
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#ifndef TODO_SCREEN_H
#define TODO_SCREEN_H

#include "screen_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Todo item structure
 */
typedef struct {
    char    text[32];    ///< Task description
    uint8_t is_complete; ///< 1 = completed, 0 = pending
} todo_item_t;

/**
 * @brief Todo screen structure
 */
typedef struct {
    Screen_t     base;           ///< Base screen structure
    todo_item_t *items;          ///< Array of todo items
    uint8_t      item_count;     ///< Current number of items
    uint8_t      max_items;      ///< Maximum number of items
    uint8_t      selected_index; ///< Currently selected item index
    uint8_t      in_edit_mode;   ///< 1 = edit mode, 0 = list mode
} TodoScreen_t;

/**
 * @brief The todo list screen instance
 */
extern Screen_t todo_screen;

/**
 * @brief Initialize the todo list screen
 */
void todo_screen_init(void);

/**
 * @brief Deinitialize the todo list screen
 */
void todo_screen_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* TODO_SCREEN_H */
