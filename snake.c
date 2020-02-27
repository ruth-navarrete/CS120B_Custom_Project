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

//void shift_register(unsigned char control_line, unsigned char red_lines, unsigned char green_lines) {
void shift_register(unsigned char control_line, unsigned char green_lines, unsigned char red_lines) {
	unsigned long i = 0;
	
	volatile unsigned long conversion = 0x00000000;
	
	/*
	unsigned long data = 0x00000000;
	data |= ( (control_line & 0x0F) | conversion);
	data |= (green_lines | conversion) << 12;
	data |= ((control_line & 0xF0) | conversion) << 16;
	data |= (red_lines | conversion) << 24;
	*/
	
	unsigned long data = 0x00000000;
	data |= (control_line | conversion);
	data |= (green_lines | conversion) << 8;
	data |= (red_lines | conversion) << 16;
	
	// iterate for all 32 data bits for control_line
	for (i = 32; i > 0; i--) {
		// set SRCLR to 1 to allow new data to be set
		// set SRCLK to 0 to stop sending of data
		PORTB = 0x08;
	
		// set SER to next data bit to be sent
		PORTB |= ((data >> (i - 1)) & 0x01);
		
		// set SRCLK to 1
		// rising edge moves data from shift to store reg
		PORTB |= 0x04;
	}
	
	// set RCLK to 1
	// rising edge copies data from shift to store register
	PORTB |= 0x02;
	
	// clear lines
	PORTB = 0x00;
}

enum led_states {led_start, red, green} led_state;
void led_tick() {
	switch (led_state) { /* transitions */
		case led_start:
			led_state = red;
			break;
		case red:
			led_state = green;
			break;
		case green:
			led_state = red;
			break;
		default:
			led_state = led_start;
			break;
	} // transitions
	
	switch (led_state) { // actions
		case led_start:
			break;
		case red:
			//shift_register(0x55, 0xFF, 0x00);
			shift_register(0x55, 0xFF, 0x55);
			// set low the column you want, set high the color you want
			break;
		case green:
			//shift_register(0xAA, 0x00, 0xFF);
			//shift_register(0xFF, 0x55, 0x00);
			shift_register(0xAA, 0xAA, 0xFF);
			break;
		default:
			break;
	} // actions
}
	
enum joystick_states {joystick_start, wait, up, down, left, right} joystick_state;
void joystick_tick() {
	switch (joystick_state) { // transitions
		case joystick_start:
			joystick_state = wait;
			break;
		case wait:
			break;
		case up:
			break;
		case down:
			break;
		default:
			joystick_state = joystick_start;
			break;
		} // transitions
		
		switch (joystick_state) { // actions
			case joystick_start:
				break;
			case up:
				break;
			case down:
				break;
			case left:
				break;
			case right:
				break;
			default:
				break;
		} // actions
	}

int main(void) {
	// Insert DDR and PORT initializations
	DDRA = 0x00; PORTA = 0xFF;  // Configure port A's 8 pins as inputs
	DDRB = 0xFF; PORTB = 0x00;  // Configure port B's 8 pins as outputs
	DDRC = 0xFF; PORTC = 0x00;  // LCD data lines
	DDRD = 0xFF; PORTD = 0x00;  // LCD control lines
	
	led_state = led_start;
	joystick_state = joystick_start;
	TimerSet(500);
	TimerOn();
	ADC_init();
	
	while(1) {
		led_tick();
		while (!TimerFlag) {}
		TimerFlag = 0;
	}
}