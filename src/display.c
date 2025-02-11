/*
 * display.c
 *
 * Authors: Luke Kamols, Jarrod Bennett, Martin Ploschner, Cody Burnett,
 * Renee Nightingale
 */ 

#include "display.h"
#include <stdio.h>
#include <avr/io.h>
#include "pixel_colour.h"
#include "ledmatrix.h"
#include "game.h"

#define ICON_OFFSET 42
#define ICON_LENGTH 13


// constant value used to display 'BATTLESHIP ??' on launch
static const uint8_t ship_main[ANIMATION_LENGTH] = 
		{0xfe,0x92,0x92,0x6c,0x00,0x0c,0x52,0x52,0x3c,0x02,0x20,0x7e,
		 0x20,0x20,0x7e,0x20,0x00,0xfe,0x00,0x3c,0x52,0x52,0x34,0x00,
		 0x12,0x2a,0x2a,0x24,0x00,0xfe,0x20,0x20,0x1e,0x00,0x5e,0x00,
		 0x3f,0x24,0x24,0x18,0x00,0x00,0x10,0x1e,0x2b,0x39,0x0d,0x39,
		 0x29,0x39,0x49,0x49,0xf9,0x4b,0x3e,0x00,0x00};
static const uint8_t ship_highlight[ICON_LENGTH] =
		{0x00, 0x00, 0x16, 0x06, 0x02, 0x06, 0x06, 0x06, 0x36, 0x36, 0x06, 0x36, 0x00};

void show_start_screen(void)
{
	MatrixColumn column_colour_data;
	uint8_t col_data;
		
	ledmatrix_clear(); // start by clearing the LED matrix
	for (uint8_t col = 0; col < MATRIX_NUM_COLUMNS; col++)
	{
		col_data = ship_main[col];
		for(uint8_t row = 0; row < MATRIX_NUM_ROWS; row++)
		{
			// If the relevant font bit is set, we make this a coloured pixel, else blank
			if(col_data>>row & 1)
			{
				column_colour_data[row] = COLOUR_GREEN;
			}
			else
			{
				column_colour_data[row] = COLOUR_BLACK;
			}
		}
		ledmatrix_update_column(col, column_colour_data);
	}
}

// Update dynamic start screen based on the frame number (0-31)
// Note: this is hardcoded to PONG game.
// Purposefully obfuscated so functionality cannot be copied for movement tasks
void update_start_screen(int8_t frame_number)
{
	// negative numbers are used to pause the animation at the loop point
	if (frame_number < 0)
	{
		return;
	}
	ledmatrix_shift_display_left();
	
	// fill in the rightmost column
	MatrixColumn column_colour_data;
	uint8_t col_data = ship_main[(frame_number+MATRIX_NUM_COLUMNS-1)%ANIMATION_LENGTH];
	for(uint8_t row = 0; row < MATRIX_NUM_ROWS; row++)
	{
		// If the relevant font bit is set, we make this a coloured pixel, else blank
		if(col_data>>row & 1)
		{
			// text is green, ship outline is red
			column_colour_data[row] = (frame_number+MATRIX_NUM_COLUMNS-1)%ANIMATION_LENGTH < ICON_OFFSET ? COLOUR_GREEN : COLOUR_RED;
		}
		// because there's only 13 columns with the ship icon, it's more efficient to only store those thirteen columns for the
		// yellow, but then there needs to be a bunch of maths to account for this offset
		else  if (frame_number+MATRIX_NUM_COLUMNS-1 >= ICON_OFFSET && frame_number+MATRIX_NUM_COLUMNS-1 < ICON_OFFSET+ICON_LENGTH
					&& ship_highlight[frame_number+MATRIX_NUM_COLUMNS-1-ICON_OFFSET]>>row & 1)
		{
			// ship internal is yellow
			column_colour_data[row] = COLOUR_YELLOW;
		}
		else
		{
			column_colour_data[row] = COLOUR_BLACK;
		}
	}
	ledmatrix_update_column(MATRIX_NUM_COLUMNS-1, column_colour_data);
}
