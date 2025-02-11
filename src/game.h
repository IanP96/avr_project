/*
 * game.h
 *
 * Author: Jarrod Bennett, Cody Burnett
 *
 * Function prototypes for game functions available externally. You may wish
 * to add extra function prototypes here to make other functions available to
 * other files.
 */

#ifndef GAME_H_
#define GAME_H_

#include <stdint.h>

// Initialise the game by resetting the grid and beat
void initialise_game(void);

// flash the cursor
void flash_cursor(void);

// move the cursor in the x and/or y direction
void move_cursor(int8_t dx, int8_t dy);

/**
 * @brief Play human's turn, normal shot at one cell.
 * @return 1 if valid move, 0 if invalid.
 */
uint8_t human_turn();
// Play the computer's turn
void computer_turn();

uint8_t bomb_cheat();
uint8_t horizontal_cheat();
uint8_t vertical_cheat();

// 0 for non-salvo, 1 for salvo
uint8_t salvo_mode;
// Shot cap for first six turns, 1-6
uint8_t salvo_shot_limit;
// Num of human shots fired on their turn for salvo mode
uint8_t shots_fired;
// Num of shots fired, bomb counts as multiple
uint8_t cells_fired;
// 1 if human turn and salvo mode. 0 otherwise
uint8_t human_salvo_mode;

uint8_t shots_left(uint8_t turn);
void complete_turn(uint8_t turn);

// Returns 1 if the human won, 2 if the computer won, 0 otherwise.
uint8_t is_game_over(void);

// 0 for normal com, 1 for search and destroy
uint8_t computer_mode;

/**
 * @brief Set human setup mode. 1 if human is in setup mode, 0 otherwise
 */
void set_human_setup_mode(uint8_t new_val);

/**
 * @brief Get human setup mode. 1 if human is in setup mode, 0 otherwise
 */
uint8_t get_human_setup_mode();

/**
 * @brief Draw human grid orange on matrix
 */
void draw_human_grid();

/**
 * @brief Initialise human ship setup
 */
void initialise_human_setup();
/**
 * @brief Move human ship during setup. Recolours if valid.
 */
void move_human_ship(int8_t dx, int8_t dy);
/**
 * @brief Place human ship during setup, if valid
 */
void place_human_ship();
/**
 * @brief Rotate ship when r is pressed during setup
 */
void rotate_human_ship();

/**
 * @brief Set cheat visible, 1 if visible
 */
void set_cheat_visible(uint8_t new_val);

/**
 * @brief Get cheat visible, 1 if visible
 */
uint8_t get_cheat_visible();
/**
 * @brief Update all matrix cells with a ship, used for cheats
 */
void show_cheat();

/**
 * @brief Calculate high score and print to terminal
 */
void show_high_score();

// Colour LED matrix for game over
void game_over_matrix();

#define SEA 0
#define CARRIER 1
#define CRUISER 2
#define DESTROYER 3
#define FRIGATE 4
#define CORVETTE 5
#define SUBMARINE 6
#define SHIP_MASK 7
#define SHIP_END 8
#define HORIZONTAL 16

#endif
