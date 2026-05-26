/**
 * @file pet_show_screen.h
 * @brief Pet show screen header file
 *
 * This screen displays the pet animation in full screen mode.
 */

#ifndef PET_SHOW_SCREEN_H
#define PET_SHOW_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "screen_manager.h"

extern Screen_t pet_show_screen;

void pet_show_screen_init(void);
void pet_show_screen_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* PET_SHOW_SCREEN_H */