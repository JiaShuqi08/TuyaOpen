/**
 * @file standby_screen.c
 * @brief Implementation of the standby screen for the application
 *
 * This file contains the implementation of the standby screen which displays
 * the current time with a clean, centered design in black and white.
 *
 * The standby screen includes:
 * - A monochrome white background
 * - Current time display (HH:MM format)
 * - Current date display
 * - Real-time clock updates
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "standby_screen.h"
#include <stdio.h>
#include <string.h>
#include "tal_time_service.h"

/***********************************************************
************************macro define************************
***********************************************************/

#define UPDATE_INTERVAL 1000 // Update clock every second

// Font definitions - using available fonts in the project
#define CLOCK_FONT &lv_font_terminusTTF_Bold_18
#define DATE_FONT &lv_font_terminusTTF_Bold_14

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_standby_screen;
static lv_obj_t *time_label;
static lv_obj_t *date_label;
static lv_timer_t *clock_timer;

Screen_t standby_screen = {
    .init = standby_screen_init,
    .deinit = standby_screen_deinit,
    .screen_obj = &ui_standby_screen,
    .name = "standby",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void update_clock(lv_timer_t *timer);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Update the clock display
 *
 * This function is called by the timer to update the time and date display.
 *
 * @param timer The timer object
 */
static void update_clock(lv_timer_t *timer)
{
    (void)timer; // Unused parameter

    // Get current time from system
    TIME_T current_posix = tal_time_get_posix();
    if (current_posix <= 0) {
        // Fallback if system time is not available
        lv_label_set_text(time_label, "--:--");
        lv_label_set_text(date_label, "----/--/--");
        return;
    }

    // Get local time
    POSIX_TM_S local_time;
    if (tal_time_get_local_time_custom(current_posix, &local_time) != OPRT_OK) {
        lv_label_set_text(time_label, "--:--");
        lv_label_set_text(date_label, "----/--/--");
        return;
    }

    // Format time string (HH:MM)
    static char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d",
             local_time.tm_hour, local_time.tm_min);
    lv_label_set_text(time_label, time_str);

    // Format date string (YYYY/MM/DD)
    static char date_str[32];
    snprintf(date_str, sizeof(date_str), "%04d/%02d/%02d",
             local_time.tm_year + 1900, local_time.tm_mon + 1, local_time.tm_mday);
    lv_label_set_text(date_label, date_str);
}

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the standby screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    uint32_t key = lv_event_get_key(e);
    printf("[%s] Keyboard event received: key = %d\n", standby_screen.name, key);
    screen_back();

    switch (key) {
    case KEY_UP:
        printf("UP key pressed\n");
        break;
    case KEY_DOWN:
        printf("DOWN key pressed\n");
        break;
    case KEY_LEFT:
        printf("LEFT key pressed\n");
        break;
    case KEY_RIGHT:
        printf("RIGHT key pressed\n");
        break;
    case KEY_ENTER:
        printf("ENTER key pressed\n");
        break;
    case KEY_ESC:
        printf("ESC key pressed - going back\n");
        break;
    default:
        printf("Unknown key pressed\n");
        break;
    }
}

/**
 * @brief Initialize the standby screen
 *
 * This function creates the standby screen UI with a white background,
 * creates time and date labels, and starts the clock update timer.
 */
void standby_screen_init(void)
{
    printf("[%s] Initializing standby screen\n", standby_screen.name);

    // Create the main screen
    ui_standby_screen = lv_obj_create(NULL);
    lv_obj_set_size(ui_standby_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);

    // Set white background for monochrome display
    lv_obj_set_style_bg_color(ui_standby_screen, lv_color_white(), 0);

    // Create time label (HH:MM)
    time_label = lv_label_create(ui_standby_screen);
    lv_label_set_text(time_label, "--:--");
    lv_obj_set_style_text_font(time_label, CLOCK_FONT, 0);
    lv_obj_set_style_text_color(time_label, lv_color_black(), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

    // Create date label (YYYY/MM/DD)
    date_label = lv_label_create(ui_standby_screen);
    lv_label_set_text(date_label, "----/--/--");
    lv_obj_set_style_text_font(date_label, DATE_FONT, 0);
    lv_obj_set_style_text_color(date_label, lv_color_black(), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 30);

    // Create timer to update clock every second (repeat indefinitely with -1)
    clock_timer = lv_timer_create(update_clock, UPDATE_INTERVAL, NULL);
    lv_timer_set_repeat_count(clock_timer, -1); // -1 means infinite repeat

    // Initial update
    update_clock(clock_timer);

    // Add keyboard event handling
    lv_obj_add_event_cb(ui_standby_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(lv_group_get_default(), ui_standby_screen);
    lv_group_focus_obj(ui_standby_screen);

    printf("[%s] Standby screen initialized successfully\n", standby_screen.name);
}

/**
 * @brief Deinitialize the standby screen
 *
 * This function cleans up the standby screen by stopping the clock timer,
 * deleting the UI objects, and removing event callbacks.
 */
void standby_screen_deinit(void)
{
    printf("[%s] Deinitializing standby screen\n", standby_screen.name);

    // Stop and delete clock timer
    if (clock_timer) {
        lv_timer_delete(clock_timer);
        clock_timer = NULL;
    }

    // Clear label pointers
    time_label = NULL;
    date_label = NULL;

    if (ui_standby_screen) {
        lv_obj_remove_event_cb(ui_standby_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_standby_screen);
        // Screen object will be cleaned up by screen manager
    }

    printf("[%s] Standby screen deinitialized\n", standby_screen.name);
}
