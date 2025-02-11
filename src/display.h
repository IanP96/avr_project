/*
 * display.h
 *
 * Authors: Luke Kamols, Jarrod Bennett, Martin Ploschner, Cody Burnett,
 * Renee Nightingale
 */ 

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "pixel_colour.h"

#define ANIMATION_LENGTH 57
#define ANIMATION_DELAY 6

// Shows a starting display.
void show_start_screen(void);

// Update dynamic start screen based on frame number (0-11)
void update_start_screen(int8_t frame_number);

#endif /* DISPLAY_H_ */
