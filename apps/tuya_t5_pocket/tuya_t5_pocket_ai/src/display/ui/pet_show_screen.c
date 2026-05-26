/**
 * @file pet_show_screen.c
 * @brief Pet show screen implementation
 *
 * This screen displays the pet animation in full screen mode,
 * allowing users to view the pet's animations without the main screen UI.
 */

#include <stdlib.h>
#include <time.h>

#include "pet_show_screen.h"
#include "screen_manager.h"
#include "toast_screen.h"
#include "main_screen.h"

// External image declarations for ducky animations
LV_IMG_DECLARE(ducky_walk);
LV_IMG_DECLARE(ducky_walk_to_left);
LV_IMG_DECLARE(ducky_blink);
LV_IMG_DECLARE(ducky_stand_still);
LV_IMG_DECLARE(ducky_sleep);
LV_IMG_DECLARE(ducky_dance);
LV_IMG_DECLARE(ducky_eat);
LV_IMG_DECLARE(ducky_bath);
LV_IMG_DECLARE(ducky_toilet);
LV_IMG_DECLARE(ducky_sick);
LV_IMG_DECLARE(ducky_emotion_happy);
LV_IMG_DECLARE(ducky_emotion_angry);
LV_IMG_DECLARE(ducky_emotion_cry);

// Screen dimensions
#define SCREEN_WIDTH AI_PET_SCREEN_WIDTH
#define SCREEN_HEIGHT AI_PET_SCREEN_HEIGHT

// Animation constants
#define PET_ANIMATION_INTERVAL 100
#define PET_MOVEMENT_INTERVAL 200
#define PET_MOVEMENT_STEP 2
#define PET_MOVEMENT_LIMIT 80
#define PET_WALK_DURATION_MIN 2000
#define PET_WALK_DURATION_MAX 8000
#define PET_IDLE_DURATION_MIN 3000
#define PET_IDLE_DURATION_MAX 10000

// Pet care menu constants
#define CARE_MENU_HEIGHT 80
#define CARE_BUTTON_WIDTH 60
#define CARE_BUTTON_HEIGHT 50
#define CARE_AREA_MARGIN 10
#define CARE_MENU_BUTTON_COUNT 5

// Care menu action types
typedef enum {
    CARE_ACTION_FEED,
    CARE_ACTION_BATH,
    CARE_ACTION_HEAL,
    CARE_ACTION_PLAY,
    CARE_ACTION_TOILET
} care_action_t;

// Care menu items
typedef struct {
    const char *name;
    const char *icon;
    care_action_t action;
    bool available;
} care_menu_item_t;

// Static variables
static lv_obj_t *ui_pet_show_screen;
static lv_obj_t *gif_container;
static lv_obj_t *care_menu_container;
static lv_obj_t *care_buttons[CARE_MENU_BUTTON_COUNT];
static lv_obj_t *pet_image_walk;
static lv_obj_t *pet_image_walk_left;
static lv_obj_t *pet_image_blink;
static lv_obj_t *pet_image_stand;
static lv_obj_t *pet_image_sleep;
static lv_obj_t *pet_image_dance;
static lv_obj_t *pet_image_eat;
static lv_obj_t *pet_image_bath;
static lv_obj_t *pet_image_toilet;
static lv_obj_t *pet_image_sick;
static lv_obj_t *pet_image_happy;
static lv_obj_t *pet_image_angry;
static lv_obj_t *pet_image_cry;
static lv_obj_t *current_normal_image;

static lv_timer_t *pet_animation_timer = NULL;
static lv_timer_t *pet_movement_timer = NULL;

static int16_t pet_x_pos = 0;
static int8_t pet_direction = 1;
static bool pet_is_walking = false;
static uint8_t idle_animation_state = 1;
static uint32_t pet_state_timer = 0;
static uint32_t pet_state_duration = 0;
static uint32_t idle_animation_timer = 0;
static uint32_t idle_animation_duration = 0;
static int16_t last_pet_x_pos = 0;

// Care menu state
static bool care_menu_active = false;
static uint8_t care_menu_selected = 0;
static lv_timer_t *care_animation_timer = NULL;

Screen_t pet_show_screen = {
    .init = pet_show_screen_init,
    .deinit = pet_show_screen_deinit,
    .screen_obj = &ui_pet_show_screen,
    .name = "PetShow",
};

// Forward declarations
static void keyboard_event_cb(lv_event_t *e);
static void pet_animation_cb(lv_timer_t *timer);
static void pet_movement_cb(lv_timer_t *timer);
static const lv_img_dsc_t *get_gif_src_by_state(bool is_walking, int8_t direction, uint8_t idle_state);
static lv_obj_t *get_gif_object_by_src(const lv_img_dsc_t *gif_src);
static void switch_pet_animation(lv_obj_t *target_image);
static void create_care_menu(void);
static void update_care_selection(void);
static void perform_care_action(uint8_t action_index);
static void care_animation_cb(lv_timer_t *timer);
static void show_care_animation(lv_obj_t *animation_image, const char *toast_msg);

/**
 * @brief Initialize the pet show screen
 */
void pet_show_screen_init(void)
{
    // Create main screen object
    ui_pet_show_screen = lv_obj_create(NULL);
    if (ui_pet_show_screen == NULL) {
        printf("[PetShow] Error: Failed to create screen object\n");
        return;
    }

    lv_obj_set_size(ui_pet_show_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(ui_pet_show_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(ui_pet_show_screen, LV_OPA_COVER, 0);

    // Create GIF container (move up to leave space for feed menu)
    gif_container = lv_obj_create(ui_pet_show_screen);
    lv_obj_set_size(gif_container, 180, 180);
    lv_obj_align(gif_container, LV_ALIGN_CENTER, -20, 0);  // Move up by 20 pixels
    lv_obj_set_style_bg_opa(gif_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gif_container, 0, 0);

    // Create all GIF widgets
    pet_image_walk = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_walk, &ducky_walk);
    lv_obj_align(pet_image_walk, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_walk, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_walk, LV_OPA_TRANSP, 0);

    pet_image_walk_left = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_walk_left, &ducky_walk_to_left);
    lv_obj_align(pet_image_walk_left, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_walk_left, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_walk_left, LV_OPA_TRANSP, 0);

    pet_image_blink = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_blink, &ducky_blink);
    lv_obj_align(pet_image_blink, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_blink, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_blink, LV_OPA_TRANSP, 0);

    pet_image_stand = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_stand, &ducky_stand_still);
    lv_obj_align(pet_image_stand, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_stand, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_stand, LV_OPA_TRANSP, 0);

    pet_image_sleep = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_sleep, &ducky_sleep);
    lv_obj_align(pet_image_sleep, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_sleep, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_sleep, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_sleep, LV_OBJ_FLAG_HIDDEN);

    pet_image_dance = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_dance, &ducky_dance);
    lv_obj_align(pet_image_dance, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_dance, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_dance, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_dance, LV_OBJ_FLAG_HIDDEN);

    pet_image_eat = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_eat, &ducky_eat);
    lv_obj_align(pet_image_eat, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_eat, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_eat, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_eat, LV_OBJ_FLAG_HIDDEN);

    pet_image_bath = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_bath, &ducky_bath);
    lv_obj_align(pet_image_bath, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_bath, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_bath, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_bath, LV_OBJ_FLAG_HIDDEN);

    pet_image_toilet = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_toilet, &ducky_toilet);
    lv_obj_align(pet_image_toilet, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_toilet, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_toilet, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_toilet, LV_OBJ_FLAG_HIDDEN);

    pet_image_sick = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_sick, &ducky_sick);
    lv_obj_align(pet_image_sick, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_sick, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_sick, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_sick, LV_OBJ_FLAG_HIDDEN);

    pet_image_happy = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_happy, &ducky_emotion_happy);
    lv_obj_align(pet_image_happy, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_happy, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_happy, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_happy, LV_OBJ_FLAG_HIDDEN);

    pet_image_angry = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_angry, &ducky_emotion_angry);
    lv_obj_align(pet_image_angry, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_angry, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_angry, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_angry, LV_OBJ_FLAG_HIDDEN);

    pet_image_cry = lv_gif_create(gif_container);
    lv_gif_set_src(pet_image_cry, &ducky_emotion_cry);
    lv_obj_align(pet_image_cry, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_size(pet_image_cry, 159, 164);
    lv_obj_set_style_bg_opa(pet_image_cry, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(pet_image_cry, LV_OBJ_FLAG_HIDDEN);

    // Initialize state
    current_normal_image = pet_image_stand;
    lv_obj_add_flag(pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);

    pet_x_pos = 0;
    pet_direction = 1;
    pet_is_walking = false;
    idle_animation_state = 1;
    pet_state_timer = 0;
    pet_state_duration = PET_IDLE_DURATION_MIN + (rand() % (PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN));
    idle_animation_timer = 0;
    idle_animation_duration = 4000 + (rand() % 8000);
    last_pet_x_pos = 0;

    // Add keyboard event handler
    lv_obj_add_event_cb(ui_pet_show_screen, keyboard_event_cb, LV_EVENT_KEY, NULL);

    // Add to group for keyboard focus
    lv_group_t *group = lv_group_get_default();
    if (group == NULL) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    lv_group_add_obj(group, ui_pet_show_screen);
    lv_group_focus_obj(ui_pet_show_screen);

    // Create care menu at the bottom
    create_care_menu();

    // Start animations
    srand(time(NULL));
    pet_animation_timer = lv_timer_create(pet_animation_cb, PET_ANIMATION_INTERVAL, NULL);
    pet_movement_timer = lv_timer_create(pet_movement_cb, PET_MOVEMENT_INTERVAL, NULL);

    printf("[PetShow] Screen initialized\n");
}

/**
 * @brief Deinitialize the pet show screen
 */
void pet_show_screen_deinit(void)
{
    // Stop timers
    if (pet_animation_timer) {
        lv_timer_del(pet_animation_timer);
        pet_animation_timer = NULL;
    }
    if (pet_movement_timer) {
        lv_timer_del(pet_movement_timer);
        pet_movement_timer = NULL;
    }
    if (care_animation_timer) {
        lv_timer_del(care_animation_timer);
        care_animation_timer = NULL;
    }

    // Reset care menu state
    care_menu_active = false;
    care_menu_selected = 0;

    // Reset pointers
    care_menu_container = NULL;
    gif_container = NULL;
    pet_image_walk = NULL;
    pet_image_walk_left = NULL;
    pet_image_blink = NULL;
    pet_image_stand = NULL;
    pet_image_sleep = NULL;
    pet_image_dance = NULL;
    pet_image_eat = NULL;
    pet_image_bath = NULL;
    pet_image_toilet = NULL;
    pet_image_sick = NULL;
    pet_image_happy = NULL;
    pet_image_angry = NULL;
    pet_image_cry = NULL;
    current_normal_image = NULL;

    // Remove from group and delete
    if (ui_pet_show_screen) {
        lv_obj_remove_event_cb(ui_pet_show_screen, keyboard_event_cb);
        lv_group_remove_obj(ui_pet_show_screen);
        printf("[PetShow] Screen deinitialized\n");
    }
}

/**
 * @brief Keyboard event callback
 */
static void keyboard_event_cb(lv_event_t *e)
{
    if (e == NULL) return;

    uint32_t key = lv_event_get_key(e);

    if (care_menu_active) {
        // Care menu navigation
        switch (key) {
        case KEY_ESC:
            printf("[PetShow] ESC pressed, closing care menu\n");
            care_menu_active = false;
            if (care_menu_container) {
                lv_obj_add_flag(care_menu_container, LV_OBJ_FLAG_HIDDEN);
            }
            update_care_selection();
            break;
        case KEY_LEFT:
            if (care_menu_selected > 0) {
                care_menu_selected--;
                update_care_selection();
            }
            break;
        case KEY_RIGHT:
            if (care_menu_selected < CARE_MENU_BUTTON_COUNT - 1) {
                care_menu_selected++;
                update_care_selection();
            }
            break;
        case KEY_ENTER:
            printf("[PetShow] Care action %d selected\n", care_menu_selected);
            perform_care_action(care_menu_selected);
            break;
        default:
            break;
        }
    } else {
        // Normal mode
        switch (key) {
        case KEY_ESC:
            printf("[PetShow] ESC pressed, returning to main screen\n");
            screen_back();
            break;
        case KEY_UP:
            // Show care menu
            printf("[PetShow] UP pressed, showing care menu\n");
            care_menu_active = true;
            care_menu_selected = 0;
            if (care_menu_container) {
                lv_obj_clear_flag(care_menu_container, LV_OBJ_FLAG_HIDDEN);
            }
            update_care_selection();
            break;
        case KEY_ENTER:
            toast_screen_show("UP: Care Menu | ESC: Exit", 2000);
            break;
        default:
            break;
        }
    }
}

/**
 * @brief Get GIF source based on state
 */
static const lv_img_dsc_t *get_gif_src_by_state(bool is_walking, int8_t direction, uint8_t idle_state)
{
    if (is_walking) {
        return (direction == 1) ? &ducky_walk : &ducky_walk_to_left;
    } else {
        return (idle_state == 0) ? &ducky_blink : &ducky_stand_still;
    }
}

/**
 * @brief Get GIF object by source
 */
static lv_obj_t *get_gif_object_by_src(const lv_img_dsc_t *gif_src)
{
    if (gif_src == &ducky_walk) return pet_image_walk;
    if (gif_src == &ducky_walk_to_left) return pet_image_walk_left;
    if (gif_src == &ducky_blink) return pet_image_blink;
    if (gif_src == &ducky_stand_still) return pet_image_stand;
    return NULL;
}

/**
 * @brief Switch pet animation
 */
static void switch_pet_animation(lv_obj_t *target_image)
{
    if (target_image == NULL) return;

    // Hide all normal animations
    lv_obj_add_flag(pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_stand, LV_OBJ_FLAG_HIDDEN);

    // Show target
    lv_obj_clear_flag(target_image, LV_OBJ_FLAG_HIDDEN);
    current_normal_image = target_image;
}

/**
 * @brief Pet animation timer callback
 */
static void pet_animation_cb(lv_timer_t *timer)
{
    if (ui_pet_show_screen == NULL || gif_container == NULL) return;

    const lv_img_dsc_t *gif_src = get_gif_src_by_state(pet_is_walking, pet_direction, idle_animation_state);
    lv_obj_t *target_image = get_gif_object_by_src(gif_src);

    if (target_image != NULL && target_image != current_normal_image) {
        switch_pet_animation(target_image);
    }
}

/**
 * @brief Pet movement timer callback
 */
static void pet_movement_cb(lv_timer_t *timer)
{
    pet_state_timer += PET_MOVEMENT_INTERVAL;

    if (!pet_is_walking) {
        idle_animation_timer += PET_MOVEMENT_INTERVAL;
        if (idle_animation_timer >= idle_animation_duration) {
            idle_animation_state = 1 - idle_animation_state;
            const lv_img_dsc_t *new_gif_src = get_gif_src_by_state(false, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
            idle_animation_timer = 0;
            idle_animation_duration = 4000 + (rand() % 8000);
        }
    }

    if (pet_state_timer >= pet_state_duration) {
        pet_is_walking = !pet_is_walking;

        if (pet_is_walking) {
            pet_direction = (rand() % 2) ? 1 : -1;
            pet_state_duration = PET_WALK_DURATION_MIN + (rand() % (PET_WALK_DURATION_MAX - PET_WALK_DURATION_MIN));
            const lv_img_dsc_t *new_gif_src = get_gif_src_by_state(true, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        } else {
            pet_state_duration = PET_IDLE_DURATION_MIN + (rand() % (PET_IDLE_DURATION_MAX - PET_IDLE_DURATION_MIN));
            idle_animation_timer = 0;
            idle_animation_duration = 4000 + (rand() % 8000);
            const lv_img_dsc_t *new_gif_src = get_gif_src_by_state(false, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        }
        pet_state_timer = 0;
    }

    if (pet_is_walking) {
        pet_x_pos += pet_direction * PET_MOVEMENT_STEP;

        if (pet_x_pos > PET_MOVEMENT_LIMIT) {
            pet_x_pos = PET_MOVEMENT_LIMIT;
            pet_direction = -1;
            const lv_img_dsc_t *new_gif_src = get_gif_src_by_state(true, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        } else if (pet_x_pos < -PET_MOVEMENT_LIMIT) {
            pet_x_pos = -PET_MOVEMENT_LIMIT;
            pet_direction = 1;
            const lv_img_dsc_t *new_gif_src = get_gif_src_by_state(true, pet_direction, idle_animation_state);
            lv_obj_t *target_image = get_gif_object_by_src(new_gif_src);
            if (target_image != NULL) {
                switch_pet_animation(target_image);
            }
        }
    }

    if (pet_x_pos != last_pet_x_pos && gif_container) {
        lv_obj_set_x(gif_container, pet_x_pos);
        last_pet_x_pos = pet_x_pos;
    }
}

/**
 * @brief Create the care menu at the bottom of the screen
 */
static void create_care_menu(void)
{
    if (ui_pet_show_screen == NULL) return;

    // Create care menu container
    care_menu_container = lv_obj_create(ui_pet_show_screen);
    lv_obj_set_size(care_menu_container, SCREEN_WIDTH, CARE_MENU_HEIGHT);
    lv_obj_align(care_menu_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(care_menu_container, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(care_menu_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(care_menu_container, 1, 0);
    lv_obj_set_style_border_color(care_menu_container, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_pad_all(care_menu_container, 5, 0);

    // Initially hidden, shown when user presses UP
    lv_obj_add_flag(care_menu_container, LV_OBJ_FLAG_HIDDEN);

    // Create care buttons (Feed, Bath, Heal, Play, Toilet)
    const char *care_names[] = {"Feed", "Bath", "Heal", "Play", "Potty"};
    const char *care_icons[] = {"[E]", "[B]", "[H]", "[P]", "[T]"};

    uint16_t button_width = (SCREEN_WIDTH - 60) / CARE_MENU_BUTTON_COUNT;
    uint16_t start_x = 10;

    for (int i = 0; i < CARE_MENU_BUTTON_COUNT; i++) {
        care_buttons[i] = lv_btn_create(care_menu_container);
        lv_obj_set_size(care_buttons[i], button_width, CARE_BUTTON_HEIGHT);
        lv_obj_align(care_buttons[i], LV_ALIGN_TOP_LEFT, start_x + i * (button_width + 3), 5);

        // Set button style
        lv_obj_set_style_bg_color(care_buttons[i], lv_color_white(), 0);
        lv_obj_set_style_bg_opa(care_buttons[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(care_buttons[i], 1, 0);
        lv_obj_set_style_border_color(care_buttons[i], lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_radius(care_buttons[i], 5, 0);
        lv_obj_set_style_text_color(care_buttons[i], lv_color_black(), 0);

        // Create label
        lv_obj_t *label = lv_label_create(care_buttons[i]);
        lv_label_set_text_fmt(label, "%s\n%s", care_icons[i], care_names[i]);
        lv_obj_set_align(label, LV_ALIGN_CENTER);
    }
}

/**
 * @brief Update care menu button selection
 */
static void update_care_selection(void)
{
    for (int i = 0; i < CARE_MENU_BUTTON_COUNT; i++) {
        if (i == care_menu_selected && care_menu_active) {
            // Selected button style
            lv_obj_set_style_bg_color(care_buttons[i], lv_color_hex(0x4CAF50), 0);
            lv_obj_set_style_border_color(care_buttons[i], lv_color_hex(0x388E3C), 0);
            lv_obj_set_style_border_width(care_buttons[i], 2, 0);
            lv_obj_set_style_text_color(care_buttons[i], lv_color_white(), 0);
        } else {
            // Normal button style
            lv_obj_set_style_bg_color(care_buttons[i], lv_color_white(), 0);
            lv_obj_set_style_border_color(care_buttons[i], lv_color_hex(0xCCCCCC), 0);
            lv_obj_set_style_border_width(care_buttons[i], 1, 0);
            lv_obj_set_style_text_color(care_buttons[i], lv_color_black(), 0);
        }
    }
}

/**
 * @brief Perform selected care action
 */
static void perform_care_action(uint8_t action_index)
{
    const char *care_names[] = {"Feed", "Bath", "Heal", "Play", "Potty"};
    lv_obj_t *animation_images[] = {pet_image_eat, pet_image_bath, pet_image_happy, pet_image_dance, pet_image_toilet};
    const char *toast_messages[] = {"Yummy! +10 Hunger", "Clean! +10 Hygiene", "Healed! +10 Health", "Happy! +10 Fun", "Done! +10 Clean"};

    printf("[PetShow] Performing care action: %s\n", care_names[action_index]);

    // Hide care menu
    care_menu_active = false;
    if (care_menu_container) {
        lv_obj_add_flag(care_menu_container, LV_OBJ_FLAG_HIDDEN);
    }

    // Show corresponding animation
    show_care_animation(animation_images[action_index], toast_messages[action_index]);
}

/**
 * @brief Show care animation
 */
static void show_care_animation(lv_obj_t *animation_image, const char *toast_msg)
{
    // Stop normal animations temporarily
    if (pet_animation_timer) {
        lv_timer_pause(pet_animation_timer);
    }
    if (pet_movement_timer) {
        lv_timer_pause(pet_movement_timer);
    }

    // Hide all normal animations
    lv_obj_add_flag(pet_image_walk, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_walk_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_blink, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pet_image_stand, LV_OBJ_FLAG_HIDDEN);

    // Show selected animation
    lv_obj_clear_flag(animation_image, LV_OBJ_FLAG_HIDDEN);
    current_normal_image = animation_image;

    // Show toast message
    toast_screen_show(toast_msg, 2000);

    // Start care animation timer (3 seconds)
    if (care_animation_timer) {
        lv_timer_del(care_animation_timer);
    }
    care_animation_timer = lv_timer_create(care_animation_cb, 3000, NULL);
}

/**
 * @brief Care animation timer callback
 */
static void care_animation_cb(lv_timer_t *timer)
{
    printf("[PetShow] Care animation complete, resuming normal animations\n");

    // Hide care animation
    if (current_normal_image && 
        current_normal_image != pet_image_walk && 
        current_normal_image != pet_image_walk_left && 
        current_normal_image != pet_image_blink && 
        current_normal_image != pet_image_stand) {
        lv_obj_add_flag(current_normal_image, LV_OBJ_FLAG_HIDDEN);
    }

    // Resume normal animations
    if (pet_animation_timer) {
        lv_timer_resume(pet_animation_timer);
    }
    if (pet_movement_timer) {
        lv_timer_resume(pet_movement_timer);
    }

    // Stop this timer
    if (care_animation_timer) {
        lv_timer_del(care_animation_timer);
        care_animation_timer = NULL;
    }

    // Restore normal animation
    switch_pet_animation(pet_image_stand);
}