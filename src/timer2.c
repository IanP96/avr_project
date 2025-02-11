/*
 * timer2.c
 *
 * Author: Peter Sutton
 *
 * timer 2 skeleton
 */

#include "timer2.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/* Set up timer 2
 */
void init_timer2(void)
{
	TCNT2 = 0;
}
