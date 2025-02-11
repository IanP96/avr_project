/*
 * project.c
 *
 * Main file
 *
 * Authors: Peter Sutton, Luke Kamols, Jarrod Bennett, Cody Burnett
 * Modified by Ian Pinto
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define F_CPU 8000000UL
#include <util/delay.h>

#include "game.h"
#include "display.h"
#include "ledmatrix.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "timer0.h"
#include "timer1.h"
#include "timer2.h"

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void start_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);

void show_salvo_mode_terminal();

// Last time the cursor was flashed
volatile uint32_t last_flash_time;

/////////////////////////////// main //////////////////////////////////
int main(void)
{
    // Setup hardware and call backs. This will turn on
    // interrupts.
    initialise_hardware();

    salvo_mode = 0;
    show_salvo_mode_terminal();

    // Show the splash screen message. Returns when display
    // is complete.
    start_screen();

    // Loop forever and continuously play the game.
    while (1)
    {
        new_game();
        play_game();
        handle_game_over();
        start_screen();
    }
}

void initialise_hardware(void)
{
    ledmatrix_setup();
    init_button_interrupts();
    // Setup serial port for 19200 baud communication with no echo
    // of incoming characters
    init_serial_stdio(19200, 0);

    init_timer0();
    init_timer1();
    init_timer2();

    // Turn on global interrupts
    sei();

    // Prepare port C for output (leds)
    DDRC = 0b00111111;

    // Set up ADC for joystick
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1);
}

// Get serial input if available, otherwise return -1
char get_serial_input()
{
    char serial_input = -1;
    if (serial_input_available())
    {
        serial_input = fgetc(stdin);
    }
    return serial_input;
}

/**
 * @brief Get the serial input in lowercase, or -1 if unavailable
 */
char get_serial_input_lower()
{
    char c = get_serial_input();
    if (c == -1)
    {
        return c;
    }
    return tolower(c);
}

/**
 * @brief Update salvo mode on terminal.
 */
void show_salvo_mode_terminal()
{
    move_terminal_cursor(0, 18);
    clear_to_end_of_line();
    printf(
        "Salvo mode: %s",
        (salvo_mode ? "on" : "off"));
}

// Show computer mode on terminal
void show_com_mode_terminal()
{
    move_terminal_cursor(0, 17);
    clear_to_end_of_line();
    printf(
        "Computer mode is %s",
        (computer_mode ? "search and destroy" : "basic"));
}

void start_screen(void)
{
    // Clear terminal screen and output a message
    clear_terminal();
    hide_cursor();
    set_display_attribute(FG_WHITE);
    move_terminal_cursor(10, 4);
    printf_P(PSTR(" _______    ______  ________  ________  __        ________   ______   __    __  ______  _______  "));
    move_terminal_cursor(10, 5);
    printf_P(PSTR("|       \\  /      \\|        \\|        \\|  \\      |        \\ /      \\ |  \\  |  \\|      \\|       \\ "));
    move_terminal_cursor(10, 6);
    printf_P(PSTR("| $$$$$$$\\|  $$$$$$\\\\$$$$$$$$ \\$$$$$$$$| $$      | $$$$$$$$|  $$$$$$\\| $$  | $$ \\$$$$$$| $$$$$$$\\"));
    move_terminal_cursor(10, 7);
    printf_P(PSTR("| $$__/ $$| $$__| $$  | $$      | $$   | $$      | $$__    | $$___\\$$| $$__| $$  | $$  | $$__/ $$"));
    move_terminal_cursor(10, 8);
    printf_P(PSTR("| $$    $$| $$    $$  | $$      | $$   | $$      | $$  \\    \\$$    \\ | $$    $$  | $$  | $$    $$"));
    move_terminal_cursor(10, 9);
    printf_P(PSTR("| $$$$$$$\\| $$$$$$$$  | $$      | $$   | $$      | $$$$$    _\\$$$$$$\\| $$$$$$$$  | $$  | $$$$$$$ "));
    move_terminal_cursor(10, 10);
    printf_P(PSTR("| $$__/ $$| $$  | $$  | $$      | $$   | $$_____ | $$_____ |  \\__| $$| $$  | $$ _| $$_ | $$      "));
    move_terminal_cursor(10, 11);
    printf_P(PSTR("| $$    $$| $$  | $$  | $$      | $$   | $$     \\| $$     \\ \\$$    $$| $$  | $$|   $$ \\| $$      "));
    move_terminal_cursor(10, 12);
    printf_P(PSTR(" \\$$$$$$$  \\$$   \\$$   \\$$       \\$$    \\$$$$$$$$ \\$$$$$$$$  \\$$$$$$  \\$$   \\$$ \\$$$$$$ \\$$      "));
    move_terminal_cursor(10, 14);
    // change this to your name and student number; remove the chevrons <>
    printf_P(PSTR("CSSE2010/7201 Project by Ian Pinto - 48006581"));

    // Output the static start screen and wait for a push button
    // to be pushed or a serial input of 's'
    show_start_screen();

    uint32_t last_screen_update, current_time;
    last_screen_update = get_current_time();

    int8_t frame_number = -2 * ANIMATION_DELAY;

    computer_mode = 0;
    show_com_mode_terminal();

    show_salvo_mode_terminal();

    // Wait until a button is pressed, or 's' is pressed on the terminal
    while (1)
    {
        // First check for if a 's' is pressed
        // There are two steps to this
        // 1) collect any serial input (if available)
        // 2) check if the input is equal to the character 's'
        char serial_input = -1;
        if (serial_input_available())
        {
            serial_input = fgetc(stdin);
        }
        if (serial_input == 'y' || serial_input == 'Y')
        {
            computer_mode = !computer_mode;
            show_com_mode_terminal();
        }
        // If the serial input is 's', then exit the start screen
        if (serial_input == 's' || serial_input == 'S')
        {
            // Human setup, com randomised
            set_human_setup_mode(1);
            srand(get_current_time()); // Set seed based on timer time
            break;
        }
        if (serial_input == 'a' || serial_input == 'A')
        {
            // Default locations for human and com
            set_human_setup_mode(0);
            srand(get_current_time()); // Set seed based on timer time
            break;
        }
        if (serial_input == 'z' || serial_input == 'Z')
        {
            // Toggle salvo mode
            salvo_mode = !salvo_mode;
            show_salvo_mode_terminal();
        }

        // Next check for any button presses
        int8_t btn = button_pushed();
        if (btn != NO_BUTTON_PUSHED)
        {
            break;
        }

        // every 200 ms, update the animation
        current_time = get_current_time();
        if (current_time - last_screen_update > 200)
        {
            update_start_screen(frame_number);
            frame_number++;
            if (frame_number > ANIMATION_LENGTH)
            {
                frame_number -= ANIMATION_LENGTH + ANIMATION_DELAY;
            }
            last_screen_update = current_time;
        }
    }
}

void new_game(void)
{
    // Clear the serial terminal
    clear_terminal();

    show_com_mode_terminal();
    show_salvo_mode_terminal();

    // Ship setup mode
    move_terminal_cursor(0, 19);
    clear_to_end_of_line();
    printf("Ship setup: ");
    if (get_human_setup_mode())
    {
        printf("manual for human, random for computer");
    }
    else
    {
        printf("default for human and computer");
    }

    // Initialise the game and display
    initialise_game();

    // Clear a button push or serial input if any are waiting
    // (The cast to void means the return value is ignored.)
    (void)button_pushed();
    clear_serial_input_buffer();
}

/**
 * @brief Write a 0-6 value to LEDs
 */
void write_to_leds(uint8_t val)
{
    uint8_t mask;
    for (uint8_t pin_num = 0; pin_num < 6; pin_num++)
    {
        mask = (1 << pin_num);
        if (pin_num < val)
        {
            PORTC |= mask;
        }
        else
        {
            PORTC &= ~mask;
        }
    }
}

int32_t joystick_val_x, joystick_val_y;
uint32_t last_joystick_check;
uint32_t joystick_delay;
// Dist from upright, at most ~412 each
int32_t joystick_delta_x, joystick_delta_y;
uint32_t taxicab_distance;

/**
 * @brief Initialise joystick delay, delta, check times
 *
 */
void initialise_joystick()
{
    last_joystick_check = get_current_time();
    joystick_delay = 255;
    joystick_delta_x = 0;
    joystick_delta_y = 0;
}

/**
 * @brief Check joystick, move cursor if needed
 */
void joystick_check()
{

    // y
    ADMUX |= 1;
    // Start the ADC conversion
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
    {
        ; // Wait until conversion finished
    }
    joystick_val_y = ADC; // read the value

    // x
    ADMUX &= ~1;
    // Start the ADC conversion
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
    {
        ; // Wait until conversion finished
    }
    joystick_val_x = ADC; // read the value

    // Find dist from upright pos
    if (joystick_val_x > (511 + 100))
    {
        joystick_delta_x = joystick_val_x - (511 + 100);
    }
    else if (joystick_val_x < (511 - 100))
    {
        joystick_delta_x = joystick_val_x - (511 - 100);
    }
    else
    {
        joystick_delta_x = 0;
    }
    if (joystick_val_y > (511 + 100))
    {
        joystick_delta_y = joystick_val_y - (511 + 100);
    }
    else if (joystick_val_y < (511 - 100))
    {
        joystick_delta_y = joystick_val_y - (511 - 100);
    }
    else
    {
        joystick_delta_y = 0;
    }

    // Move cursor
    int8_t dx, dy;
    dx = ((joystick_delta_x > 0) ? 1 : ((joystick_delta_x < 0) ? -1 : 0));
    dy = ((joystick_delta_y > 0) ? 1 : ((joystick_delta_y < 0) ? -1 : 0));
    if (dx || dy)
    {
        move_cursor(dx, dy);
    }

    // Update timing
    joystick_delay = 600 - pow(pow(joystick_delta_x, 2) + pow(joystick_delta_y, 2), 0.5);
    last_joystick_check = get_current_time();
}

void play_game(void)
{
    uint32_t current_time;
    int8_t btn; // The button pushed

    last_flash_time = get_current_time();

    // 0 if not paused, 1 if paused
    uint8_t paused = 0;
    // Time between current and last flash
    uint32_t time_delta = 0;

    // Whether the human has made a valid move
    uint8_t valid_human_move = 0;

    write_to_leds(0); // TODO change to 0

    if (get_human_setup_mode())
    {
        initialise_human_setup();
    }

    // Human setup
    while (get_human_setup_mode())
    {
        btn = button_pushed();
        // Serial input made lowercase
        char serial_input_lower = get_serial_input_lower();
        human_salvo_mode = salvo_mode;

        if (btn == BUTTON0_PUSHED || serial_input_lower == 'd')
        {
            // Right
            move_human_ship(1, 0);
        }
        else if (btn == BUTTON1_PUSHED || serial_input_lower == 's')
        {
            // Down
            move_human_ship(0, -1);
        }
        else if (btn == BUTTON2_PUSHED || serial_input_lower == 'w')
        {
            // Up
            move_human_ship(0, 1);
        }
        else if (btn == BUTTON3_PUSHED || serial_input_lower == 'a')
        {
            // Left
            move_human_ship(-1, 0);
        }
        else if (serial_input_lower == 'f')
        {
            // Place ship
            place_human_ship();
        }
        else if (serial_input_lower == 'r')
        {
            rotate_human_ship();
        }
    }

    draw_human_grid();

    // Who won
    uint8_t result = is_game_over();

    volatile uint32_t last_cheat_time = get_current_time();

    initialise_joystick();

    // We play the game until it's over
    while (!result)
    {
        // We need to check if any button has been pushed, this will be
        // NO_BUTTON_PUSHED if no button has been pushed
        // Checkout the function comment in `buttons.h` and the implementation
        // in `buttons.c`.
        btn = button_pushed();
        // Serial input made lowercase
        char serial_input_lower = get_serial_input_lower();
        human_salvo_mode = salvo_mode;

        if (!paused)
        {
            valid_human_move = 0;
            if (salvo_mode)
            {
                write_to_leds(shots_left(0));
            }

            if (btn == BUTTON0_PUSHED || serial_input_lower == 'd')
            {
                // Right
                move_cursor(1, 0);
            }
            else if (btn == BUTTON1_PUSHED || serial_input_lower == 's')
            {
                // Down
                move_cursor(0, -1);
            }
            else if (btn == BUTTON2_PUSHED || serial_input_lower == 'w')
            {
                // Up
                move_cursor(0, 1);
            }
            else if (btn == BUTTON3_PUSHED || serial_input_lower == 'a')
            {
                // Left
                move_cursor(-1, 0);
            }
            else if (serial_input_lower == 'f')
            {
                // Fire
                valid_human_move = human_turn();
            }
            else if (serial_input_lower == 'b')
            {
                valid_human_move = bomb_cheat();
            }
            else if (serial_input_lower == 'n')
            {
                valid_human_move = horizontal_cheat();
            }
            else if (serial_input_lower == 'm')
            {
                valid_human_move = vertical_cheat();
            }
            else if (serial_input_lower == 'c')
            {
                // Cheats
                set_cheat_visible(1);
                last_cheat_time = get_current_time();
                show_cheat();
            }

            current_time = get_current_time();
            if (current_time >= last_flash_time + 200)
            {
                // 200ms (0.2 second) has passed since the last time we advance the
                // notes here, so update the advance the notes
                flash_cursor();

                // Update the most recent time the notes were advance
                last_flash_time = current_time;
            }
            if (current_time >= last_cheat_time + 1000)
            {
                // One second passed, no more cheats
                if (get_cheat_visible())
                {
                    set_cheat_visible(0);
                    show_cheat();
                }
            }
            if (current_time >= last_joystick_check + joystick_delay)
            {
                joystick_check();
            }

            // // Joystick, TODO delete
            // if (x_or_y == 0)
            // {
            //     // x
            //     ADMUX &= ~1;
            // }
            // else
            // {
            //     // y
            //     ADMUX |= 1;
            // }
            // // Start the ADC conversion
            // ADCSRA |= (1 << ADSC);
            // while (ADCSRA & (1 << ADSC))
            // {
            //     ; /* Wait until conversion finished */
            // }
            // joystick_value = ADC; // read the value

            if (valid_human_move && shots_left(0) == 0)
            {
                complete_turn(0);
                if (is_game_over())
                {
                    break;
                }
                write_to_leds(shots_left(1));
                while (shots_left(1) != 0)
                {
                    computer_turn();
                    write_to_leds(shots_left(1));
                }
                complete_turn(1);
            }
            result = is_game_over();
        }
        if (serial_input_lower == 'p')
        {
            if (paused)
            {
                paused = 0;
                move_terminal_cursor(0, 11);
                clear_to_end_of_line();
                last_flash_time = get_current_time() - time_delta;
            }
            else
            {
                paused = 1;
                move_terminal_cursor(0, 11);
                printf("Game paused.");
                time_delta = get_current_time() - last_flash_time;
            }
        }
    }
    // We get here if the game is over.
}

void handle_game_over()
{
    set_cheat_visible(0);

    move_terminal_cursor(10, 14);
    printf_P(PSTR("GAME OVER"));
    move_terminal_cursor(10, 15);
    printf_P(PSTR("Press a button or 's'/'S' to start a new game"));

    if (is_game_over() == 1)
    {
        // Human won, show high score
        show_high_score();
    }

    // Who won? Print to terminal
    /**
     * @brief 1 if the human won, 2 if the computer won
     */
    uint8_t winner = is_game_over();
    move_terminal_cursor(0, 9);
    printf(
        "The %s won.",
        ((winner == 1) ? "human" : "computer"));

    game_over_matrix();

    while (button_pushed() == NO_BUTTON_PUSHED && tolower(get_serial_input()) != 's')
    {
        ; // wait
    }
}
