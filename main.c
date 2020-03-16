/*	Author: rnava021
 *  Name: Ruth Navarrete
 *	Lab Section: 24
 *	Assignment: Custom lab project - Snake
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "header/io.h"
#include "header/timer.h"
#include "header/bit.h"
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif

#include <stdlib.h>	// abs(), rand()
#include <stdio.h>	// sprintf()

#define start_button ((~(PINA) & 0x04)) // A2
#define reset_button ((~(PINA) & 0x08)) // A3

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

unsigned char set_bit_non_port (unsigned char x, unsigned char k, unsigned char b) {
	return (b ? (x | (0x01 << k)) : (x & ~(0x01 << k)));
}

unsigned char get_bit_non_port (unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}

// A struct to collect all items related to a task.
 typedef struct task {
	int state;                  // Task's current state
	unsigned long period;       // Task period
	unsigned long elapsedTime;  // Time elapsed since last task tick
	int (*TickFct)(int);        // Task tick function
} task;

// structs unique to game
struct snake {
	unsigned char x;
	unsigned char y;
	char direction;
} snake;

typedef struct food {
	unsigned char x;
	unsigned char y;
} food;

// array to hold tasks
task tasks[4];
const unsigned short tasksNum = 4;

// constant variables
const unsigned long periodGCD = 1;

// shared variables
unsigned char matrix_coordinates[8][8];		// keep track of all elements on board, goes [column] [row]
/* idea of matrix coordinates
 * C0{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C1{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C2{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C3{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C4{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C5{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C6{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * C7{[R0][R1][R2][R3][R4][R5][R6][R7]}
 * when populating, find the column then the row
 */
unsigned char in_game;					// if 1, game is in progress. if 0, game is not in progress
unsigned char score;					// local high score, will overwrite global high score if higher
char new_direction;
struct snake body_of_snake[64];		// max number of snake nodes is 64
unsigned char number_of_snake_node;		// number of snake nodes, increases when food is eaten
struct food game_food;					// food element in game, coordinates update when food is eaten
// strings to be displayed at various points in the game
char game_over_prompt[]	= "GAME OVER";
char r_start_prompt[]	= "release button  to begin";
char r_reset_prompt[]	= "release button  to reset";
char r_restart_prompt[]	= "release button  to restart";
char prompt[]			= "Eat RED food to make snake grow!";
char scores[]			= "High Score:          Score:     ";
char reset_prompt[]     = "RESET HIGH SCORE";
//						[12345678901234567890123456789012]
//							0-10  |  11-20  |  21-30  | 31-32 
uint8_t high_score;						// EEPROM memory location for global high score

/* used to shift values for led matrix
 * led matrix is 32 bits long where blue is [7:0], red is [15:8], green is [27-20], control is [19:16], [31:28]
 * my breadboard is set up that there are three shift registers to set control, green, and red
 * control goes to first shift register
 * green goes to second shift register
 * red goes to third shift register
 */
void shift_register (unsigned char control_line, unsigned char green_lines, unsigned char red_lines) {
//void shift_register () {
	unsigned long i = 0;
	
	// volatile since otherwise compiler will optimize and remove it
	volatile unsigned long conversion = 0x00000000;
	
	// set data so that it is in the form [0 == blue] [red] [green] [control]
	unsigned long data = 0x00000000;
	data |= (control_line | conversion);
	data |= (green_lines | conversion) << 8;
	data |= (red_lines | conversion) << 16;
	
	// iterate for all 32 data bits
	// shift starting from MSB and going to LSB
	// aka [0 == blue] goes out first
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
void chose_direction() {
	unsigned short ud_val, lr_val;
	
	// sample joystick
	ud_val = set_adc_mux(0x00);
	lr_val = set_adc_mux(0x01);
	
	// left logic shifted to right because my matrix is upside down in regard to joystick
	if ( (lr_val < (512 - 256)) && (abs(ud_val - 512) < 256) ) {
		new_direction = 'r';
		return;
	}
	
	// right logic shifted to left because my matrix is upside down in regard to joystick
	if ( (lr_val >= (512 + 256)) && (abs(ud_val - 512) < 256) ) {
		new_direction = 'l';
		return;
	}
	
	// up logic shifted to down because my matrix is upside down in regard to joystick
	if ( (ud_val >= (512 + 256)) && (abs(lr_val - 512) < 256) ) {
		new_direction = 'd';
		return;
	}
	
	// down logic shifted to up because my matrix is upside down in regard to joystick
	if ( (ud_val < (512 - 256)) && (abs(lr_val - 512) < 256) ) {
		new_direction = 'u';
		return;
	}
	
	// no conditions met, maintain new_direction
	return;
}

// clear the matrix)coordinates board used to keep track of snake node locations
void clear_board() {
    unsigned rows, columns, i;
	
    // clear board so that food spot does not also get displayed
	for (columns = 0; columns < 8; columns++) {
		for (rows = 0; rows < 8; rows++) {
			matrix_coordinates[columns][rows] = 0;
		}
	}
}

// set led matrix board coordinates, keeping track of number of elements that fill a space
void populate_board() {
	unsigned rows, columns, i;
	
    // clear board so that food spot does not also get displayed
	clear_board();
	
    // iterate over the snake, starting from tail going to head
    // for each snake coordinate, populate the 2d array spot
	for (i = 0; i < number_of_snake_node; i++) {
		matrix_coordinates[body_of_snake[i].y][body_of_snake[i].x] += 1;
	}
}

// change x and y positions of the snake nodes
void move_snake_positions() {
	unsigned char i;
	for (i = (number_of_snake_node - 1); i > 0; i--) {
		body_of_snake[i].x = body_of_snake[i - 1].x;
		body_of_snake[i].y = body_of_snake[i - 1].y;
		body_of_snake[i].direction = body_of_snake[i - 1].direction;
	}
	switch (new_direction) {
		case 'l':
			body_of_snake[0].y += 1;
			body_of_snake[0].x;
			body_of_snake[0].direction = 'l';
			break;
		case 'r':
			body_of_snake[0].y -= 1;
			body_of_snake[0].x;
			body_of_snake[0].direction = 'r';
			break;
		case 'u':
			body_of_snake[0].y;
			body_of_snake[0].x += 1;
			body_of_snake[0].direction = 'u';
			break;
		case 'd':
			body_of_snake[0].y;
			body_of_snake[0].x -= 1;
			body_of_snake[0].direction = 'd';
			break;
		default:
			if (body_of_snake[0].direction == 'l') {
				body_of_snake[0].y += 1;
				body_of_snake[0].x;
			}
			if (body_of_snake[0].direction == 'r') {
				body_of_snake[0].y -= 1;
				body_of_snake[0].x;
			}
			if (body_of_snake[0].direction == 'u') {
				body_of_snake[0].y;
				body_of_snake[0].x += 1;
			}
			if (body_of_snake[0].direction == 'd') {
				body_of_snake[0].y;
				body_of_snake[0].x -= 1;
			}
		
			body_of_snake[0].direction = new_direction;
			break;
	}
}

// check to see if the snake collides with anything
// if snake collides with self or walls, return 1
// else if snake collides with nothing, return 0
unsigned char check_collision ()  {
	unsigned char column, row;
	
	for (column = 0; column < 8; column++) {
		for (row = 0; row < 8; row++) {
            // snake collides with itself
			if (matrix_coordinates[body_of_snake[0].y][body_of_snake[0].x] > 1) {
				return 1;
			}
            // snake collides with wall
			if ((body_of_snake[0].x < 0) || (body_of_snake[0].x > 7) || (body_of_snake[0].y < 0) || (body_of_snake[0].y > 7)) {
				return 1;
			}
		}
	}
	return 0;
}

// update the lines for the shift register for the led matrix
// shows each individual green node, one at a time
// used to show green snake
void update_matrix_lines_green () {
	unsigned char lines_control = 0x00;
    unsigned char lines_color = 0xFF;
	static unsigned char i = 0;
	if (!(i < number_of_snake_node)) {
		i = 0;
	}
	lines_control = set_bit_non_port(lines_control, body_of_snake[i].y, 1);
    lines_color = set_bit_non_port(lines_color, body_of_snake[i].x, 0);
	shift_register(lines_control, lines_color, 0xFF);
	i++;
}

// update the lines for the shift register for the led matrix
// based on game_food coordinates
// used to show red food
void update_matrix_lines_red () {
	unsigned char lines_control = 0x00;
    unsigned char lines_color = 0xFF;
	lines_control = set_bit_non_port(lines_control, game_food.y, 1);
	lines_color =set_bit_non_port(lines_color, game_food.x, 0);
	shift_register(lines_control, 0xFF, lines_color);
}

// move food to new location that is not on top of the snake
void populate_new_food() {
	unsigned char temp_x;
	unsigned char temp_y;
	
	do {
		temp_x = rand() % 8;
		temp_y = rand() % 8;
		populate_board();
		matrix_coordinates[temp_y][temp_x] += 1;
		LCD_DisplayString(1, "food while");
	} while (matrix_coordinates[temp_y][temp_x] > 1);
	
	game_food.x = temp_x;
	game_food.y = temp_y;
}

// this function is the one causing problems currently
void add_new_snake_segment() {
	unsigned char i;
	
	// for all elements, adjust the coordinates and direction so that each element gets shifted to the right one
	// this allows for a new node to be added to the front of the array
	for (i = number_of_snake_node; i > 0; i--) {
		body_of_snake[i].x = body_of_snake[i - 1].x;
		body_of_snake[i].y = body_of_snake[i - 1].y;
		body_of_snake[i].direction = body_of_snake[i - 1].direction;
	}
	
	if (new_direction == 'u') {
		body_of_snake[0].y = game_food.y;
		body_of_snake[0].x = game_food.x + 1;
	}
	else if (new_direction == 'd') {
		body_of_snake[0].y = game_food.y;
		body_of_snake[0].x = game_food.x - 1;
	}
	else if (new_direction == 'l') {
		body_of_snake[0].y = game_food.y + 1;
		body_of_snake[0].x = game_food.x;
	}
	else {
		body_of_snake[0].y = game_food.y - 1;
		body_of_snake[0].x = game_food.x;
	}
	body_of_snake[0].direction = new_direction;
	number_of_snake_node++;
}

enum button_handler_states {bh_init, game_off, wait_release_start, game_in_progress, game_over_display, wait_release_reset, resetting, wait_start_game_over};
int button_handler_SM (int state) {
	static unsigned char i;
	static char high_score_output[2];
	static char score_output[2];
	
	switch (state) { // transitions
		case bh_init:
			state = game_off;
			in_game = 0;
			break;
		case game_off:
			if (reset_button) {
				state = wait_release_reset;
			}
			else if (start_button) {
				state = wait_release_start;
			}
			else {
				state = game_off;
			}
			break;
		case wait_release_start:
			state = (start_button) ? wait_release_start : game_in_progress;
			if (!start_button) {
				in_game = 1;
				number_of_snake_node = 0;
			}
			break;
		case game_in_progress:
			if (reset_button) {
				state = wait_start_game_over;
				in_game = 0;
			}
			else if (in_game) {
				state = game_in_progress;
			}
			else {
				state = game_over_display;
				i = 0;
			}
			break;
		case game_over_display:
			state = (i < 20) ? game_over_display : game_off;
			break;
		case wait_release_reset:
			state = (reset_button) ? wait_release_reset : resetting;
			if (!reset_button) {
				i = 0;
			}
			break;
		case resetting:
			state = (i < 20) ? resetting : game_off;
			break;
		case wait_start_game_over:
			state = (reset_button) ? wait_start_game_over : game_off;
			if (!reset_button) {
				in_game = 0;
			}
			break;
		default:
			state = bh_init;
			break;
	} // transitions

	switch (state) { // actions
		case bh_init:
			break;
		case game_off:
			LCD_DisplayString(1,  prompt);
			break;
		case wait_release_start:
			LCD_DisplayString(1, r_start_prompt);
			break;
		case game_in_progress:
			LCD_DisplayString(1, scores);
			sprintf(score_output, "%d", score);
			LCD_DisplayString_WO_ClearScreen(29, score_output);
			sprintf(high_score_output, "%d", eeprom_read_byte(&high_score));
			LCD_DisplayString_WO_ClearScreen(13, high_score_output);
			break;
		case game_over_display:
			shift_register(0xFF, 0xFF,0x00);
			LCD_DisplayString(1, game_over_prompt);
			i++;
			break;
		case wait_release_reset:
			LCD_DisplayString(1, r_reset_prompt);
			break;
		case resetting:
			i++;
			eeprom_write_byte(&high_score, 0);
			LCD_DisplayString(1, reset_prompt);
			break;
		case wait_start_game_over:
			LCD_DisplayString(1, r_restart_prompt);
			break;
		default:
			LCD_DisplayString(1, "broken");
			break;
	} // actions
	return state;
}

enum joystick_states {js_init, js_wait_for_in_game, js_in_game};
int joystick_SM (int state) {
	switch (state) { // transitions
		case js_init:
			state = js_wait_for_in_game;
			break;
		case js_wait_for_in_game:
			state = (in_game) ? js_in_game : js_wait_for_in_game;
			if (in_game) {
				new_direction = 'r';
			}
			break;
		case js_in_game:
			state = (in_game) ? js_in_game : js_wait_for_in_game;
			break;
		default:
			js_init;
			break;
	} // transitions
	
	switch (state) { // actions
		case js_init:
			break;
		case js_wait_for_in_game:
			break;
		case js_in_game:
			chose_direction();
			break;
		default:
			break;
	} // actions
	return state;
}

enum snake_movement_states {sm_init, sm_wait_for_in_game, sm_in_game};
int snake_movement_SM (int state) {
	static unsigned long timeing = 60000;
	unsigned char i;
	
	switch (state) { // transitions
		case sm_init:
			state = sm_wait_for_in_game;
			break;
		case sm_wait_for_in_game:
			state = (in_game) ? sm_in_game : sm_wait_for_in_game;
			if (in_game) {
				score = 0;
				number_of_snake_node = 0;
				game_food.x = 2;
				game_food.y = 4;
				timeing = 0;
			}
			break;
		case sm_in_game:
			state = (in_game) ? sm_in_game : sm_wait_for_in_game;
			break;
		default:
			sm_init;
			break;
	} // transitions
	
	switch (state) { // actions
		case sm_init:
			break;
		case sm_wait_for_in_game:
			if (eeprom_read_byte(&high_score) > 64) {
				eeprom_write_byte(&high_score, 0);
			}
			break;

		case sm_in_game:
			if (number_of_snake_node == 0) {
				body_of_snake[number_of_snake_node].x = 4;
				body_of_snake[number_of_snake_node].y = 6;
				body_of_snake[number_of_snake_node].direction = 'r';
				number_of_snake_node++;
				populate_new_food();
			}
			move_snake_positions();
			populate_board();
			
			if ((body_of_snake[0].x == game_food.x) && (body_of_snake[0].y == game_food.y)) {
				add_new_snake_segment();
				score++;				
				populate_new_food();
			}
			if (score > eeprom_read_byte(&high_score)) {
				eeprom_write_byte(&high_score, score);
			}
			
			if (check_collision()) {
				in_game = 0;
			}
			break;
		default:
			break;
	} // actions
	return state;
}

enum led_matrix_states {lm_init, lm_wait_for_in_game, lm_in_game_green, lm_in_game_red};
int led_matrix_SM (int state) {
	unsigned char temp_control;
	unsigned char temp_color;
	
	switch (state) { // transitions
		case lm_init:
			state = lm_wait_for_in_game;
			break;
		case lm_wait_for_in_game:
			state = (in_game) ? lm_in_game_green : lm_wait_for_in_game;
			break;
		case lm_in_game_green:
			state = (in_game) ? lm_in_game_red : lm_wait_for_in_game;
			break;
		case lm_in_game_red:
			state = (in_game) ? lm_in_game_green : lm_wait_for_in_game;
			break;
		default:
			lm_init;
			break;
	} // transitions
	
	switch (state) { // actions
		case lm_init:
			break;
		case lm_wait_for_in_game:
			shift_register(0x00, 0x00, 0x00);
			break;
		case lm_in_game_green:
            update_matrix_lines_green();
			break;
		case lm_in_game_red:
            update_matrix_lines_red();
			break;
		default:
			break;
	} // actions
	return state;
}

int main (void) {
	// Insert DDR and PORT initializations
	DDRA = 0x10; PORTA = 0xEF;  // Configure port A's 8 pins as inputs
	DDRB = 0xFF; PORTB = 0x00;  // Configure port B's 8 pins as outputs
	DDRC = 0xFF; PORTC = 0x00;  // LCD data lines
	DDRD = 0xFF; PORTD = 0x00;  // LCD control lines
	
	TimerSet(periodGCD);
	TimerOn();
	LCD_init();
	ADC_init();
	
	unsigned char i = 0;
	tasks[i].state = bh_init;
	tasks[i].period = 100;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &button_handler_SM;
	i++;

	tasks[i].state = js_init;
	tasks[i].period = 50;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &joystick_SM;
	i++;

	tasks[i].state = sm_init;
	tasks[i].period = 200;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &snake_movement_SM;
	i++;

	tasks[i].state = lm_init;
	tasks[i].period = 1;
	tasks[i].elapsedTime = 0;
	tasks[i].TickFct = &led_matrix_SM;
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