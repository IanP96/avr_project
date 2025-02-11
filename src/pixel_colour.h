/*
 * pixel_colour.h
 *
 * Author: Peter Sutton
 */ 

#ifndef PIXEL_COLOUR_H_
#define PIXEL_COLOUR_H_

#include <stdint.h>

// Each PixelType is an 8 bit number - 4 bits of green in the high bits,
// 4 bits of red in the low bits
typedef uint8_t PixelColour;

// Some useful colour definitions
#define COLOUR_BLACK		(0x00)
#define COLOUR_RED			(0x0F)
#define COLOUR_GREEN		(0xF0)
#define COLOUR_ORANGE		(0x3C)
#define COLOUR_YELLOW		(0xFF)

// My custom colours

// Pixel already fired at
#define COLOUR_DARK_YELLOW (0x23)
// Sunken ship
#define COLOUR_DARK_RED (0x01)

// At game over, unfired, no ship
#define COLOUR_DARK_GREEN (0x10)
// At game over, unfired, has ship
#define COLOUR_DARK_ORANGE (0x14)

#endif /* PIXEL_COLOUR_H_ */
