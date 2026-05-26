/**
 * @file main_screen.c
 * @brief Implementation of the main screen for the application
 *
 * This file contains the implementation of the main screen which displays
 * the main AI Pocket Pet interface including status bar, pet area, and
 * menu system. This is the primary screen after the startup sequence.
 *
 * The main screen includes:
 * - A white background main screen
 * - Status bar with network and battery indicators
 * - Pet area with animated pet character
 * - Bottom menu system for navigation
 * - Sub-menu overlay system
 * - Toast notification system
 * - Keyboard event handling
 *
 * @copyright Copyright (c) 2024 LVGL PC Simulator Project
 */

#include "main_screen.h"
#include "toast_screen.h"
#include "menu_bath_screen.h"
#include "menu_food_screen.h"
#include "menu_health_screen.h"
#include "menu_info_screen.h"
#include "menu_scan_screen.h"
#include "menu_sleep_screen.h"
#include "menu_video_screen.h"
#include "standby_screen.h"
#include "pet_show_screen.h"
#include "ebook_screen.h"
#include "rfid_scan_screen.h"
#include "camera_screen.h"
#ifdef ENABLE_LVGL_HARDWARE
#include "game_pet.h"
#endif
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#if defined(ENABLE_LVGL_HARDWARE)
#include "axp2101_driver.h"
#include "tal_system.h"
#include "tal_kv.h"
#include "tal_time_service.h"
#include "netmgr.h"
#endif
/***********************************************************
***********************Type Definitions********************
***********************************************************/

/***********************************************************
***********************Image Declarations*****************
***********************************************************/

// External image declarations for menu icons
LV_IMG_DECLARE(info_icon);   // From icons/menu_info_icon.c
LV_IMG_DECLARE(eat_icon);    // From icons/menu_eat_icon.c
LV_IMG_DECLARE(toilet_icon); // From icons/menu_toilet_icon.c
LV_IMG_DECLARE(sick_icon);   // From icons/menu_sick_icon.c
LV_IMG_DECLARE(sleep_icon);  // From icons/menu_sleep_icon.c
LV_IMG_DECLARE(camera_icon); // From icons/menu_camera_icon.c
LV_IMG_DECLARE(scan_icon);   // From icons/menu_scan_icon.c

// Status bar icons declarations
LV_IMG_DECLARE(wifi_1_bar_icon);
LV_IMG_DECLARE(wifi_2_bar_icon);
LV_IMG_DECLARE(wifi_3_bar_icon);
LV_IMG_DECLARE(wifi_off_icon);
LV_IMG_DECLARE(wifi_find_icon);
LV_IMG_DECLARE(wifi_add_icon);
// LV_IMG_DECLARE(four_g_logo_icon);
// LV_IMG_DECLARE(cellular_1_bar_icon);
// LV_IMG_DECLARE(cellular_2_bar_icon);
// LV_IMG_DECLARE(cellular_3_bar_icon);
LV_IMG_DECLARE(four_g_icon);
LV_IMG_DECLARE(cellular_off_icon);
LV_IMG_DECLARE(cellular_connected_no_internet_icon);
LV_IMG_DECLARE(battery_0_icon);
LV_IMG_DECLARE(battery_1_icon);
LV_IMG_DECLARE(battery_2_icon);
LV_IMG_DECLARE(battery_3_icon);
LV_IMG_DECLARE(battery_4_icon);
LV_IMG_DECLARE(battery_5_icon);
LV_IMG_DECLARE(battery_full_icon);
LV_IMG_DECLARE(battery_charging_icon);

/***********************************************************
***********************defines****************************
***********************************************************/

#ifndef AI_PET_SCREEN_WIDTH
#define AI_PET_SCREEN_WIDTH 384
#endif
#ifndef AI_PET_SCREEN_HEIGHT
#define AI_PET_SCREEN_HEIGHT 168
#endif

#define MENU_BUTTON_COUNT       5
#define BATTERY_UPDATE_INTERVAL 1000
#define UI_UPDATE_INTERVAL      100
#define STANDBY_TIME            30 // Seconds of inactivity before standby
#define PET_NAME_KV_KEY         "pet_name"

// Font definitions - easily customizable
#define SCREEN_TITLE_FONT   &lv_font_terminusTTF_Bold_18
#define SCREEN_CONTENT_FONT &lv_font_terminusTTF_Bold_16
#define SCREEN_INFO_FONT    &lv_font_terminusTTF_Bold_14

/***********************************************************
***********************variable define**********************
***********************************************************/

static lv_obj_t *ui_main_screen;

// Main screen UI components
static lv_obj_t *status_bar;
static lv_obj_t *bottom_menu;
static lv_obj_t *horizontal_line;

// Menu system
static lv_obj_t *menu_buttons[MENU_BUTTON_COUNT];
static uint8_t   current_selected_button = 0;

// Status bar components
static lv_obj_t *wifi_icon;
// static lv_obj_t *four_g_logo_obj;
static lv_obj_t *cellular_icon;
static lv_obj_t *battery_icon;
static lv_obj_t *battery_label; // Battery info label
static lv_obj_t *clock_label;   // Clock display label

// Status tracking
static uint8_t current_wifi_strength      = 0;
static uint8_t current_cellular_strength  = 2;
static uint8_t current_cellular_connected = true;
static uint8_t current_battery_level      = 4;
static bool    current_battery_charging   = false;

// Clock variables
static uint8_t current_hour    = 12;
static uint8_t current_minute  = 0;
static bool    clock_edit_mode = false;
static bool    clock_edit_hour = true; // true = editing hour, false = editing minute
static TIME_T  last_sync_time  = 0;    // Last sync time for system time update

// UI update timer (for status bar updates)
static lv_timer_t *ui_update_timer = NULL;

// Standby mode timer
static uint16_t standby_time = 0;

// Pet event callback
static pet_event_callback_t pet_event_callback  = NULL;
static void                *pet_event_user_data = NULL;

// Pet stats storage
static pet_stats_t main_screen_pet_stats;

Screen_t main_screen = {
    .init       = main_screen_init,
    .deinit     = main_screen_deinit,
    .screen_obj = &ui_main_screen,
    .name       = "Main",
};

/***********************************************************
********************function declaration********************
***********************************************************/

static void keyboard_event_cb(lv_event_t *e);
static void create_main_ui_components(void);

// Menu system functions
static void      update_menu_button_selection(uint8_t old_selection, uint8_t new_selection);
static void      handle_menu_navigation(uint32_t key);
static void      handle_menu_selection(void);
static lv_obj_t *create_bottom_menu(lv_obj_t *parent);
static void      handle_main_navigation(uint32_t key);
static uint8_t   get_selected_button(void);

// UI component creation functions
static lv_obj_t *simple_status_bar_create(lv_obj_t *parent);

// Status bar icon helper functions (inline for performance)
static inline const lv_img_dsc_t *get_wifi_icon_by_strength(uint8_t strength);
static inline const lv_img_dsc_t *get_cellular_icon_by_strength(uint8_t strength, uint8_t connected);
static inline const lv_img_dsc_t *get_battery_icon_by_level(uint8_t level, bool charging);

// UI update timer callback
static void ui_update_timer_cb(lv_timer_t *timer);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Keyboard event callback
 *
 * This function handles keyboard events for the main screen.
 *
 * @param e The event object
 */
static void keyboard_event_cb(lv_event_t *e)
{
    if (e == NULL) {
        return;
    }

    standby_time = 0; // Reset standby timer on any key event

    // lv_event_code_t code = lv_event_get_code(e);

    uint32_t key = lv_event_get_key(e);

    // Simple key handling with pet animation controls and menu navigation
    switch (key) {
    case KEY_UP:
        printf("[%s] Keyboard event: UP\n", main_screen.name);
        if (clock_edit_mode) {
            // In clock edit mode: UP increases value
            if (clock_edit_hour) {
                current_hour++;
                if (current_hour >= 24)
                    current_hour = 0;
            } else {
                current_minute++;
                if (current_minute >= 60)
                    current_minute = 0;
            }
            printf("[%s] Clock adjusted: %02d:%02d\n", main_screen.name, current_hour, current_minute);
        } else {
            // UP key works like LEFT key - navigate to previous icon
            handle_menu_navigation(key);
        }
        break;
    case KEY_DOWN:
        printf("[%s] Keyboard event: DOWN\n", main_screen.name);
        if (clock_edit_mode) {
            // In clock edit mode: DOWN decreases value
            if (clock_edit_hour) {
                if (current_hour == 0)
                    current_hour = 23;
                else
                    current_hour--;
            } else {
                if (current_minute == 0)
                    current_minute = 59;
                else
                    current_minute--;
            }
            printf("[%s] Clock adjusted: %02d:%02d\n", main_screen.name, current_hour, current_minute);
        } else {
            // DOWN key works like RIGHT key - navigate to next icon
            handle_menu_navigation(key);
        }
        break;
    case KEY_LEFT:
        printf("[%s] Keyboard event: LEFT\n", main_screen.name);
        if (clock_edit_mode) {
            // In clock edit mode: LEFT switches to hour editing
            clock_edit_hour = true;
            printf("[%s] Clock editing hour\n", main_screen.name);
        } else {
            // Always handle menu navigation for LEFT/RIGHT keys
            handle_menu_navigation(key);
        }
        break;
    case KEY_RIGHT:
        printf("[%s] Keyboard event: RIGHT\n", main_screen.name);
        if (clock_edit_mode) {
            // In clock edit mode: RIGHT switches to minute editing
            clock_edit_hour = false;
            printf("[%s] Clock editing minute\n", main_screen.name);
        } else {
            // Always handle menu navigation for LEFT/RIGHT keys
            handle_menu_navigation(key);
        }
        break;
    case KEY_ENTER:
        printf("[%s] Keyboard event: ENTER\n", main_screen.name);
        if (clock_edit_mode) {
            // In clock edit mode: ENTER saves and exits
            clock_edit_mode = false;
            // Save time to system
            POSIX_TM_S tm     = {0};
            tm.tm_hour        = current_hour;
            tm.tm_min         = current_minute;
            tm.tm_sec         = 0;
            tm.tm_mday        = 1;
            tm.tm_mon         = 0;
            tm.tm_year        = 124; // 2024 - 1900
            TIME_T posix_time = tal_time_mktime(&tm);
            if (posix_time > 0) {
                tal_time_set_posix(posix_time, 2); // 2 = other source
            }
            toast_screen_show("Clock saved", 1500);
            printf("[%s] Clock saved: %02d:%02d\n", main_screen.name, current_hour, current_minute);
        } else {
            // ENTER always triggers menu selection
            handle_menu_selection();
        }
        break;
    case KEY_ESC:
        printf("[%s] Keyboard event: ESC\n", main_screen.name);
        // ESC exits clock edit mode or shows help message
        if (clock_edit_mode) {
            clock_edit_mode = false;
            // Save time to system
            POSIX_TM_S tm     = {0};
            tm.tm_hour        = current_hour;
            tm.tm_min         = current_minute;
            tm.tm_sec         = 0;
            tm.tm_mday        = 1;
            tm.tm_mon         = 0;
            tm.tm_year        = 124; // 2024 - 1900
            TIME_T posix_time = tal_time_mktime(&tm);
            if (posix_time > 0) {
                tal_time_set_posix(posix_time, 2); // 2 = other source
            }
            toast_screen_show("Clock saved", 1500);
            printf("[%s] Clock edit mode exited, time saved: %02d:%02d\n", main_screen.name, current_hour,
                   current_minute);
        } else {
            // toast_screen_show("Use LEFT/RIGHT to select, ENTER to confirm", 2000);
        }
        break;
    case KEY_JOYCON: {
#if defined(ENABLE_LVGL_HARDWARE)
        // uint8_t chat_text[] = "Tell me today's weather and tell me a new story";
        // toast_screen_show("Tell you a new story", 1000);
        // ai_text_agent_upload(chat_text, sizeof(chat_text));
        screen_load(&camera_screen);
#else
        toast_screen_show("Unlock at Higher Level", 2000);
#endif
        break;
    }
#if !defined(ENABLE_LVGL_HARDWARE)
    // Battery testing keys
    case 97: // 'a' key - Battery 0 (empty)
        printf("A key pressed - Setting battery to empty\n");
        main_screen_set_battery_state(0, false);
        break;
    case 115: // 's' key - Battery 1
        printf("S key pressed - Setting battery to 1 bar\n");
        main_screen_set_battery_state(1, false);
        break;
    case 100: // 'd' key - Battery 2
        printf("D key pressed - Setting battery to 2 bars\n");
        main_screen_set_battery_state(2, false);
        break;
    case 102: // 'f' key - Battery 3
        printf("F key pressed - Setting battery to 3 bars\n");
        main_screen_set_battery_state(3, false);
        break;
    case 103: // 'g' key - Battery 4
        printf("G key pressed - Setting battery to 4 bars\n");
        main_screen_set_battery_state(4, false);
        break;
    case 104: // 'h' key - Battery 5
        printf("H key pressed - Setting battery to 5 bars\n");
        main_screen_set_battery_state(5, false);
        break;
    case 106: // 'j' key - Battery 6 (full)
        printf("J key pressed - Setting battery to full\n");
        main_screen_set_battery_state(6, false);
        break;
    case 99: // 'c' key - Battery charging
        printf("C key pressed - Setting battery to charging\n");
        // screen_load(&standby_screen);
        // screen_load(&ebook_screen);
        // screen_load(&rfid_scan_screen);
        break;
#endif
    default:
        printf("[%s] Keyboard event: %d\n", main_screen.name, key);
        break;
    }
}

/**
 * @brief Create main UI components
 *
 * This function creates all the main UI components including status bar,
 * pet area, menu system, and toast notifications.
 */
static void create_main_ui_components(void)
{
    if (ui_main_screen == NULL) {
        printf("[%s] Error: Cannot create UI components - main screen is NULL\n", main_screen.name);
        return;
    }

    // Create UI components using simple inline implementations
    status_bar = simple_status_bar_create(ui_main_screen);
    if (status_bar == NULL) {
        printf("[%s] Warning: Failed to create status bar\n", main_screen.name);
    }

    // Create bottom menu using the real menu system FIRST (lowest layer)
    bottom_menu = create_bottom_menu(ui_main_screen);
    if (bottom_menu == NULL) {
        printf("[%s] Warning: Failed to create bottom menu\n", main_screen.name);
    }

    // Add horizontal line across the screen, 2px thick, positioned 1/3 from bottom
    horizontal_line = lv_obj_create(ui_main_screen);
    if (horizontal_line == NULL) {
        printf("[%s] Error: Failed to create horizontal line\n", main_screen.name);
    } else {
        lv_obj_set_size(horizontal_line, AI_PET_SCREEN_WIDTH, 2);
        lv_obj_align(horizontal_line, LV_ALIGN_TOP_LEFT, 0, 112); // 168 * (2/3) = 112 pixels from top
        lv_obj_set_style_bg_color(horizontal_line, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(horizontal_line, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(horizontal_line, 0, 0);
        lv_obj_set_style_pad_all(horizontal_line, 0, 0);
    }
}

/**
 * @brief Initialize the main screen
 *
 * This function creates the main screen UI with a white background,
 * initializes all UI components, and sets up event handling.
 */
void main_screen_init(void)
{
    // Create main screen object
    ui_main_screen = lv_obj_create(NULL);
    if (ui_main_screen == NULL) {
        printf("[%s] Error: Failed to create main screen object\n", main_screen.name);
        return;
    }

    lv_obj_set_size(ui_main_screen, AI_PET_SCREEN_WIDTH, AI_PET_SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_main_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_main_screen, LV_OPA_COVER, 0);

    // Create all UI components
    create_main_ui_components();

    // Add keyboard event handler to the screen
    lv_obj_add_event_cb(ui_main_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);

    // Make sure the screen can receive keyboard focus
    lv_group_t *group = lv_group_get_default();
    if (group == NULL) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    lv_group_add_obj(group, ui_main_screen);
    lv_group_focus_obj(ui_main_screen);

    // Initialize pet stats
    main_screen_init_pet_stats(NULL);

    // Start UI update timer for status bar updates
    ui_update_timer = lv_timer_create(ui_update_timer_cb, UI_UPDATE_INTERVAL, NULL);
}

/**
 * @brief Deinitialize the main screen
 *
 * This function cleans up the main screen by stopping animations,
 * deleting the UI object, and removing event callbacks.
 */
void main_screen_deinit(void)
{
    // Stop UI update timer
    if (ui_update_timer) {
        lv_timer_del(ui_update_timer);
        ui_update_timer = NULL;
    }

    // Remove event callback and delete the main screen object
    if (ui_main_screen) {
        // Remove from group before deleting
        lv_obj_remove_event_cb(ui_main_screen, keyboard_event_cb); // Remove event callback
        lv_group_remove_obj(ui_main_screen);                       // Remove from group
        printf("deinit main screen\n");
        // Delete the main screen object
        // lv_obj_del(ui_main_screen);
        // ui_main_screen = NULL;
    }

    // Reset component pointers
    status_bar      = NULL;
    bottom_menu     = NULL;
    horizontal_line = NULL;

    // Reset status bar icon pointers
    wifi_icon = NULL;
    // four_g_logo_obj = NULL;
    cellular_icon = NULL;
    battery_icon  = NULL;
    battery_label = NULL;
    clock_label   = NULL;

    // Reset menu system variables

    for (int i = 0; i < MENU_BUTTON_COUNT; i++) {
        menu_buttons[i] = NULL;
    }
}

/***********************************************************
******************UI Component Creation********************
***********************************************************/

static lv_obj_t *simple_status_bar_create(lv_obj_t *parent)
{
    if (parent == NULL) {
        printf("Error: Cannot create status bar - parent is NULL\n");
        return NULL;
    }

    lv_obj_t *status_bar = lv_obj_create(parent);
    if (status_bar == NULL) {
        printf("Error: Failed to create status bar object\n");
        return NULL;
    }

    lv_obj_set_size(status_bar, AI_PET_SCREEN_WIDTH, 24);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 2, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi icon (image widget)
    wifi_icon = lv_img_create(status_bar);
    lv_obj_set_size(wifi_icon, 24, 24);
    lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 5, 0);

    // 4G logo icon (static, 24px)
    // four_g_logo_obj = lv_img_create(status_bar);
    // lv_obj_set_size(four_g_logo_obj, 24, 24);
    // lv_obj_align(four_g_logo_obj, LV_ALIGN_LEFT_MID, 35, 0);
    // lv_img_set_src(four_g_logo_obj, &four_g_icon);

    // Cellular signal icon
    cellular_icon = lv_img_create(status_bar);
    lv_obj_set_size(cellular_icon, 24, 24);
    lv_obj_align(cellular_icon, LV_ALIGN_LEFT_MID, 35, 0);

    // Battery info label (voltage and percentage)
    battery_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(battery_label, SCREEN_INFO_FONT, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_black(), 0);
    lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -35, 0);
    // lv_label_set_text(battery_label, "4.2V 100%");

    // Battery icon (image widget)
    battery_icon = lv_img_create(status_bar);
    lv_obj_set_size(battery_icon, 24, 24);
    lv_obj_align(battery_icon, LV_ALIGN_RIGHT_MID, -5, 0);

    // Clock label (centered) - simple text label for e-ink display
    clock_label = lv_label_create(status_bar);
    lv_obj_set_style_text_font(clock_label, SCREEN_INFO_FONT, 0);
    lv_obj_set_style_text_color(clock_label, lv_color_black(), 0);
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(clock_label, "12:00");

    return status_bar;
}

// Menu system integration functions
static void update_menu_button_selection(uint8_t old_selection, uint8_t new_selection)
{
    // Update visual selection in bottom menu - following menu_system.c exactly
    if (old_selection < MENU_BUTTON_COUNT && new_selection < MENU_BUTTON_COUNT) {
        lv_obj_t *old_btn = menu_buttons[old_selection];
        lv_obj_t *new_btn = menu_buttons[new_selection];

        // Reset old button style - following menu_system.c reference implementation
        if (old_btn) {
            lv_obj_set_style_bg_color(old_btn, lv_color_white(), 0);
            lv_obj_set_style_border_width(old_btn, 0, 0);
            lv_obj_set_style_shadow_width(old_btn, 0, 0);

            // Handle old button content styling (image inside button)
            lv_obj_t *old_child = lv_obj_get_child(old_btn, 0);
            if (old_child && lv_obj_check_type(old_child, &lv_image_class)) {
                // Reset image styling for unselected state
                lv_obj_set_style_img_recolor_opa(old_child, LV_OPA_TRANSP, 0);
                lv_obj_set_style_img_recolor(old_child, lv_color_black(), 0);
                lv_obj_set_style_img_opa(old_child, LV_OPA_COVER, 0);
            }
        }

        // Set new button style - following menu_system.c reference implementation
        if (new_btn) {
            lv_obj_set_style_bg_color(new_btn, lv_color_black(), 0);
            lv_obj_set_style_border_color(new_btn, lv_color_black(), 0);
            lv_obj_set_style_border_width(new_btn, 2, 0);
            lv_obj_set_style_shadow_width(new_btn, 0, 0);

            // Handle new button content styling (image inside button)
            lv_obj_t *new_child = lv_obj_get_child(new_btn, 0);
            if (new_child && lv_obj_check_type(new_child, &lv_image_class)) {
                // Invert the image colors for selected state - black becomes white, white becomes black
                lv_obj_set_style_img_recolor_opa(new_child, LV_OPA_COVER, 0);
                lv_obj_set_style_img_recolor(new_child, lv_color_white(), 0);
                lv_obj_set_style_img_opa(new_child, LV_OPA_COVER, 0);
            }
        }
    }
}

static void handle_menu_navigation(uint32_t key)
{
    // Forward navigation to the real menu system for visual feedback
    handle_main_navigation(key);
}

static void handle_menu_selection(void)
{
    // Get the currently selected button from menu system
    uint8_t selected_button = get_selected_button();

    printf("[%s] Menu selection: button %d\n", main_screen.name, selected_button);

    // Switch to corresponding screen using screen manager stack
    // Note: Food, Bath, Health moved to pet show screen
    switch (selected_button) {
    case 0: // Info
        printf("Loading info screen\n");
        screen_load(&menu_info_screen);
        break;
    case 1: // Sleep
        printf("Loading sleep screen\n");
        screen_load(&menu_sleep_screen);
        break;
    case 2: // Video
        printf("Loading video screen\n");
        screen_load(&menu_video_screen);
        break;
    case 3: // Scan
        printf("Loading scan screen\n");
        screen_load(&menu_scan_screen);
        break;
    case 4: // Pet Show
        printf("Loading pet show screen\n");
        screen_load(&pet_show_screen);
        break;
    default:
        printf("Unknown menu selection: %d\n", selected_button);
        break;
    }
}

/***********************************************************
***********State Setting Interface Functions***************
***********************************************************/

/**
 * @brief Set WiFi signal strength state (state will be updated in next timer cycle)
 * @param strength WiFi signal strength (0-5)
 */
void main_screen_set_wifi_state(uint8_t strength)
{
    current_wifi_strength = strength;
    printf("[%s] WiFi strength set to: %d\n", main_screen.name, strength);
}

/**
 * @brief Set battery state (state will be updated in next timer cycle)
 * @param level Battery level (0-6)
 * @param charging Battery charging status
 */
void main_screen_set_battery_state(uint8_t level, bool charging)
{
    current_battery_level    = level;
    current_battery_charging = charging;
    printf("[%s] Battery state set to: level=%d, charging=%d\n", main_screen.name, level, charging);
}

/***********************************************************
******************Status Bar Icon Helpers******************
***********************************************************/
static inline const lv_img_dsc_t *get_wifi_icon_by_strength(uint8_t strength)
{
    switch (strength) {
    case 1:
        return &wifi_1_bar_icon;
    case 2:
        return &wifi_2_bar_icon;
    case 3:
        return &wifi_3_bar_icon;
    case 4:
        return &wifi_find_icon;
    case 5:
        return &wifi_add_icon;
    default:
        return &wifi_off_icon;
    }
}

static inline const lv_img_dsc_t *get_cellular_icon_by_strength(uint8_t strength, uint8_t connected)
{
    // if (strength == 0) return &cellular_off_icon;
    // if (strength == 4 || !connected) return &cellular_connected_no_internet_icon;

    // switch (strength) {
    //     case 1: return &cellular_1_bar_icon;
    //     case 2: return &cellular_2_bar_icon;
    //     case 3: return &cellular_3_bar_icon;
    //     default: return &cellular_off_icon;
    // }

    if (connected)
        return &four_g_icon;
    else
        return &cellular_off_icon;
}

static inline const lv_img_dsc_t *get_battery_icon_by_level(uint8_t level, bool charging)
{
    if (charging)
        return &battery_charging_icon;

    switch (level) {
    case 0:
        return &battery_0_icon;
    case 1:
        return &battery_1_icon;
    case 2:
        return &battery_2_icon;
    case 3:
        return &battery_3_icon;
    case 4:
        return &battery_4_icon;
    case 5:
        return &battery_5_icon;
    case 6:
        return &battery_full_icon;
    default:
        return &battery_full_icon;
    }
}

/***********************************************************
*****************UI Update Timer Callback******************
***********************************************************/

/**
 * @brief UI update timer callback - updates all UI elements based on current state
 */
static void ui_update_timer_cb(lv_timer_t *timer)
{
    static uint32_t second_counter = 0;

    if (standby_time++ > STANDBY_TIME * 1000 / UI_UPDATE_INTERVAL) {
        // Enter standby mode
        printf("[%s] Entering standby mode due to inactivity\n", main_screen.name);
        screen_load(&standby_screen);
        standby_time = 0;
    }

    // Update clock every second
    second_counter++;
    if (second_counter >= 1000 / UI_UPDATE_INTERVAL) {
        second_counter = 0;

        if (!clock_edit_mode) {
            // Update time from system time (synchronized with RTC/cloud)
            TIME_T current_posix = tal_time_get_posix();

            // Only update if time has changed (handles potential time jumps)
            if (current_posix != last_sync_time && current_posix > 0) {
                last_sync_time = current_posix;

                POSIX_TM_S local_time;
                if (tal_time_get_local_time_custom(current_posix, &local_time) == OPRT_OK) {
                    current_hour   = (uint8_t)local_time.tm_hour;
                    current_minute = (uint8_t)local_time.tm_min;
                } else {
                    // Fallback: manual increment if system time fails
                    current_minute++;
                    if (current_minute >= 60) {
                        current_minute = 0;
                        current_hour++;
                        if (current_hour >= 24) {
                            current_hour = 0;
                        }
                    }
                }
            } else if (current_posix == 0) {
                // System time not available, use manual increment
                current_minute++;
                if (current_minute >= 60) {
                    current_minute = 0;
                    current_hour++;
                    if (current_hour >= 24) {
                        current_hour = 0;
                    }
                }
            }
        }

        // Update clock display
        if (clock_label) {
            char time_text[16];
            if (clock_edit_mode) {
                // Show editing mode with highlighted field
                if (clock_edit_hour) {
                    snprintf(time_text, sizeof(time_text), "[%02d]:%02d", current_hour, current_minute);
                } else {
                    snprintf(time_text, sizeof(time_text), "%02d:[%02d]", current_hour, current_minute);
                }
            } else {
                snprintf(time_text, sizeof(time_text), "%02d:%02d", current_hour, current_minute);
            }
            lv_label_set_text(clock_label, time_text);
        }
    }

    // Update WiFi icon if changed
    if (wifi_icon) {
        lv_img_set_src(wifi_icon, get_wifi_icon_by_strength(current_wifi_strength));
    }

    // Update cellular icon if changed
    if (cellular_icon) {
#ifdef ENABLE_LVGL_HARDWARE
        netmgr_status_e status;
        OPERATE_RET     result = netmgr_conn_get(NETCONN_CELLULAR, NETCONN_CMD_STATUS, &status);
        if (result == OPRT_OK) {
            current_cellular_connected = status;
        }
#endif
        lv_img_set_src(cellular_icon,
                       get_cellular_icon_by_strength(current_cellular_strength, current_cellular_connected));
    }

#if defined(ENABLE_LVGL_HARDWARE)
    // Read from hardware
    uint16_t voltage_mv      = axp2101_getBattVoltage();
    uint8_t  battery_percent = axp2101_getBatteryPercent();
    current_battery_charging = axp2101_isCharging();

    // Update state
    current_battery_level = (uint8_t)(battery_percent / 100.0f * 7);
    if (current_battery_level > 6)
        current_battery_level = 6;

    // Update label using snprintf
    if (battery_label) {
        char battery_text[16];
        snprintf(battery_text, sizeof(battery_text), "%.1fV %d%%", ((float)voltage_mv) / 1000.0f, battery_percent);
        lv_label_set_text(battery_label, battery_text);
    }
#else
    // PC simulator mode - update label based on current state
    if (battery_label) {
        char  battery_text[20];
        int   demo_percent = current_battery_level * 100 / 7;
        float demo_voltage = 3.0f + (current_battery_level * 1.2f / 6);
        if (current_battery_charging) {
            snprintf(battery_text, sizeof(battery_text), "%.1fV %d%% CHG", demo_voltage, demo_percent);
        } else {
            snprintf(battery_text, sizeof(battery_text), "%.1fV %d%%", demo_voltage, demo_percent);
        }
        lv_label_set_text(battery_label, battery_text);
    }
#endif
    if (battery_icon) {
        lv_img_set_src(battery_icon, get_battery_icon_by_level(current_battery_level, current_battery_charging));
    }
}

/***********************************************************
*****************Menu System Functions*********************
***********************************************************/

/**
 * @brief Create the bottom menu with navigation buttons
 */
static lv_obj_t *create_bottom_menu(lv_obj_t *parent)
{
// Define constants to match menu_system.c
#define BOTTOM_MENU_HEIGHT  26
#define MENU_BUTTON_SIZE    20
#define MENU_BUTTON_SPACING 24
#define MENU_BUTTON_START_X (AI_PET_SCREEN_WIDTH - 160)

    // Create bottom menu container - transparent like menu_system.c
    lv_obj_t *bottom_container = lv_obj_create(parent);
    lv_obj_set_size(bottom_container, AI_PET_SCREEN_WIDTH, BOTTOM_MENU_HEIGHT);
    lv_obj_align(bottom_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(bottom_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom_container, 0, 0);
    lv_obj_set_style_pad_all(bottom_container, 2, 0);

    // Menu icons array - removed eat, toilet, sick (moved to pet show screen)
    const lv_img_dsc_t *menu_icons[] = {&info_icon,  &sleep_icon,  &camera_icon, &scan_icon,
                                        &info_icon};

    // Create menu buttons exactly like menu_system.c
    for (uint8_t i = 0; i < MENU_BUTTON_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(bottom_container);
        lv_obj_set_size(btn, MENU_BUTTON_SIZE, MENU_BUTTON_SIZE);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -(MENU_BUTTON_START_X - i * MENU_BUTTON_SPACING), 0);

        // Set default button style like menu_system.c
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);

        // Create icon image inside button like menu_system.c
        lv_obj_t *img = lv_img_create(btn);
        lv_img_set_src(img, menu_icons[i]);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

        // Store button reference for selection updates
        menu_buttons[i] = btn;
    }

    // Initialize first button as selected like menu_system.c
    update_menu_button_selection(0, current_selected_button);

    return bottom_container;
}

/**
 * @brief Handle main menu navigation
 */
static void handle_main_navigation(uint32_t key)
{
    uint8_t old_selection = current_selected_button;
    uint8_t new_selection = old_selection;

    switch (key) {
    case KEY_UP:
    case KEY_LEFT:
        // Move to previous icon with wrap around
        if (current_selected_button > 0) {
            new_selection = current_selected_button - 1;
        } else {
            new_selection = MENU_BUTTON_COUNT - 1; // Wrap to last icon
        }
        break;
    case KEY_DOWN:
    case KEY_RIGHT:
        // Move to next icon with wrap around
        if (current_selected_button < MENU_BUTTON_COUNT - 1) {
            new_selection = current_selected_button + 1;
        } else {
            new_selection = 0; // Wrap to first icon
        }
        break;
    }

    if (new_selection != old_selection) {
        update_menu_button_selection(old_selection, new_selection);
        current_selected_button = new_selection;
        printf("[%s] Menu navigation: %d -> %d\n", main_screen.name, old_selection, new_selection);
    }
}

/**
 * @brief Get current selected button
 */
static uint8_t get_selected_button(void)
{
    return current_selected_button;
}

/***********************************************************
***************Pet Event Callback Functions***************
***********************************************************/

void main_screen_register_pet_event_callback(pet_event_callback_t callback, void *user_data)
{
    pet_event_callback  = callback;
    pet_event_user_data = user_data;
    printf("[%s] Pet event callback registered\n", main_screen.name);
}

pet_stats_t *main_screen_get_pet_stats(void)
{
    return &main_screen_pet_stats;
}

uint8_t main_screen_update_pet_stats(pet_stats_t *stats)
{
    if (NULL == stats) {
        return 1;
    }

    if (stats->health <= 100) {
        main_screen_pet_stats.health = stats->health;
    }
    if (stats->hungry <= 100) {
        main_screen_pet_stats.hungry = stats->hungry;
    }
    if (stats->clean <= 100) {
        main_screen_pet_stats.clean = stats->clean;
    }
    if (stats->happy <= 100) {
        main_screen_pet_stats.happy = stats->happy;
    }
    if (stats->age_days <= 999) {
        main_screen_pet_stats.age_days = stats->age_days;
    }
    if (stats->weight_kg <= 999.9) {
        main_screen_pet_stats.weight_kg = stats->weight_kg;
    }

    printf("[%s] Pet stats updated - Health: %d, Hungry: %d, Clean: %d, Happy: %d\n", main_screen.name, stats->health,
           stats->hungry, stats->clean, stats->happy);

    return 0;
}

void main_screen_init_pet_stats(pet_stats_t *stats)
{
    if (NULL == stats) {
        stats = &main_screen_pet_stats;
    }

    stats->health    = 85;
    stats->hungry    = 60;
    stats->clean     = 70;
    stats->happy     = 90;
    stats->age_days  = 15;
    stats->weight_kg = 1.2f;
    strncpy(stats->name, "Ducky", sizeof(stats->name) - 1);
    stats->name[sizeof(stats->name) - 1] = '\0';

    // Also initialize internal pet stats
    main_screen_pet_stats = *stats;

    printf("[%s] Pet stats initialized - Name: %s, Health: %d, Hungry: %d, Clean: %d, Happy: %d\n", main_screen.name,
           stats->name, stats->health, stats->hungry, stats->clean, stats->happy);
}

/**
 * @brief Trigger a pet event through the callback system
 * @param event_type Type of pet event
 */
static void trigger_pet_event(pet_event_type_t event_type)
{
    if (pet_event_callback != NULL) {
        printf("[%s] Triggering pet event: %d\n", main_screen.name, event_type);
        pet_event_callback(event_type, pet_event_user_data);
    } else {
        printf("[%s] Pet event callback not registered, cannot trigger event %d\n", main_screen.name, event_type);
    }
}

/**
 * @brief Handle pet event and trigger callback
 * @param event_type Type of pet event
 */
void main_screen_handle_pet_event(pet_event_type_t event_type)
{
    // Trigger the callback if registered
    trigger_pet_event(event_type);

    // Log event for debugging
    switch (event_type) {
    case PET_EVENT_FEED_HAMBURGER:
    case PET_EVENT_FEED_PIZZA:
    case PET_EVENT_FEED_APPLE:
    case PET_EVENT_FEED_FISH:
    case PET_EVENT_FEED_CARROT:
    case PET_EVENT_FEED_ICE_CREAM:
    case PET_EVENT_FEED_COOKIE:
        printf("[%s] Pet is eating\n", main_screen.name);
        break;
    case PET_EVENT_DRINK_WATER:
        printf("[%s] Pet is drinking water\n", main_screen.name);
        break;
    case PET_EVENT_TOILET:
        printf("[%s] Pet is using toilet\n", main_screen.name);
        break;
    case PET_EVENT_TAKE_BATH:
        printf("[%s] Pet is taking a bath\n", main_screen.name);
        break;
    case PET_EVENT_SEE_DOCTOR:
        printf("[%s] Pet is seeing the doctor\n", main_screen.name);
        break;
    case PET_EVENT_SLEEP:
        printf("[%s] Pet is sleeping\n", main_screen.name);
        break;
    case PET_EVENT_WAKE_UP:
        printf("[%s] Pet is waking up\n", main_screen.name);
        break;
    case PET_STAT_RANDOMIZE:
        printf("[%s] Pet stats randomized\n", main_screen.name);
        break;
    default:
        printf("[%s] Unknown pet event: %d\n", main_screen.name, event_type);
        break;
    }
}
