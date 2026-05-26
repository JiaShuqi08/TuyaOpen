/**
 * @file todo_screen.c
 * @brief Implementation of the todo list screen
 *
 * This file contains the implementation of the todo list screen which provides
 * comprehensive todo list management using virtual keyboard for text input.
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "todo_screen.h"
#include "keyboard_screen.h"
#include "toast_screen.h"
#include <stdio.h>
#include <string.h>

/***********************************************************
************************macro define************************
***********************************************************/

// Font definitions
#define SCREEN_TITLE_FONT   &lv_font_terminusTTF_Bold_18
#define SCREEN_CONTENT_FONT &lv_font_terminusTTF_Bold_16
#define SCREEN_INFO_FONT    &lv_font_terminusTTF_Bold_14

// Screen dimensions
#define SCREEN_WIDTH  384
#define SCREEN_HEIGHT 168

// Todo list configuration
#define MAX_TODO_ITEMS       10
#define MAX_TODO_TEXT_LENGTH 20
#define TODO_LIST_HEIGHT     90
#define TODO_ITEM_HEIGHT     20

// Operating modes
#define MODE_LIST   0 // Main list view
#define MODE_ACTION 1 // Action menu (Add/Edit/Delete)

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_todo_screen;

// UI components
static lv_obj_t *title_label         = NULL;
static lv_obj_t *todo_list_container = NULL;
static lv_obj_t *todo_items[MAX_TODO_ITEMS];
static lv_obj_t *status_bar  = NULL;
static lv_obj_t *action_menu = NULL;

// Todo list state
static todo_item_t todo_items_data[MAX_TODO_ITEMS];
static uint8_t     todo_item_count = 0;
static uint8_t     selected_index  = 0;
static uint8_t     current_mode    = MODE_LIST;
static uint8_t     action_selected = 0;

// Edit mode tracking
static uint8_t editing_index = 0;
static uint8_t is_adding     = 0;

Screen_t todo_screen = {
    .init       = todo_screen_init,
    .deinit     = todo_screen_deinit,
    .screen_obj = &ui_todo_screen,
    .name       = "todo",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_ui_components(void);
static void update_todo_list_display(void);
static void update_selection_highlight(void);
static void update_status_bar(void);
static void show_action_menu(void);
static void hide_action_menu(void);
static void handle_action_selection(uint8_t action);
static void delete_selected_todo(void);
static void toggle_complete_status(void);

// Keyboard callback
static void keyboard_callback(const char *text, void *user_data);
static void add_new_todo(void);
static void edit_selected_todo(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Create UI components
 */
static void create_ui_components(void)
{
    // Title label
    title_label = lv_label_create(ui_todo_screen);
    lv_label_set_text(title_label, "To-Do List");
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 8);
    lv_obj_set_style_text_font(title_label, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);

    // Status bar
    status_bar = lv_label_create(ui_todo_screen);
    lv_obj_align(status_bar, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_set_style_text_font(status_bar, SCREEN_INFO_FONT, 0);
    lv_obj_set_style_text_color(status_bar, lv_color_black(), 0);

    // Todo list container
    todo_list_container = lv_obj_create(ui_todo_screen);
    lv_obj_set_size(todo_list_container, SCREEN_WIDTH - 20, TODO_LIST_HEIGHT);
    lv_obj_align(todo_list_container, LV_ALIGN_TOP_LEFT, 10, 30);
    lv_obj_set_style_bg_color(todo_list_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(todo_list_container, 1, 0);
    lv_obj_set_style_border_color(todo_list_container, lv_color_black(), 0);
    lv_obj_set_style_pad_all(todo_list_container, 2, 0);
    lv_obj_set_style_radius(todo_list_container, 3, 0);

    // Create todo item placeholders
    for (uint8_t i = 0; i < MAX_TODO_ITEMS; i++) {
        todo_items[i] = NULL;
    }

    // Action menu
    action_menu = lv_obj_create(ui_todo_screen);
    lv_obj_set_size(action_menu, SCREEN_WIDTH - 20, 60);
    lv_obj_align(action_menu, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    lv_obj_set_style_bg_color(action_menu, lv_color_black(), 0);
    lv_obj_set_style_border_width(action_menu, 1, 0);
    lv_obj_set_style_border_color(action_menu, lv_color_black(), 0);
    lv_obj_set_style_radius(action_menu, 3, 0);
    lv_obj_add_flag(action_menu, LV_OBJ_FLAG_HIDDEN);

    // Action menu items
    const char *actions[] = {"Add", "Edit", "Delete"};
    for (uint8_t i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_obj_create(action_menu);
        lv_obj_set_size(btn, (SCREEN_WIDTH - 28) / 3, 52);
        lv_obj_align(btn, LV_ALIGN_LEFT_MID, 4 + i * ((SCREEN_WIDTH - 28) / 3), 0);
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_white(), 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, actions[i]);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
    }

    update_status_bar();
}

/**
 * @brief Update status bar display
 */
static void update_status_bar(void)
{
    if (status_bar) {
        char status_text[32];
        snprintf(status_text, sizeof(status_text), "%d/%d", todo_item_count, MAX_TODO_ITEMS);
        lv_label_set_text(status_bar, status_text);
    }
}

/**
 * @brief Update todo list display
 */
static void update_todo_list_display(void)
{
    // Clear existing items
    for (uint8_t i = 0; i < MAX_TODO_ITEMS; i++) {
        if (todo_items[i]) {
            lv_obj_del(todo_items[i]);
            todo_items[i] = NULL;
        }
    }

    // Create new items
    for (uint8_t i = 0; i < todo_item_count; i++) {
        todo_items[i] = lv_obj_create(todo_list_container);
        lv_obj_set_size(todo_items[i], SCREEN_WIDTH - 28, TODO_ITEM_HEIGHT - 2);
        lv_obj_align(todo_items[i], LV_ALIGN_TOP_LEFT, 2, 2 + i * TODO_ITEM_HEIGHT);
        lv_obj_set_style_bg_color(todo_items[i], lv_color_white(), 0);
        lv_obj_set_style_border_width(todo_items[i], 0, 0);
        lv_obj_set_style_pad_all(todo_items[i], 0, 0);

        // Checkbox
        lv_obj_t *check_label = lv_label_create(todo_items[i]);
        lv_obj_align(check_label, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_set_style_text_font(check_label, SCREEN_CONTENT_FONT, 0);
        lv_obj_set_style_text_color(check_label, lv_color_black(), 0);
        lv_label_set_text(check_label, todo_items_data[i].is_complete ? "[X]" : "[ ]");

        // Task text
        lv_obj_t *task_label = lv_label_create(todo_items[i]);
        lv_obj_align(task_label, LV_ALIGN_LEFT_MID, 25, 0);
        lv_obj_set_style_text_font(task_label, SCREEN_CONTENT_FONT, 0);
        lv_obj_set_style_text_color(task_label, lv_color_black(), 0);
        lv_label_set_text(task_label, todo_items_data[i].text);
    }

    update_selection_highlight();
    update_status_bar();
}

/**
 * @brief Update selection highlight
 */
static void update_selection_highlight(void)
{
    // Reset all items
    for (uint8_t i = 0; i < todo_item_count; i++) {
        if (todo_items[i]) {
            lv_obj_set_style_bg_color(todo_items[i], lv_color_white(), 0);
            lv_obj_set_style_border_width(todo_items[i], 0, 0);

            lv_obj_t *check_label = lv_obj_get_child(todo_items[i], 0);
            if (check_label)
                lv_obj_set_style_text_color(check_label, lv_color_black(), 0);
            lv_obj_t *task_label = lv_obj_get_child(todo_items[i], 1);
            if (task_label)
                lv_obj_set_style_text_color(task_label, lv_color_black(), 0);
        }
    }

    // Highlight selected item
    if (selected_index < todo_item_count && todo_items[selected_index]) {
        lv_obj_set_style_bg_color(todo_items[selected_index], lv_color_black(), 0);

        lv_obj_t *check_label = lv_obj_get_child(todo_items[selected_index], 0);
        if (check_label)
            lv_obj_set_style_text_color(check_label, lv_color_white(), 0);
        lv_obj_t *task_label = lv_obj_get_child(todo_items[selected_index], 1);
        if (task_label)
            lv_obj_set_style_text_color(task_label, lv_color_white(), 0);
    }
}

/**
 * @brief Show action menu
 */
static void show_action_menu(void)
{
    lv_obj_clear_flag(action_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(todo_list_container, LV_OBJ_FLAG_HIDDEN);
    action_selected = 0;
    lv_label_set_text(title_label, "Select Action");

    // Highlight first action
    lv_obj_t *first_action = lv_obj_get_child(action_menu, 0);
    if (first_action) {
        lv_obj_set_style_bg_color(first_action, lv_color_white(), 0);
        lv_obj_t *label = lv_obj_get_child(first_action, 0);
        if (label)
            lv_obj_set_style_text_color(label, lv_color_black(), 0);
    }
}

/**
 * @brief Hide action menu
 */
static void hide_action_menu(void)
{
    lv_obj_add_flag(action_menu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(todo_list_container, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(title_label, "To-Do List");

    // Reset action menu styles
    for (uint8_t i = 0; i < 3; i++) {
        lv_obj_t *action = lv_obj_get_child(action_menu, i);
        if (action) {
            lv_obj_set_style_bg_color(action, lv_color_black(), 0);
            lv_obj_t *label = lv_obj_get_child(action, 0);
            if (label)
                lv_obj_set_style_text_color(label, lv_color_white(), 0);
        }
    }
}

/**
 * @brief Handle action selection
 */
static void handle_action_selection(uint8_t action)
{
    switch (action) {
    case 0: // Add
        add_new_todo();
        break;
    case 1: // Edit
        edit_selected_todo();
        break;
    case 2: // Delete
        delete_selected_todo();
        hide_action_menu();
        break;
    }
}

/**
 * @brief Add new todo item using virtual keyboard
 */
static void add_new_todo(void)
{
    if (todo_item_count >= MAX_TODO_ITEMS) {
        toast_screen_show("List full!", 1500);
        hide_action_menu();
        return;
    }

    is_adding     = 1;
    editing_index = todo_item_count;

    // Show virtual keyboard with empty initial text
    keyboard_screen_show_with_callback("", keyboard_callback, NULL);
}

/**
 * @brief Edit selected todo item using virtual keyboard
 */
static void edit_selected_todo(void)
{
    if (selected_index >= todo_item_count) {
        hide_action_menu();
        return;
    }

    is_adding     = 0;
    editing_index = selected_index;

    // Show virtual keyboard with existing text
    keyboard_screen_show_with_callback(todo_items_data[selected_index].text, keyboard_callback, NULL);
}

/**
 * @brief Keyboard callback for text input completion
 */
static void keyboard_callback(const char *text, void *user_data)
{
    (void)user_data;

    if (text && strlen(text) > 0) {
        if (is_adding) {
            // Add new task
            strncpy(todo_items_data[editing_index].text, text, MAX_TODO_TEXT_LENGTH - 1);
            todo_items_data[editing_index].text[MAX_TODO_TEXT_LENGTH - 1] = '\0';
            todo_items_data[editing_index].is_complete                    = 0;
            todo_item_count++;
            selected_index = editing_index;
            toast_screen_show("Added!", 1500);
        } else {
            // Edit existing task
            strncpy(todo_items_data[editing_index].text, text, MAX_TODO_TEXT_LENGTH - 1);
            todo_items_data[editing_index].text[MAX_TODO_TEXT_LENGTH - 1] = '\0';
            toast_screen_show("Updated!", 1500);
        }

        // Refresh the list display after returning from keyboard
        // This will be called after screen_back completes
        update_todo_list_display();
    } else {
        printf("Keyboard input cancelled or empty\n");
    }
}

/**
 * @brief Delete selected todo item
 */
static void delete_selected_todo(void)
{
    if (selected_index >= todo_item_count)
        return;

    for (uint8_t i = selected_index; i < todo_item_count - 1; i++) {
        memcpy(&todo_items_data[i], &todo_items_data[i + 1], sizeof(todo_item_t));
    }

    todo_item_count--;
    if (selected_index >= todo_item_count && todo_item_count > 0) {
        selected_index = todo_item_count - 1;
    }

    update_todo_list_display();
    toast_screen_show("Deleted!", 1500);
}

/**
 * @brief Toggle completion status
 */
static void toggle_complete_status(void)
{
    if (selected_index >= todo_item_count)
        return;

    todo_items_data[selected_index].is_complete = !todo_items_data[selected_index].is_complete;
    update_todo_list_display();
    toast_screen_show(todo_items_data[selected_index].is_complete ? "Completed!" : "Pending!", 1500);
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);

    switch (current_mode) {
    case MODE_LIST:
        // Main list mode
        switch (key) {
        case KEY_UP:
            if (selected_index > 0) {
                selected_index--;
                update_selection_highlight();
            }
            break;

        case KEY_DOWN:
            if (selected_index < todo_item_count - 1) {
                selected_index++;
                update_selection_highlight();
            }
            break;

        case KEY_LEFT:
            // Go back to previous screen
            screen_back();
            break;

        case KEY_RIGHT:
            // Show action menu
            show_action_menu();
            current_mode = MODE_ACTION;
            break;

        case KEY_ENTER:
            // Toggle completion
            toggle_complete_status();
            break;

        case KEY_ESC:
            screen_back();
            break;
        }
        break;

    case MODE_ACTION:
        // Action menu mode
        switch (key) {
        case KEY_UP:
        case KEY_LEFT:
            if (action_selected > 0) {
                // Reset previous
                lv_obj_t *prev = lv_obj_get_child(action_menu, action_selected);
                if (prev) {
                    lv_obj_set_style_bg_color(prev, lv_color_black(), 0);
                    lv_obj_t *label = lv_obj_get_child(prev, 0);
                    if (label)
                        lv_obj_set_style_text_color(label, lv_color_white(), 0);
                }
                action_selected--;
                // Highlight new
                lv_obj_t *curr = lv_obj_get_child(action_menu, action_selected);
                if (curr) {
                    lv_obj_set_style_bg_color(curr, lv_color_white(), 0);
                    lv_obj_t *label = lv_obj_get_child(curr, 0);
                    if (label)
                        lv_obj_set_style_text_color(label, lv_color_black(), 0);
                }
            }
            break;

        case KEY_DOWN:
        case KEY_RIGHT:
            if (action_selected < 2) {
                // Reset previous
                lv_obj_t *prev = lv_obj_get_child(action_menu, action_selected);
                if (prev) {
                    lv_obj_set_style_bg_color(prev, lv_color_black(), 0);
                    lv_obj_t *label = lv_obj_get_child(prev, 0);
                    if (label)
                        lv_obj_set_style_text_color(label, lv_color_white(), 0);
                }
                action_selected++;
                // Highlight new
                lv_obj_t *curr = lv_obj_get_child(action_menu, action_selected);
                if (curr) {
                    lv_obj_set_style_bg_color(curr, lv_color_white(), 0);
                    lv_obj_t *label = lv_obj_get_child(curr, 0);
                    if (label)
                        lv_obj_set_style_text_color(label, lv_color_black(), 0);
                }
            }
            break;

        case KEY_ENTER:
            handle_action_selection(action_selected);
            break;

        case KEY_ESC:
            hide_action_menu();
            current_mode = MODE_LIST;
            break;
        }
        break;
    }
}

/**
 * @brief Initialize the todo list screen
 */
void todo_screen_init(void)
{
    ui_todo_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_todo_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_todo_screen, lv_color_white(), 0);

    current_mode = MODE_LIST;

    // Add sample todos
    if (todo_item_count == 0) {
        strncpy(todo_items_data[0].text, "Buy groceries", MAX_TODO_TEXT_LENGTH - 1);
        todo_items_data[0].is_complete = 0;
        strncpy(todo_items_data[1].text, "Walk the dog", MAX_TODO_TEXT_LENGTH - 1);
        todo_items_data[1].is_complete = 1;
        strncpy(todo_items_data[2].text, "Read a book", MAX_TODO_TEXT_LENGTH - 1);
        todo_items_data[2].is_complete = 0;
        todo_item_count                = 3;
        selected_index                 = 0;
    }

    create_ui_components();
    update_todo_list_display();

    lv_obj_add_event_cb(ui_todo_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_todo_screen);
    lv_group_focus_obj(ui_todo_screen);
}

/**
 * @brief Deinitialize the todo list screen
 */
void todo_screen_deinit(void)
{
    if (ui_todo_screen) {
        lv_obj_remove_event_cb(ui_todo_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_todo_screen);
    }

    title_label         = NULL;
    todo_list_container = NULL;
    status_bar          = NULL;
    action_menu         = NULL;
    for (uint8_t i = 0; i < MAX_TODO_ITEMS; i++) {
        todo_items[i] = NULL;
    }
}
