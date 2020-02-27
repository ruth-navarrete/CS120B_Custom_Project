/*	Author: rnava021
 *  Partner(s) Name: Ruth Navarrete
 *	Lab Section: 24
 *	Assignment: Custom lab project
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include "header/io.h"
#include "header/timer.h"
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif

void ADC_init() {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}

/*
PB0 = SR_1	(0x01)
PB1 = RCLK	(0x02)
PB2 = SRCLK	(0x04)
PB3 = SRCLR	(0x08)
*/

int main(void) {
    return 0;
}