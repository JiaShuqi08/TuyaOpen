/**
 * @file menu_scan_screen.c
 * @brief Implementation of the scan menu screen for the application
 *
 * This file contains the implementation of the scan menu screen which provides
 * various scanning and game features including WiFi scan, I2C scan, and games.
 *
 * The scan menu includes:
 * - WiFi scanning functionality
 * - I2C device scanning
 * - Mini games (Dino, Snake)
 * - Level indicator
 * - Automatic transition timer
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "menu_scan_screen.h"
#include "screen_manager.h"
#include "wifi_scan_screen.h"
#include "i2c_scan_screen.h"
#include "dino_game_screen.h"
#include "snake_game_screen.h"
#include "level_indicator_screen.h"
#include "ebook_screen.h"
#include "temp_humidity_screen.h"
#include "ai_log_screen.h"
#include "photo_screen.h"
#include "toast_screen.h"
#include "todo_screen.h"
#include "tal_time_service.h"
#include <stdio.h>

// Font definitions - easily customizable
#define SCREEN_TITLE_FONT   &lv_font_terminusTTF_Bold_18
#define SCREEN_CONTENT_FONT &lv_font_terminusTTF_Bold_16
#define SCREEN_INFO_FONT    &lv_font_terminusTTF_Bold_14

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t   *ui_menu_scan_screen;
static lv_obj_t   *scan_menu_list;
static lv_timer_t *timer;
static uint8_t     selected_item      = 0;
static uint8_t     last_selected_item = 0;

// Clock editing variables
static uint8_t   clock_hour            = 12;
static uint8_t   clock_minute          = 0;
static uint16_t  clock_year            = 2024;
static uint8_t   clock_month           = 1;
static uint8_t   clock_day             = 1;
static bool      clock_edit_mode       = false;
static uint8_t   clock_edit_field      = 0; // 0=hour, 1=minute, 2=year, 3=month, 4=day
static lv_obj_t *clock_edit_popup      = NULL;
static lv_obj_t *clock_edit_title      = NULL;
static lv_obj_t *clock_edit_time_label = NULL;
static lv_obj_t *clock_edit_date_label = NULL;
static lv_obj_t *clock_edit_hint       = NULL;

Screen_t menu_scan_screen = {
    .init       = menu_scan_screen_init,
    .deinit     = menu_scan_screen_deinit,
    .screen_obj = &ui_menu_scan_screen,
    .name       = "menu_scan_screen",
    .state_data = NULL,
};

/***********************************************************
********************function declaration********************
***********************************************************/

// External screen declarations
// extern Screen_t wifi_scan_screen;
// extern Screen_t i2c_scan_screen;
// extern Screen_t dino_game_screen;
// extern Screen_t snake_game_screen;
// extern Screen_t level_indicator_screen;
// extern Screen_t temp_humidity_screen;
// extern Screen_t photo_screen;

static void menu_scan_screen_timer_cb(lv_timer_t *timer);
static void keyboard_event_cb(lv_event_t *e);
static void update_selection(uint8_t old_selection, uint8_t new_selection);
static void handle_scan_selection(void);
static void show_clock_edit_popup(void);
static void hide_clock_edit_popup(void);
static void update_clock_display(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Timer callback for the scan menu screen
 *
 * This function is called when the scan menu screen timer expires.
 * It can be used for periodic updates.
 *
 * @param timer The timer object
 */
static void menu_scan_screen_timer_cb(lv_timer_t *timer)
{
    printf("[%s] scan menu timer callback\n", menu_scan_screen.name);
    // Add any periodic update logic here
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the scan menu screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", menu_scan_screen.name, key);

    // Handle clock edit mode first if active
    if (clock_edit_mode) {
        switch (key) {
        case KEY_UP:
            switch (clock_edit_field) {
            case 0: // Hour
                clock_hour = (clock_hour + 1) % 24;
                break;
            case 1: // Minute
                clock_minute = (clock_minute + 1) % 60;
                break;
            case 2: // Year
                clock_year = (clock_year >= 2100) ? 2000 : (clock_year + 1);
                break;
            case 3: // Month
                clock_month = (clock_month >= 12) ? 1 : (clock_month + 1);
                break;
            case 4: // Day
            {
                // Get max days for current month/year
                uint8_t max_days = 31;
                if (clock_month == 2) {
                    // Check leap year
                    if ((clock_year % 4 == 0 && clock_year % 100 != 0) || (clock_year % 400 == 0)) {
                        max_days = 29;
                    } else {
                        max_days = 28;
                    }
                } else if (clock_month == 4 || clock_month == 6 || clock_month == 9 || clock_month == 11) {
                    max_days = 30;
                }
                clock_day = (clock_day >= max_days) ? 1 : (clock_day + 1);
            }
            break;
            }
            update_clock_display();
            break;
        case KEY_DOWN:
            switch (clock_edit_field) {
            case 0: // Hour
                clock_hour = (clock_hour == 0) ? 23 : (clock_hour - 1);
                break;
            case 1: // Minute
                clock_minute = (clock_minute == 0) ? 59 : (clock_minute - 1);
                break;
            case 2: // Year
                clock_year = (clock_year <= 2000) ? 2100 : (clock_year - 1);
                break;
            case 3: // Month
                clock_month = (clock_month == 1) ? 12 : (clock_month - 1);
                break;
            case 4: // Day
            {
                uint8_t max_days_d = 31;
                if (clock_month == 2) {
                    if ((clock_year % 4 == 0 && clock_year % 100 != 0) || (clock_year % 400 == 0)) {
                        max_days_d = 29;
                    } else {
                        max_days_d = 28;
                    }
                } else if (clock_month == 4 || clock_month == 6 || clock_month == 9 || clock_month == 11) {
                    max_days_d = 30;
                }
                clock_day = (clock_day == 1) ? max_days_d : (clock_day - 1);
            }
            break;
            }
            update_clock_display();
            break;
        case KEY_LEFT:
            clock_edit_field = (clock_edit_field == 0) ? 4 : (clock_edit_field - 1);
            update_clock_display();
            break;
        case KEY_RIGHT:
            clock_edit_field = (clock_edit_field == 4) ? 0 : (clock_edit_field + 1);
            update_clock_display();
            break;
        case KEY_ENTER:
        case KEY_ESC: {
            // Save date and time
            POSIX_TM_S tm     = {0};
            tm.tm_hour        = clock_hour;
            tm.tm_min         = clock_minute;
            tm.tm_sec         = 0;
            tm.tm_mday        = clock_day;
            tm.tm_mon         = clock_month - 1;
            tm.tm_year        = clock_year - 1900;
            TIME_T posix_time = tal_time_mktime(&tm);
            if (posix_time > 0) {
                tal_time_set_posix(posix_time, 2); // 2 = other source
            }
            printf("[Clock Edit] Date & Time saved: %04d-%02d-%02d %02d:%02d\n", 
                   clock_year, clock_month, clock_day, clock_hour, clock_minute);
            toast_screen_show("Date & Time saved", 1500);
            hide_clock_edit_popup();
            break;
        }
        default:
            break;
        }
        return;
    }

    // Normal menu mode
    uint32_t child_count = lv_obj_get_child_cnt(scan_menu_list);
    if (child_count == 0)
        return;

    uint8_t old_selection = selected_item;
    uint8_t new_selection = old_selection;

    switch (key) {
    case KEY_UP:
        if (selected_item > 0) {
            new_selection = selected_item - 1;
        }
        break;
    case KEY_DOWN:
        if (selected_item < child_count - 1) {
            new_selection = selected_item + 1;
        }
        break;
    case KEY_LEFT:
        printf("LEFT key pressed\n");
        break;
    case KEY_RIGHT:
        printf("RIGHT key pressed\n");
        break;
    case KEY_ENTER:
        handle_scan_selection();
        break;
    case KEY_ESC:
        printf("ESC key pressed - returning to main menu\n");
        last_selected_item = 0;
        screen_back();
        break;
    default:
        printf("Key %d pressed\n", key);
        break;
    }

    if (new_selection != old_selection) {
        update_selection(old_selection, new_selection);
        selected_item = new_selection;
    }
}

/**
 * @brief Update visual selection highlighting
 */
static void update_selection(uint8_t old_selection, uint8_t new_selection)
{
    uint32_t child_count = lv_obj_get_child_cnt(scan_menu_list);

    if (old_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(scan_menu_list, old_selection), lv_color_white(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(scan_menu_list, old_selection), lv_color_black(), 0);
    }

    if (new_selection < child_count) {
        lv_obj_set_style_bg_color(lv_obj_get_child(scan_menu_list, new_selection), lv_color_black(), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(scan_menu_list, new_selection), lv_color_white(), 0);
        lv_obj_scroll_to_view(lv_obj_get_child(scan_menu_list, new_selection), LV_ANIM_ON);
    }
}

/**
 * @brief Handle scan menu selection based on current selected item
 */
static void handle_scan_selection(void)
{
    last_selected_item = selected_item;

    switch (selected_item) {
    case 0: // WiFi scan demo
        printf("WiFi scan demo selected\n");
        screen_load(&wifi_scan_screen);
        break;
    case 1: // I2C device scan demo
        printf("I2C device scan demo selected\n");
        screen_load(&i2c_scan_screen);
        break;
    case 2: // Dino Game
        printf("Dino Game selected\n");
        screen_load(&dino_game_screen);
        break;
    case 3: // Snake Game
        printf("Snake Game selected\n");
        screen_load(&snake_game_screen);
        break;
    case 4: // Level Indicator
        printf("Level Indicator selected\n");
        screen_load(&level_indicator_screen);
        break;
    case 5: // E-book Reader
        printf("E-book Reader action selected\n");
        screen_load(&ebook_screen);
        break;
    case 6: // Temperature & Humidity
        printf("Temperature & Humidity selected\n");
        screen_load(&temp_humidity_screen);
        break;
    case 7: // Camera
        printf("Camera selected\n");
        screen_load(&ai_log_screen);
        break;
    case 8: // PHOTO
        printf("PHOTO selected\n");
        screen_load(&photo_screen);
        break;
    case 9: // To-Do List
        printf("To-Do List selected\n");
        screen_load(&todo_screen);
        break;
    case 10: // Clock Settings
        printf("Clock Settings selected\n");
        show_clock_edit_popup();
        break;
    default:
        printf("Unknown scan option selected\n");
        break;
    }
}

/**
 * @brief Initialize the scan menu screen
 *
 * This function creates the scan menu UI with scanning options and games.
 */
void menu_scan_screen_init(void)
{
    ui_menu_scan_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_menu_scan_screen, 384, 168);
    lv_obj_set_style_bg_color(ui_menu_scan_screen, lv_color_white(), 0);

    // Title at the top
    lv_obj_t *title = lv_label_create(ui_menu_scan_screen);
    lv_label_set_text(title, "Device Scan & Games");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(title, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    // List for scan menu items
    scan_menu_list = lv_list_create(ui_menu_scan_screen);
    lv_obj_set_size(scan_menu_list, 364, 128);
    lv_obj_align(scan_menu_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(scan_menu_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scan_menu_list, LV_DIR_VER);

    lv_obj_set_style_border_color(scan_menu_list, lv_color_black(), 0);
    lv_obj_set_style_border_width(scan_menu_list, 2, 0);

    // Add scan menu items
    lv_obj_t *btn;
    lv_obj_t *label;

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_WIFI, "WiFi scan demo");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_SETTINGS, "I2C device scan demo");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_PLAY, "Dino Game");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_SHUFFLE, "Snake Game");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_EYE_OPEN, "Level Indicator");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_FILE, "E-book Reader");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_WARNING, "Temperature & Humidity");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_CALL, "AI Log Analyzer");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_IMAGE, "PHOTO");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_EDIT, "To-Do List");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    btn   = lv_list_add_btn(scan_menu_list, LV_SYMBOL_EDIT, "Clock Settings");
    label = lv_obj_get_child(btn, 1);
    if (label)
        lv_obj_set_style_text_font(label, SCREEN_CONTENT_FONT, 0);

    selected_item        = last_selected_item;
    uint32_t child_count = lv_obj_get_child_cnt(scan_menu_list);

    if (selected_item >= child_count) {
        selected_item      = 0;
        last_selected_item = 0;
    }

    if (child_count > 0) {
        update_selection(0, selected_item);
        printf("[%s] Restored selection to item %d\n", menu_scan_screen.name, selected_item);
    }

    timer = lv_timer_create(menu_scan_screen_timer_cb, 1000, NULL);
    lv_obj_add_event_cb(ui_menu_scan_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_menu_scan_screen);
    lv_group_focus_obj(ui_menu_scan_screen);
}

/**
 * @brief Deinitialize the scan menu screen
 *
 * This function cleans up the scan menu by deleting the UI object
 * and timer, and removing the event callback.
 */
void menu_scan_screen_deinit(void)
{
    // Clean up clock edit popup if active
    if (clock_edit_popup) {
        lv_obj_del(clock_edit_popup);
        clock_edit_popup = NULL;
    }
    clock_edit_title      = NULL;
    clock_edit_time_label = NULL;
    clock_edit_hint       = NULL;
    clock_edit_mode       = false;

    if (ui_menu_scan_screen) {
        printf("deinit scan menu screen\n");
        lv_obj_remove_event_cb(ui_menu_scan_screen, NULL);
        lv_group_remove_obj(ui_menu_scan_screen);
    }
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
}

/**
 * @brief Create the scan menu screen (alias for init)
 */
void menu_scan_screen_create(void)
{
    menu_scan_screen_init();
}

/**
 * @brief Update clock display
 */
static void update_clock_display(void)
{
    if (clock_edit_time_label) {
        char time_text[32];
        if (clock_edit_field == 0) {
            snprintf(time_text, sizeof(time_text), "[%02d]:%02d", clock_hour, clock_minute);
        } else if (clock_edit_field == 1) {
            snprintf(time_text, sizeof(time_text), "%02d:[%02d]", clock_hour, clock_minute);
        } else {
            snprintf(time_text, sizeof(time_text), "%02d:%02d", clock_hour, clock_minute);
        }
        lv_label_set_text(clock_edit_time_label, time_text);
    }
    
    if (clock_edit_date_label) {
        char date_text[32];
        if (clock_edit_field == 2) {
            snprintf(date_text, sizeof(date_text), "[%04d]/%02d/%02d", clock_year, clock_month, clock_day);
        } else if (clock_edit_field == 3) {
            snprintf(date_text, sizeof(date_text), "%04d/[%02d]/%02d", clock_year, clock_month, clock_day);
        } else if (clock_edit_field == 4) {
            snprintf(date_text, sizeof(date_text), "%04d/%02d/[%02d]", clock_year, clock_month, clock_day);
        } else {
            snprintf(date_text, sizeof(date_text), "%04d/%02d/%02d", clock_year, clock_month, clock_day);
        }
        lv_label_set_text(clock_edit_date_label, date_text);
    }
}

/**
 * @brief Show clock edit popup
 */
static void show_clock_edit_popup(void)
{
    // Get current time and date from system
    TIME_T current_posix = tal_time_get_posix();
    if (current_posix > 0) {
        POSIX_TM_S local_time;
        if (tal_time_get_local_time_custom(current_posix, &local_time) == OPRT_OK) {
            clock_hour   = (uint8_t)local_time.tm_hour;
            clock_minute = (uint8_t)local_time.tm_min;
            clock_year   = (uint16_t)(local_time.tm_year + 1900);
            clock_month  = (uint8_t)(local_time.tm_mon + 1);
            clock_day    = (uint8_t)local_time.tm_mday;
        }
    }

    clock_edit_mode  = true;
    clock_edit_field = 0; // Start with hour

    // Hide the scan menu list
    lv_obj_add_flag(scan_menu_list, LV_OBJ_FLAG_HIDDEN);

    // Create clock edit popup
    clock_edit_popup = lv_obj_create(ui_menu_scan_screen);
    lv_obj_set_size(clock_edit_popup, 364, 180);
    lv_obj_align(clock_edit_popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(clock_edit_popup, lv_color_white(), 0);
    lv_obj_set_style_border_color(clock_edit_popup, lv_color_black(), 0);
    lv_obj_set_style_border_width(clock_edit_popup, 2, 0);

    // Title
    clock_edit_title = lv_label_create(clock_edit_popup);
    lv_label_set_text(clock_edit_title, "Date & Time Settings");
    lv_obj_align(clock_edit_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(clock_edit_title, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(clock_edit_title, lv_color_black(), 0);

    // Date display
    clock_edit_date_label = lv_label_create(clock_edit_popup);
    lv_obj_align(clock_edit_date_label, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_text_font(clock_edit_date_label, SCREEN_CONTENT_FONT, 0);
    lv_obj_set_style_text_color(clock_edit_date_label, lv_color_black(), 0);

    // Clock display
    clock_edit_time_label = lv_label_create(clock_edit_popup);
    lv_obj_align(clock_edit_time_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(clock_edit_time_label, SCREEN_TITLE_FONT, 0);
    lv_obj_set_style_text_color(clock_edit_time_label, lv_color_black(), 0);

    update_clock_display();

    // Instructions
    clock_edit_hint = lv_label_create(clock_edit_popup);
    lv_label_set_text(clock_edit_hint, "UP/DOWN: Adjust  LEFT/RIGHT: Switch  ENTER/ESC: Save");
    lv_obj_align(clock_edit_hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(clock_edit_hint, SCREEN_INFO_FONT, 0);
    lv_obj_set_style_text_color(clock_edit_hint, lv_color_black(), 0);
}

/**
 * @brief Hide clock edit popup
 */
static void hide_clock_edit_popup(void)
{
    clock_edit_mode = false;

    // Clean up popup objects
    if (clock_edit_popup) {
        lv_obj_del(clock_edit_popup);
        clock_edit_popup = NULL;
    }
    clock_edit_title      = NULL;
    clock_edit_time_label = NULL;
    clock_edit_date_label = NULL;
    clock_edit_hint       = NULL;

    // Show the scan menu list again
    lv_obj_clear_flag(scan_menu_list, LV_OBJ_FLAG_HIDDEN);
}
