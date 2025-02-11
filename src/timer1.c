/*
 * timer1.c
 *
 * Author: Peter Sutton
 *
 * timer 1 skeleton
 */

#include "timer1.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/* Set up timer 1
 */
void init_timer1(void)
{
	TCNT1 = 0;
}
