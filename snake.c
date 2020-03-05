/*	Author: rnava021
 *  Name: Ruth Navarrete
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

#include <stdlib.h>	// abs()

// initialize ADC
void ADC_init () {
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}

// chose what pin is used to read ADC from using ADC mux
uint16_t set_adc_mux (unsigned char port){
	ADMUX = (port <= 0x07) ? (ADMUX & 0xF8) | port : ADMUX;
	
	// Start conversion by setting the ADSC bit.
	ADCSRA |= (1 << ADSC);
	
	// Wait for conversion to complete
	static unsigned char i = 0;
	for(i = 0; i < 15; ++i) {
		asm("nop");
	}
	
	// Read the results
	return ADC;
	
}

// A struct to collect all items related to a task.
typedef struct task {
	int state;                  // Task's current state
	unsigned long period;       // Task period
	unsigned long elapsedTime;  // Time elapsed since last task tick
	int (*TickFct)(int);        // Task tick function
} task;

// array to hold tasks
task tasks[2];
const unsigned short tasksNum = 2;

// constant variables
const unsigned long periodGCD = 50;
const unsigned long periodLedMatrix = 500;
const unsigned long periodJoystick = 50;

// shared variables
unsigned char matrix_coordinates[8][8];

/*	pin set up for shift register
 * PB0 = SER	(0x01)	start of data input
 * PB1 = RCLK	(0x02)
 * PB2 = SRCLK	(0x04)
 * PB3 = SRCLR	(0x08)
 */

/* used to shift values for led matrix
 * led matrix is 32 bits long
 * bits 0-7   set blue
 * bits 8-15  set red
 * bits 16-19 set control lines
 * bits 20-27 set green
 * bits 28-31 set control lines
 * my breadboard is set up that there are three shift registers to set control, green, and red
 * control goes to first shift register
 * green goes to second shift register
 * red goes to third shift register
 */
void shift_register (unsigned char control_line, unsigned char green_lines, unsigned char red_lines) {
	unsigned long i = 0;
	
	// volatile since otherwise compiler will optimize and remove it
	volatile unsigned long conversion = 0x00000000;
	
	// set data so that it is in the form [0] [red] [green] [control]
	unsigned long data = 0x00000000;
	data |= (control_line | conversion);
	data |= (green_lines | conversion) << 8;
	data |= (red_lines | conversion) << 16;
	
	// iterate for all 32 data bits
	// shift starting from MSB and going to LSB
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
	//PORTB = 0x00;
}

// samples ADC from joystick and then chooses either up/down/left/right/none for direction
char chose_direction() {
	unsigned short ud_val, lr_val;
	
	// sample joystick
	ud_val = set_adc_mux(0x00);
	lr_val = set_adc_mux(0x01);
	
	// left logic
	if ( (lr_val < (512 - 256)) && (abs(ud_val - 512) < 256) ) {
		return 'l';
	}
	
	// right logic
	if ( (lr_val >= (512 + 256)) && (abs(ud_val - 512) < 256) ) {
		return 'r';
	}
	
	// up logic
	if ( (ud_val >= (512 + 256)) && (abs(lr_val - 512) < 256) ) {
		return 'u';
	}
	
	// down logic
	if ( (ud_val < (512 - 256)) && (abs(lr_val - 512) < 256) ) {
		return 'd';
	}
	
	// no conditions met
	return 'n';
}

enum led_states {led_start, red, green};
int led_matrix_SM (int state) {
	switch (state) { // transitions
		case led_start:
			state = red;
			break;
		case red:
			state = green;
			break;
		case green:
			state = red;
			break;
		default:
			state = led_start;
			break;
	} // transitions
	
	switch (state) { // actions
		case led_start:
			shift_register(0x10, 0xF7, 0xFF);
			break;
		case red:
			//shift_register(0x55, 0xFF, 0x00);
			shift_register(0x55, 0xFF, 0x55);
			// set high the column you want, set low the color you want
			break;
		case green:
			//shift_register(0xAA, 0x00, 0xFF);
			//shift_register(0xFF, 0x55, 0x00);
			shift_register(0xAA, 0xAA, 0xFF);
			break;
		default:
			break;
	} // actions
	return state;
}
	
enum joystick_states {joystick_start, wait, up, down, left, right};
int joystick_SM (int state) {
	char curr_direction, new_direction;
	
	switch (state) { // transitions
		case joystick_start:
			curr_direction = 'r';
			state = wait;
			break;
		case wait:			
			new_direction = chose_direction();
			if (new_direction == 'l') {
				state = left;
			}
			else if (new_direction == 'r') {
				state = right;
			}
			else if (new_direction == 'u') {
				state = up;
			}
			else if (new_direction == 'd') {
				state = down;
			}
			break;
		case up:
			state = wait;
			break;
		case down:
			state = wait;
			break;
		case left:
			state = wait;
			break;
		case right:
			state = wait;
			break;
		default:
			state = joystick_start;
			break;
	} // transitions

	switch (state) { // actions
		case joystick_start:
			break;
		case wait:
			break;
		case up:
			LCD_DisplayString(1, "UP");
			break;
		case down:
			LCD_DisplayString(1, "down");
			break;
		case left:
			LCD_DisplayString(1, "left");
			break;
		case right:
			LCD_DisplayString(1, "RIGHT");
			break;
		default:
			break;
	} // actions
	return state;
}

int main (void) {
	// Insert DDR and PORT initializations
	DDRA = 0x00; PORTA = 0xFF;  // Configure port A's 8 pins as inputs
	DDRB = 0xFF; PORTB = 0x00;  // Configure port B's 8 pins as outputs
	DDRC = 0xFF; PORTC = 0x00;  // LCD data lines
	DDRD = 0xFF; PORTD = 0x00;  // LCD control lines
	
	TimerSet(periodGCD);
	TimerOn();
	LCD_init();
	ADC_init();
	
	unsigned char i = 0;
	tasks[i].state = led_start;
	tasks[i].period = periodLedMatrix;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &led_matrix_SM;
	i++;
	
	tasks[i].state = joystick_start;
	tasks[i].period = periodJoystick;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &joystick_SM;
	i++;
	
	while (1) {
		for (i = 0; i < tasksNum; i++) {
			if (tasks[i].elapsedTime >= tasks[i].period) {
				tasks[i].state = tasks[i].TickFct(tasks[i].state);
				tasks[i].elapsedTime = 0;
			}
			tasks[i].elapsedTime += periodGCD;
		}
		while (!TimerFlag) { }
		TimerFlag = 0;
	}
	return 0;
}