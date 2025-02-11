/*
 * game.c
 *
 * Functionality related to the game state and features.
 *
 * Author: Jarrod Bennett, Cody Burnett
 */

#include "game.h"
#include "project.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include "display.h"
#include "ledmatrix.h"
#include "terminalio.h"
#include "timer0.h"
#include "string.h"

uint8_t human_grid[GRID_NUM_ROWS][GRID_NUM_COLUMNS];
uint8_t computer_grid[GRID_NUM_ROWS][GRID_NUM_COLUMNS];
int8_t cursor_x, cursor_y;
uint8_t cursor_on;

// 0 for non-salvo, 1 for salvo
uint8_t salvo_mode;
// Shot cap for first six turns, 1-6
uint8_t salvo_shot_limit;
// Num of shots fired for salvo mode, bomb count as one shot
uint8_t shots_fired;
// Num of shots fired, bomb counts as multiple
uint8_t cells_fired;
// Locations of shots made, locations as bytes
uint8_t shots_to_update[27];
// 1 if human turn and salvo mode. 0 otherwise
uint8_t human_salvo_mode;

// Computer ships sunk by human
uint8_t computer_ships_sunk;
// Human ships sunk by computer
uint8_t human_ships_sunk;

// 0 for normal com, 1 for search and destroy
uint8_t computer_mode;
// How many unhit spaces on human grid for computer to fire at
uint8_t com_unhit_cells_left;
// How many unhit spaces on com grid for human to fire at
uint8_t human_unhit_cells_left;
// How many cells to destroy for com, 0-4
uint8_t num_cells_to_destroy;
// Cells left to destroy, or 0xFF if invalid
uint8_t cells_to_destroy[4];

// bit 0 is bomb cheat, bit 1 is horiz cheat, bit 2 is vert cheat. 0 if unused, 1 if used
uint8_t cheats_used;

// 1 if human is in setup mode, 0 otherwise
uint8_t human_setup_mode;
// Ship being places, goes 6-1
uint8_t ship_human_placing;
// Start pos of ship as byte
uint8_t ship_setup_start;
// End pos of ship as byte
uint8_t ship_setup_end;
// Ship lengths, index 0 is ship 1, etc...
uint8_t const ship_lengths[] = {6, 4, 3, 3, 2, 2};
// Whether the current ship position in setup is valid, 1 if valid
uint8_t ship_setup_valid_pos;

/**
 * @brief Set human setup mode. 1 if human is in setup mode, 0 otherwise
 */
void set_human_setup_mode(uint8_t new_val)
{
	human_setup_mode = new_val;
}

/**
 * @brief Get human setup mode. 1 if human is in setup mode, 0 otherwise
 */
uint8_t get_human_setup_mode()
{
	return human_setup_mode;
}

// Whether the com's ships are visible using c/C cheat, 1 if visible
uint8_t cheat_visible;

/**
 * @brief Set cheat visible, 1 if visible
 */
void set_cheat_visible(uint8_t new_val)
{
	cheat_visible = new_val;
}

/**
 * @brief Get cheat visible, 1 if visible
 */
uint8_t get_cheat_visible()
{
	return cheat_visible;
}

void check_surroundings(uint8_t x, uint8_t y);

void computer_turn();
uint8_t get_pixel_colour(uint8_t cell_data);
uint8_t get_cursor_colour(uint8_t cell_data);

uint8_t const MATRIX_WIDTH = 8;
uint8_t const FIRE_MASK = (1 << 5);
uint8_t const SUNKEN_MASK = (1 << 6);
uint8_t const HIT_MASK = (1 << 7);
// Invalid moves printed when user tries to fire at same spot again
const char *INVALID_MOVE_MESSAGES[3];
// Ship names
const char *SHIP_NAMES[6];
// Invalid pos byte
uint8_t const INVALID_CELL = 0xFF;

// y-coord of next computer shot
volatile uint8_t next_com_hit_y;
// x-coord of next computer shot
volatile uint8_t next_com_hit_x;
// Number of invalid moves made by human
volatile uint8_t invalid_move_count;

/**
 * @brief Returns random int between 0 inc and max exc.
 *
 * @param max Max value exclusive.
 */
uint8_t random_int(uint8_t max)
{
	return rand() % max;
}

/**
 * @brief Returns shots left for salvo, 1 for non-salvo.
 * @param turn 0 for human turn, 1 for com turn
 */
uint8_t shots_left(uint8_t turn)
{
	if (salvo_mode)
	{
		if ((turn ? com_unhit_cells_left : human_unhit_cells_left) == 0)
		{
			// No unfired cells left
			return 0;
		}
		uint8_t max = 6 - (turn ? computer_ships_sunk : human_ships_sunk);
		if (salvo_shot_limit < max)
		{
			max = salvo_shot_limit;
		}
		return max - shots_fired;
	}
	else
	{
		return !shots_fired;
	}
}

/**
 * @brief Get y val of position byte (upper half)
 */
uint8_t get_y(uint8_t byte)
{
	return ((byte & 0xF0) >> 4) & 0x0F;
}

/**
 * @brief Get x val of position byte (lower half)
 */
uint8_t get_x(uint8_t byte)
{
	return byte & 0x0F;
}

/**
 * @brief Whether this cell has been fired at.
 * @param cell The cell to check.
 * @return uint8_t Whether this cell has been fired at.
 */
uint8_t fired_at(uint8_t cell)
{
	return cell & FIRE_MASK;
}

/**
 * @brief Return 1 if num between 0-7, 0 otherwise
 */
uint8_t valid_range(int8_t num)
{
	return (0 <= num) && (num <= 7);
}

/**
 * @brief Convert x and y vals to position byte.
 */
uint8_t convert_pos_to_byte(uint8_t x, uint8_t y)
{
	return x | (y << 4);
}

/**
 * @brief Redraw grid for human setup, checks validity (overlapping ships), updates ship start and end
 */
void redraw_human_setup(uint8_t old_start, uint8_t old_end, uint8_t new_start, uint8_t new_end)
{
	uint8_t cell;

	// Redraw old pos
	for (uint8_t x = get_x(old_start); x <= get_x(old_end); x++)
	{
		for (uint8_t y = get_y(old_start); y <= get_y(old_end); y++)
		{
			cell = human_grid[y][x];
			if (cell & SHIP_MASK)
			{
				ledmatrix_draw_pixel_in_human_grid(x, y, COLOUR_ORANGE);
			}
			else
			{
				ledmatrix_draw_pixel_in_human_grid(x, y, COLOUR_BLACK);
			}
		}
	}

	// Redraw new pos, check if valid
	ship_setup_valid_pos = 1;
	for (uint8_t x = get_x(new_start); x <= get_x(new_end); x++)
	{
		for (uint8_t y = get_y(new_start); y <= get_y(new_end); y++)
		{
			cell = human_grid[y][x];
			if (cell & SHIP_MASK)
			{
				// Ship overlap
				ship_setup_valid_pos = 0;
				ledmatrix_draw_pixel_in_human_grid(x, y, COLOUR_RED);
			}
			else
			{
				// Not overlapping
				ledmatrix_draw_pixel_in_human_grid(x, y, COLOUR_GREEN);
			}
		}
	}

	// Update ship setup position
	ship_setup_start = new_start;
	ship_setup_end = new_end;
}

/**
 * @brief Initialise human ship setup
 */
void initialise_human_setup()
{
	ship_human_placing = 1;
	ship_setup_start = convert_pos_to_byte(0, 0);
	ship_setup_end = convert_pos_to_byte(0, ship_lengths[ship_human_placing - 1] - 1);
	ship_setup_valid_pos = 1;
	for (uint8_t x = 0; x < 8; x++)
	{
		for (uint8_t y = 0; y < 8; y++)
		{
			human_grid[y][x] = SEA;
		}
	}
	redraw_human_setup(ship_setup_start, ship_setup_end, ship_setup_start, ship_setup_end);
}

/**
 * @brief Move human ship during setup. Recolours if valid.
 */
void move_human_ship(int8_t dx, int8_t dy)
{
	int8_t new_start_x = get_x(ship_setup_start) + dx,
		   new_start_y = get_y(ship_setup_start) + dy,
		   new_end_x = get_x(ship_setup_end) + dx,
		   new_end_y = get_y(ship_setup_end) + dy;
	if (valid_range(new_start_x) && valid_range(new_start_y) && valid_range(new_end_x) && valid_range(new_end_y))
	{
		redraw_human_setup(ship_setup_start, ship_setup_end, convert_pos_to_byte(new_start_x, new_start_y), convert_pos_to_byte(new_end_x, new_end_y));
	}
}

/**
 * @brief Rotate ship when r is pressed
 */
void rotate_human_ship()
{
	uint8_t horizontal = (get_y(ship_setup_start) == get_y(ship_setup_end));
	uint8_t length_delta = ship_lengths[ship_human_placing - 1] - 1;
	if (horizontal)
	{
		// Was horizontal, make vertical
		uint8_t new_end_x = get_x(ship_setup_start), new_end_y = get_y(ship_setup_start) + length_delta;
		uint8_t new_start_y = get_y(ship_setup_start);
		if (new_end_y > 7)
		{
			// Over board edge
			uint8_t diff = new_end_y - 7;
			new_end_y -= diff;
			new_start_y -= diff;
		}
		redraw_human_setup(ship_setup_start, ship_setup_end, convert_pos_to_byte(new_end_x, new_start_y), convert_pos_to_byte(new_end_x, new_end_y));
	}
	else
	{
		// Was vertical, make horizontal
		uint8_t new_end_x = get_x(ship_setup_start) + length_delta, new_end_y = get_y(ship_setup_start);
		uint8_t new_start_x = get_x(ship_setup_start);
		if (new_end_x > 7)
		{
			// Over board edge
			uint8_t diff = new_end_x - 7;
			new_end_x -= diff;
			new_start_x -= diff;
		}
		redraw_human_setup(ship_setup_start, ship_setup_end, convert_pos_to_byte(new_start_x, new_end_y), convert_pos_to_byte(new_end_x, new_end_y));
	}
}

/**
 * @brief Place human ship during setup, if valid
 */
void place_human_ship()
{
	uint8_t horizontal = (get_y(ship_setup_start) == get_y(ship_setup_end));
	if (ship_setup_valid_pos)
	{
		// Colour orange, update grid array
		for (uint8_t x = get_x(ship_setup_start); x <= get_x(ship_setup_end); x++)
		{
			for (uint8_t y = get_y(ship_setup_start); y <= get_y(ship_setup_end); y++)
			{
				uint8_t is_end = ((x == get_x(ship_setup_start) && y == get_y(ship_setup_start)) ||
								  (x == get_x(ship_setup_end) && y == get_y(ship_setup_end)));
				human_grid[y][x] = ship_human_placing | (is_end ? SHIP_END : 0) | (horizontal ? HORIZONTAL : 0);
				ledmatrix_draw_pixel_in_human_grid(x, y, COLOUR_ORANGE);
			}
		}

		// Update vars
		if (ship_human_placing == 6)
		{
			// Exit setup mode, no more ships
			set_human_setup_mode(0);
		}
		else
		{
			ship_human_placing++;
			ship_setup_start = convert_pos_to_byte(0, 0);
			ship_setup_end = convert_pos_to_byte(0, ship_lengths[ship_human_placing - 1] - 1);
			ship_setup_valid_pos = 1;
			redraw_human_setup(ship_setup_start, ship_setup_end, ship_setup_start, ship_setup_end);
		}
	}
}

/**
 * @brief Draw human grid orange on matrix
 */
void draw_human_grid()
{
	for (uint8_t i = 0; i < GRID_NUM_COLUMNS; i++)
	{
		for (uint8_t j = 0; j < GRID_NUM_COLUMNS; j++)
		{
			if (human_grid[j][i] & SHIP_MASK)
			{
				ledmatrix_draw_pixel_in_human_grid(i, j, COLOUR_ORANGE);
			}
		}
	}
}

/**
 * @brief Randomise the com grid
 */
void random_com_grid()
{
	// Make everything sea
	for (uint8_t x = 0; x < 8; x++)
	{
		for (uint8_t y = 0; y < 8; y++)
		{
			computer_grid[y][x] = SEA;
		}
	}

	uint8_t length_delta;
	uint8_t const VERTICAL_MASK = (1 << 3);
	uint8_t x1, y1, x2, y2;
	uint8_t valid_positions[120];
	uint8_t num_valid_positions;

	uint8_t valid = 1;
	uint8_t pos_byte;

	// Biggest to smallest ship
	for (uint8_t ship = 1; ship <= 6; ship++)
	{
		// Find valid positions
		length_delta = ship_lengths[ship - 1] - 1;
		num_valid_positions = 0;
		for (uint8_t is_vertical = 0; is_vertical < 2; is_vertical++)
		{
			for (x1 = 0; x1 < 8; x1++)
			{
				for (y1 = 0; y1 < 8; y1++)
				{
					// Check that ship fits on grid
					valid = 1;
					x2 = x1 + (is_vertical ? 0 : length_delta);
					y2 = y1 + (is_vertical ? length_delta : 0);
					if (valid_range(x2) && valid_range(y2))
					{
						// Check for no overlap
						for (uint8_t x = x1; x <= x2; x++)
						{
							for (uint8_t y = y1; y <= y2; y++)
							{
								if (computer_grid[y][x] && SHIP_MASK)
								{
									// Already has ship
									valid = 0;
									break;
								}
							}
							if (!valid)
							{
								break;
							}
						}
					} else
					{
						valid = 0;
					}
					
					if (valid)
					{
						valid_positions[num_valid_positions] = (convert_pos_to_byte(x1, y1) | (is_vertical ? VERTICAL_MASK : 0));
						num_valid_positions++;
					}
				}
			}
		}

		// Choose random valid position
		pos_byte = valid_positions[random_int(num_valid_positions)];
		uint8_t is_vertical = (pos_byte & VERTICAL_MASK);
		pos_byte &= (~VERTICAL_MASK); // Clear vertical bit
		x1 = get_x(pos_byte);
		y1 = get_y(pos_byte);
		x2 = x1 + (is_vertical ? 0 : length_delta);
		y2 = y1 + (is_vertical ? length_delta : 0);
		uint8_t is_end;
		for (uint8_t x = x1; x <= x2; x++)
		{
			for (uint8_t y = y1; y <= y2; y++)
			{
				is_end = ((x == x1 && y == y1) ||
								  (x == x2 && y == y2));
				computer_grid[y][x] = (ship | (is_end ? SHIP_END : 0) | (is_vertical ? 0 : HORIZONTAL));
			}
		}
	}
}

// Initialise the game by resetting the grid and beat
void initialise_game(void)
{
	// clear the splash screen art
	ledmatrix_clear();

	if (!get_human_setup_mode())
	{
		// Default setup for human and com

		// see "Human Turn" feature for how ships are encoded
		// fill in the grid with the ships
		uint8_t initial_human_grid[GRID_NUM_ROWS][GRID_NUM_COLUMNS] =
			{{SEA, SEA, SEA, SEA, SEA, SEA, SEA, SEA},
			 {SEA, CARRIER | HORIZONTAL | SHIP_END, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL | SHIP_END, SEA},
			 {SEA, SEA, SEA, SEA, SEA, SEA, SEA, SEA},
			 {SEA, SEA, CORVETTE | SHIP_END, SEA, SEA, SUBMARINE | SHIP_END, SEA, SEA},
			 {DESTROYER | SHIP_END, SEA, CORVETTE | SHIP_END, SEA, SEA, SUBMARINE | SHIP_END, SEA, FRIGATE | SHIP_END},
			 {DESTROYER, SEA, SEA, SEA, SEA, SEA, SEA, FRIGATE},
			 {DESTROYER | SHIP_END, SEA, CRUISER | HORIZONTAL | SHIP_END, CRUISER | HORIZONTAL, CRUISER | HORIZONTAL, CRUISER | HORIZONTAL | SHIP_END, SEA, FRIGATE | SHIP_END},
			 {SEA, SEA, SEA, SEA, SEA, SEA, SEA, SEA}};
		uint8_t initial_computer_grid[GRID_NUM_ROWS][GRID_NUM_COLUMNS] =
			{{SEA, SEA, SEA, SEA, SEA, SEA, SEA, SEA},
			 {DESTROYER | SHIP_END, SEA, CRUISER | HORIZONTAL | SHIP_END, CRUISER | HORIZONTAL, CRUISER | HORIZONTAL, CRUISER | HORIZONTAL | SHIP_END, SEA, FRIGATE | SHIP_END},
			 {DESTROYER, SEA, SEA, SEA, SEA, SEA, SEA, FRIGATE},
			 {DESTROYER | SHIP_END, SEA, CORVETTE | SHIP_END, SEA, SEA, SUBMARINE | SHIP_END, SEA, FRIGATE | SHIP_END},
			 {SEA, SEA, CORVETTE | SHIP_END, SEA, SEA, SUBMARINE | SHIP_END, SEA, SEA},
			 {SEA, SEA, SEA, SEA, SEA, SEA, SEA, SEA},
			 {SEA, CARRIER | HORIZONTAL | SHIP_END, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL, CARRIER | HORIZONTAL | SHIP_END, SEA},
			 {SEA, SEA, SEA, SEA, SEA, SEA, SEA, SEA}};

		for (uint8_t i = 0; i < GRID_NUM_COLUMNS; i++)
		{
			for (uint8_t j = 0; j < GRID_NUM_COLUMNS; j++)
			{
				human_grid[j][i] = initial_human_grid[j][i];
				computer_grid[j][i] = initial_computer_grid[j][i];
			}
		}
	}
	else
	{
		// Human grid setup happens later
		for (uint8_t i = 0; i < GRID_NUM_COLUMNS; i++)
		{
			for (uint8_t j = 0; j < GRID_NUM_COLUMNS; j++)
			{
				human_grid[j][i] = SEA;
			}
		}
		// Com grid random setup
		random_com_grid();
	}

	// Initialise cursor state
	cursor_x = 3;
	cursor_y = 3;
	cursor_on = 1;

	// Initialise location of next computer hit
	next_com_hit_y = 7;
	next_com_hit_x = 0;

	computer_ships_sunk = 0;
	human_ships_sunk = 0;

	invalid_move_count = 0;

	cheats_used = 0;

	set_cheat_visible(0);

	salvo_shot_limit = 1;
	human_salvo_mode = 0;
	shots_fired = 0;
	cells_fired = 0;

	INVALID_MOVE_MESSAGES[0] = "Invalid move, try again.";
	INVALID_MOVE_MESSAGES[1] = "Invalid move. TRY AGAIN.";
	INVALID_MOVE_MESSAGES[2] = "INVALID MOVE GRRRRRRRRRR";

	SHIP_NAMES[0] = "Carrier";
	SHIP_NAMES[1] = "Cruiser";
	SHIP_NAMES[2] = "Destroyer";
	SHIP_NAMES[3] = "Frigate";
	SHIP_NAMES[4] = "Corvette";
	SHIP_NAMES[5] = "Submarine";

	com_unhit_cells_left = 64;
	num_cells_to_destroy = 0;
	human_unhit_cells_left = 64;
}

/**
 * @brief Checks for sunken ships, outputs message to terminal, colours sunken ships on matrix.
 * @param turn 0 for human turn, 1 for computer turn.
 * @param cell_just_hit The cell that has just been hit.
 */
void check_for_sunken(uint8_t turn, uint8_t cell_just_hit)
{
	uint8_t ship = cell_just_hit & SHIP_MASK;
	// Whether an unhit ship of the given ship type has been found
	uint8_t unhit_found = 0;

	for (uint8_t i = 0; i < 8; i++)
	{
		for (uint8_t j = 0; j < 8; j++)
		{
			uint8_t cell_at_pos = turn ? human_grid[i][j] : computer_grid[i][j];
			if (
				((cell_at_pos & SHIP_MASK) == ship) // Same ship
				&& !(cell_at_pos & HIT_MASK)		// Unfired
			)
			{
				unhit_found = 1;
				break;
			}
		}
		if (unhit_found)
		{
			break;
		}
	}
	if (!unhit_found)
	{
		// New sunken ship
		if (turn)
		{
			// Computer turn, I sunk your
			move_terminal_cursor(0, ++human_ships_sunk + 1);
			printf("I Sunk Your %s", SHIP_NAMES[ship - 1]);
			for (uint8_t i = 0; i < 8; i++)
			{
				for (uint8_t j = 0; j < 8; j++)
				{
					if ((human_grid[i][j] & SHIP_MASK) == ship)
					{
						human_grid[i][j] |= SUNKEN_MASK;
						ledmatrix_draw_pixel_in_human_grid(j, i, COLOUR_DARK_RED);
					}
				}
			}
		}
		else
		{
			// Human turn, You Sunk My
			move_terminal_cursor(40 - strlen(SHIP_NAMES[ship - 1]), ++computer_ships_sunk + 1);
			printf("You Sunk My %s", SHIP_NAMES[ship - 1]);
			for (uint8_t i = 0; i < 8; i++)
			{
				for (uint8_t j = 0; j < 8; j++)
				{
					if ((computer_grid[i][j] & SHIP_MASK) == ship)
					{
						computer_grid[i][j] |= SUNKEN_MASK;
						ledmatrix_draw_pixel_in_computer_grid(j, i, COLOUR_DARK_RED);
					}
				}
			}
		}
	}
}

/**
 * @brief Fire at x, y. For human turn, also clear invalid move msg and count. Assumes move is valid. Fires blindly (does not update hit bit). Does not update matrix. Updates cell fired count, not shot count.
 * @param turn 0 for human turn, 1 for com turn
 */
void fire(uint8_t turn, uint8_t x, uint8_t y)
{
	uint8_t cell, not_already_fired_at = 0;
	if (turn)
	{
		// Com turn
		cell = human_grid[y][x];
		if (!fired_at(cell))
		{
			not_already_fired_at = 1;
			human_grid[y][x] |= FIRE_MASK;
			com_unhit_cells_left--;
		}
	}
	else
	{
		// Human turn

		// Clear invalid move msg
		move_terminal_cursor(0, 0);
		clear_to_end_of_line();

		invalid_move_count = 0;

		cell = computer_grid[y][x];
		if (!fired_at(cell))
		{
			not_already_fired_at = 1;
			computer_grid[y][x] |= FIRE_MASK;
			human_unhit_cells_left--;
			
		}
	}
	if (not_already_fired_at)
	{
		shots_to_update[cells_fired] = convert_pos_to_byte(x, y);
		cells_fired++;
		if (!turn)
		{
			// Update cursor on matrix if needed
			if ((x == cursor_x) && (y == cursor_y))
			{
				cursor_on = !cursor_on;
				flash_cursor();
			}
			else
			{
				ledmatrix_draw_pixel_in_computer_grid(x, y, COLOUR_DARK_GREEN);
			}
		}
	}
}

/**
 * @brief Completes turn by updating matrix and resetting shot counts.
 * @param turn 0 for human turn, 1 for com turn
 */
void complete_turn(uint8_t turn)
{
	human_salvo_mode = 0;

	// Update matrix
	uint8_t ship_data;
	uint8_t pos_byte, x, y;

	for (uint8_t i = 0; i < cells_fired; i++)
	{
		pos_byte = shots_to_update[i];
		x = get_x(pos_byte);
		y = get_y(pos_byte);

		if (turn)
		{
			// Com turn
			human_grid[y][x] |= HIT_MASK;
			ship_data = human_grid[y][x];
			ledmatrix_draw_pixel_in_human_grid(
				x, y, get_pixel_colour(ship_data));
		}
		else
		{
			// Human turn
			computer_grid[y][x] |= HIT_MASK;
			ship_data = computer_grid[y][x];
			// Update cursor on matrix if needed
			if ((x == cursor_x) && (y == cursor_y))
			{
				cursor_on = !cursor_on;
				flash_cursor();
			}
			else
			{
				ledmatrix_draw_pixel_in_computer_grid(
					x, y, get_pixel_colour(ship_data));
			}
		}
		check_for_sunken(turn, ship_data);
	}

	// If com turn finished and in salvo mode, enter human salvo mode
	human_salvo_mode = (turn == 1 && salvo_mode);

	// Reset counts
	shots_fired = 0;
	cells_fired = 0;
	if (salvo_shot_limit < 6)
	{
		salvo_shot_limit++;
	}
}

/**
 * @brief Print invalid move msg and increase invalid move count accordingly
 */
void invalid_move_msg()
{
	move_terminal_cursor(0, 1);
	clear_to_end_of_line();
	printf("%s", INVALID_MOVE_MESSAGES[invalid_move_count]);
	if (invalid_move_count < 2)
	{
		invalid_move_count++;
	}
}

/**
 * @brief Play human's turn, normal shot at one cell.
 * @return 1 if valid move, 0 if invalid.
 */
uint8_t human_turn()
{
	uint8_t fired = computer_grid[cursor_y][cursor_x] & FIRE_MASK;
	uint8_t valid = 1;
	if (fired)
	{
		// Invalid move
		invalid_move_msg();
		valid = 0;
	}
	else
	{
		// Valid move
		fire(0, cursor_x, cursor_y);
		shots_fired++;
	}

	return valid;
}

/**
 * @brief Update all matrix cells with a ship, used for cheats
 */
void show_cheat()
{
	uint8_t cell;
	for (uint8_t x = 0; x < 8; x++)
	{
		for (uint8_t y = 0; y < 8; y++)
		{
			cell = computer_grid[y][x];
			if (cell & SHIP_MASK)
			{
				ledmatrix_draw_pixel_in_computer_grid(x, y, get_pixel_colour(cell));
			}
		}
	}
	cursor_on = !cursor_on;
	flash_cursor();
}

/**
 * @brief Use bomb cheat.
 * @return uint8_t 1 if valid move (bomb not already used), 0 if invalid.
 */
uint8_t bomb_cheat()
{
	if (cheats_used & (1 << 0))
	{
		invalid_move_msg();
		return 0;
	}

	// Fire at cursor
	fire(0, cursor_x, cursor_y);

	// Fire at surrounds
	int8_t dx, dy, new_x, new_y;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (1 <= i && i <= 3)
		{
			dx = 1;
		}
		else if (5 <= i && i <= 7)
		{
			dx = -1;
		}
		else
		{
			dx = 0;
		}
		if (i == 0 || i == 1 || i == 7)
		{
			dy = 1;
		}
		else if (3 <= i && i <= 5)
		{
			dy = -1;
		}
		else
		{
			dy = 0;
		}

		new_x = cursor_x + dx;
		new_y = cursor_y + dy;
		if (!(new_x < 0 || 7 < new_x || new_y < 0 || 7 < new_y))
		{
			// Location is in board
			fire(0, new_x, new_y);
		}
	}

	shots_fired++;

	// Cheat has been used
	cheats_used |= (1 << 0);
	return 1;
}

/**
 * @brief Use horizontal cheat.
 * @return uint8_t 1 if valid move (cheat not already used), 0 if invalid.
 */
uint8_t horizontal_cheat()
{
	if (cheats_used & (1 << 1))
	{
		invalid_move_msg();
		return 0;
	}

	// Fire at cursor
	fire(0, cursor_x, cursor_y);

	// Fire at surrounds
	for (uint8_t x = 0; x < 8; x++)
	{
		if (x == cursor_x)
		{
			// Already fired at cursor position, skip
			continue;
		}
		fire(0, x, cursor_y);
	}

	shots_fired++;

	// Cheat has been used
	cheats_used |= (1 << 1);
	return 1;
}

/**
 * @brief Use vertical cheat.
 * @return uint8_t 1 if valid move (cheat not already used), 0 if invalid.
 */
uint8_t vertical_cheat()
{
	if (cheats_used & (1 << 2))
	{
		invalid_move_msg();
		return 0;
	}

	// Fire at cursor
	fire(0, cursor_x, cursor_y);

	// Fire at surrounds
	for (uint8_t y = 0; y < 8; y++)
	{
		if (y == cursor_y)
		{
			// Already fired at cursor position, skip
			continue;
		}
		fire(0, cursor_x, y);
	}

	shots_fired++;

	// Cheat has been used
	cheats_used |= (1 << 2);
	return 1;
}

/**
 * @brief Fires at a random unfired cell
 */
void com_search()
{
	uint8_t target_num = random_int(com_unhit_cells_left);
	int8_t count = -1;
	uint8_t target_x, target_y;
	uint8_t cell;
	for (target_x = 0; target_x < 8; target_x++)
	{
		for (target_y = 0; target_y < 8; target_y++)
		{
			cell = human_grid[target_y][target_x];
			if (!fired_at(cell))
			{
				count++;
				if (count == target_num)
				{
					break;
				}
			}
		}
		if (count == target_num)
		{
			break;
		}
	}

	fire(1, target_x, target_y);

	// if (human_grid[target_y][target_x] & SHIP_MASK)
	// {
	// 	check_surroundings(target_x, target_y);
	// }
}

// Updates num_cells_to_destroy based on unfired cells in the surrounds
void check_surroundings(uint8_t x, uint8_t y)
{
	int8_t dx, dy, new_x, new_y;
	uint8_t cell;
	for (uint8_t i = 0; i < 4; i++)
	{
		dx = (i == 1 ? 1 : (i == 3 ? -1 : 0));
		dy = (i == 0 ? 1 : (i == 2 ? -1 : 0));
		new_x = x + dx;
		new_y = y + dy;
		if (new_x < 0 || 7 < new_x || new_y < 0 || 7 < new_y)
		{
			// Invalid
			cells_to_destroy[i] = INVALID_CELL;
		}
		else
		{
			cell = human_grid[new_y][new_x];
			if (!fired_at(cell))
			{
				num_cells_to_destroy++;
				cells_to_destroy[i] = convert_pos_to_byte(new_x, new_y);
			}
			else
			{
				cells_to_destroy[i] = INVALID_CELL;
			}
		}
	}
}

void destroy()
{
	int8_t count = -1, target_num = 0;
	uint8_t count_to_fire_at = random_int(num_cells_to_destroy);
	uint8_t cell, pos_byte, x, y;
	for (target_num = 0; target_num < 4; target_num++)
	{
		pos_byte = cells_to_destroy[target_num];
		if (pos_byte == INVALID_CELL)
		{
			continue;
		}
		x = get_x(pos_byte);
		y = get_y(pos_byte);
		cell = human_grid[y][x];
		if (!fired_at(cell))
		{
			count++;
			if (count == count_to_fire_at)
			{
				break;
			}
		}
	}
	fire(1, x, y);
	num_cells_to_destroy--;
}

void computer_turn()
{
	/*
	// How many unhit spaces on human grid for computer to fire at
	uint8_t unhit_cells_left;
	// How many cells to destroy for com, 0-4
	uint8_t num_cells_to_destroy;
	// Cells left to destroy, or 0xFF if invalid
	uint8_t cells_to_destroy[4];
	*/
	if (computer_mode)
	{
		// Search and destroy
		if (num_cells_to_destroy)
		{
			destroy();
		}
		else
		{
			// Look for new places to destroy
			uint8_t place_to_destroy_found = 0;
			uint8_t cell;
			for (uint8_t x = 0; x < 8; x++)
			{
				for (uint8_t y = 0; y < 8; y++)
				{
					cell = human_grid[y][x];
					if ((cell & SHIP_MASK) && (cell & HIT_MASK))
					{
						check_surroundings(x, y);
						if (num_cells_to_destroy)
						{
							destroy();
							place_to_destroy_found = 1;
							break;
						}
					}
				}
				if (place_to_destroy_found)
				{
					break;
				}
			}
			if (!place_to_destroy_found)
			{
				// No places to destroy found, fire randomly
				com_search();
			}
		}
	}
	else
	{
		fire(1, next_com_hit_x, next_com_hit_y);

		// Update location of next computer fire
		next_com_hit_x++;
		if (next_com_hit_x == 8)
		{
			// Go to next row
			next_com_hit_x = 0;
			next_com_hit_y--;
		}
		if (next_com_hit_y == 255)
		{
			// Back to top row
			// IS THIS MEANT TO HAPPEN?
			next_com_hit_y = 7;
		}
	}
	shots_fired++;
}

// Gets pixel colour based on whether there is a ship and whether a shot has been fired at that position,
// not used for cursor colour
uint8_t get_pixel_colour(uint8_t cell_data)
{
	// Whether there is a ship at the current location
	uint8_t has_ship = cell_data & SHIP_MASK;
	// Whether the current location has been fired at
	uint8_t hit = cell_data & HIT_MASK;
	uint8_t fired = cell_data & FIRE_MASK;
	// Whether the current location is sunken
	uint8_t sunken = cell_data & SUNKEN_MASK;

	if (get_cheat_visible() && has_ship)
	{
		if (sunken)
		{
			return COLOUR_DARK_RED;
		}
		else if (fired)
		{
			return COLOUR_RED;
		}
		else
		{
			return COLOUR_ORANGE;
		}
	}

	if (sunken)
	{
		// Sunken
		return COLOUR_DARK_RED;
	}
	else if (has_ship && hit)
	{
		// Hit ship
		return COLOUR_RED;
	}
	else if (hit)
	{
		// Hit but no ship
		return COLOUR_GREEN;
	}
	else if (fired && human_salvo_mode)
	{
		return COLOUR_DARK_GREEN;
	}
	else
	{
		// Nothing, just sea
		return COLOUR_BLACK;
	}
}

// Gets pixel colour for cursor
uint8_t get_cursor_colour(uint8_t cell_data)
{
	// Whether the current location has been fired at
	uint8_t fired = cell_data & FIRE_MASK;
	if (fired)
	{
		return COLOUR_DARK_YELLOW;
	}
	else
	{
		return COLOUR_YELLOW;
	}
}

void flash_cursor(void)
{
	cursor_on = 1 - cursor_on;

	// Ship info at current location
	uint8_t ship_data = computer_grid[cursor_y][cursor_x];

	// Cursor on, show yellow/dark yellow
	if (cursor_on)
	{
		ledmatrix_draw_pixel_in_computer_grid(cursor_x, cursor_y, get_cursor_colour(ship_data));
		return;
	}

	// Cursor off, see if it should be dark green
	if (human_salvo_mode)
	{
		uint8_t pos_byte, x, y;
		for (uint8_t i = 0; i < cells_fired; i++)
		{
			pos_byte = shots_to_update[i];
			x = get_x(pos_byte);
			y = get_y(pos_byte);
			if ((x == cursor_x) && (y == cursor_y)/* && !(cell & SHIP_MASK)*/)
			{
				// Colour dark green
				ledmatrix_draw_pixel_in_computer_grid(cursor_x, cursor_y, COLOUR_DARK_GREEN);
				return;
			}
		}
	}

	// Cursor off, normal colour
	ledmatrix_draw_pixel_in_computer_grid(
		cursor_x, cursor_y, get_pixel_colour(ship_data));
}

// moves the position of the cursor by (dx, dy) such that if the cursor
// started at (cursor_x, cursor_y) then after this function is called,
// it should end at ( (cursor_x + dx) % WIDTH, (cursor_y + dy) % HEIGHT)
// the cursor should be displayed after it is moved as well
void move_cursor(int8_t dx, int8_t dy)
{
	// YOUR CODE HERE
	/*suggestions for implementation:
	 * 1: remove the display of the cursor at the current location
	 *		(and replace it with whatever piece is at that location)
	 * 2: update the positional knowledge of the cursor, this will include
	 *		variables cursor_x, cursor_y and cursor_visible. Make sure you
	 *		consider what should happen if the cursor moves off the board.
	 * 3: display the cursor at the new location
	 * 4: reset the cursor flashing cycle. See project.c for how the cursor
	 *		is flashed.
	 */

	cursor_on = 1;
	flash_cursor(); // Set cursor_on to 0, replace initial cursor position

	// Update positional knowledge of cursor
	cursor_x += dx;
	if (cursor_x == 8)
	{
		cursor_x = 0;
	}
	else if (cursor_x == -1)
	{
		cursor_x = 7;
	}
	cursor_y += dy;
	if (cursor_y == 8)
	{
		cursor_y = 0;
	}
	else if (cursor_y == -1)
	{
		cursor_y = 7;
	}

	// Flash new cursor
	cursor_on = 0;
	flash_cursor();						  // Set cursor_on to 1, flash cursor
	last_flash_time = get_current_time(); // Reset flashing cycle
}

// Returns 1 if the human won, 2 if the computer won, 0 otherwise.
uint8_t is_game_over(void)
{
	for (uint8_t player = 1; player < 3; player++)
	{
		uint8_t unsunken_found = 0;
		for (uint8_t i = 0; i < 7; i++)
		{
			for (uint8_t j = 0; j < 8; j++)
			{
				uint8_t cell = (player == 1) ? computer_grid[i][j] : human_grid[i][j];
				if (
					(cell & SHIP_MASK)		 // has a ship
					&& !(cell & SUNKEN_MASK) // ship is unsunken
				)
				{
					unsunken_found = 1;
					break;
				}
			}
			if (unsunken_found)
			{
				break;
			}
		}
		if (!unsunken_found)
		{
			// Player has won
			return player;
		}
	}
	return 0;
}

/**
 * @brief Calculate high score and print to terminal
 */
void show_high_score() {

	uint16_t high_score, ship_score, accuracy_score = 0;
	uint8_t num_unfired_cells[] = {0, 0, 0, 0, 0, 0};
	uint8_t cell, ship;

	for (uint8_t x = 0; x < 8; x++)
	{
		for (uint8_t y = 0; y < 8; y++)
		{
			cell = human_grid[y][x];
			ship = (cell & SHIP_MASK);
			if (ship && !fired_at(cell))
			{
				num_unfired_cells[ship - 1]++;
			}
			if (!fired_at(computer_grid[y][x]))
			{
				accuracy_score++;
			}
		}
	}
	ship_score = 0;
	for (uint8_t i = 0; i < 6; i++)
	{
		ship_score += num_unfired_cells[i] * num_unfired_cells[i];
	}

	high_score = ship_score * accuracy_score;

	move_terminal_cursor(0, 16);
	clear_to_end_of_line();
	printf("Your score: %i", high_score);
}

// Colour LED matrix for game over
void game_over_matrix()
{
	for (uint8_t player = 0; player < 2; player++)
	{
		for (uint8_t i = 0; i < 8; i++)
		{
			for (uint8_t j = 0; j < 8; j++)
			{
				uint8_t cell = player ? human_grid[i][j] : computer_grid[i][j];
				if (!(cell & FIRE_MASK))
				{
					// Has not been fired at
					uint8_t colour = (cell & SHIP_MASK) ? COLOUR_DARK_ORANGE : COLOUR_DARK_GREEN;
					if (player)
					{
						ledmatrix_draw_pixel_in_human_grid(j, i, colour);
					}
					else
					{
						ledmatrix_draw_pixel_in_computer_grid(j, i, colour);
					}
				}
			}
		}
	}
	// Fix matrix colour at cursor to hide cursor yellow
	ledmatrix_draw_pixel_in_computer_grid(
		cursor_x, cursor_y,
		get_pixel_colour(computer_grid[cursor_y][cursor_x]));
}