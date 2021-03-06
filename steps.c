#include "steps.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <math.h>
#include "global_variables.h"
#include "uart.h"
#include "config.h"


extern struct vector location;
extern struct vector placement;

const char DRIVE_PHASES_X[] = {0b10000000,0b11000000,0b01000000,0b01100000,0b00100000,0b00110000,0b00010000,0b10010000};
const char DRIVE_PHASES_Y[] = {0b00001000,0b00001100,0b00000100,0b00000110,0b00000010,0b00000011,0b00000001,0b00001001};
const char DRIVE_PHASES_Z[] = {0b10000000,0b11000000,0b01000000,0b01100000,0b00100000,0b00110000,0b00010000,0b10010000};
const char DRIVE_PHASES_E[] = {0b00001000,0b00001100,0b00000100,0b00000110,0b00000010,0b00000011,0b00000001,0b00001001};



static struct discret_vector translation_discret = {0,0,0,0};
static long translation_discret_length = 0;
static struct discret_vector progress;
static uint8_t state0 = 0b00000000;
static int kappa = (OVERFLOWS_PER_SECOND_TIMER1*60)/(STEPS_PER_X*INITIAL_F);


ISR(TIMER1_OVF_vect){
	static int j = 0;
	static int i = 0;
	/*static int j = 0;*/
	if ((state0 == NEW_TASK) && (translation_discret_length != 0)){
		/*PORTC |= (1<<PC1);*/
		//PORTC |= (1<<PC0);
		if (j < kappa){
			j++;
			/*Для охлаждения*/
			PORTC = 0b00000000;
			PORTB = 0b00000000;
		}else{
			//PORTC = (1<<PC3);
			j = 0;
			if (translation_discret.x > 0){
				if (progress.x < i*translation_discret.x/translation_discret_length){
					progress.x += 1;
					location.x += 1.0/STEPS_PER_X;
					doStep(INC_X);
				}
			}else if (translation_discret.x < 0){
				if (progress.x > i*translation_discret.x/translation_discret_length){
					progress.x -= 1;
					location.x -= 1.0/STEPS_PER_X;
					doStep(DEC_X);
				}
			}

			if (translation_discret.y > 0){
				if (progress.y < i*translation_discret.y/translation_discret_length){
					progress.y += 1;
					location.y += 1.0/STEPS_PER_Y;
					doStep(INC_Y);
				}
			}else if (translation_discret.y < 0){
				if (progress.y > i*translation_discret.y/translation_discret_length){
					progress.y -= 1;
					location.y -= 1.0/STEPS_PER_Y;
					doStep(DEC_Y);
				}
			}

			if (translation_discret.z > 0){
				if (progress.z < i*translation_discret.z/translation_discret_length){
					progress.z += 1;
					location.z += 1.0/STEPS_PER_Z;
					doStep(INC_Z);
				}
			}else if (translation_discret.z < 0){
				if (progress.z > i*translation_discret.z/translation_discret_length){
					progress.z -= 1;
					location.z -= 1.0/STEPS_PER_Z;
					doStep(DEC_Z);
				}
			}

			if (translation_discret.e > 0){
				if (progress.e < i*translation_discret.e/translation_discret_length){
					progress.e += 1;
					location.e += 1.0/STEPS_PER_E;
					doStep(INC_E);
				}
			}else if (translation_discret.e < 0){
				if (progress.e > i*translation_discret.e/translation_discret_length){
					progress.e -= 1;
					location.e -= 1.0/STEPS_PER_E;
					doStep(DEC_E);
				}
			}

			if (i < translation_discret_length){
				i++;
			}else{
				/*
					When task has been complete
				*/
				state0 &= ~NEW_TASK;
				i = 0;
				/*Для охлаждения*/
				PORTC = 0b00000000;
				PORTB = 0b00000000;
				sendStaicMessage(SUCCESS_DONE);
			}
		}
		/*
		PORT_X &= CLEAR_X;
		PORT_X |= pgm_read_byte(driveX + j);
		if ( j == PHASE_LENGTH - 1){
			j = 0;
		}else{
			j++;
		}
		*/
	}else if ((state0 == NEW_TASK) && (translation_discret_length == 0)){
		state0 &= ~NEW_TASK;
		i = 0;
		j = 0;
		/*Для охлаждения*/
		PORTC = 0b00000000;
		PORTB = 0b00000000;
		sendStaicMessage(SUCCESS_DONE);
	}
}

void moveOn(float F){ /*Здесь ошибка*/
	/*
		Calculate translation vector
	*/

	struct vector translation;
	//if((location.x>1.9)&&(location.x<2.1)) {PORTC |= (1<<PC0);}
	//if((placement.x>-0.1)&&(placement.x<0.1)) {PORTC |= (1<<PC3);}
	/*if((F>-0.1)&&(F<0.1)) {PORTC |= (1<<PC3);}*/
	//if(location.x == placement.x) {PORTC |= (1<<PC0);}
	translation.x = placement.x - location.x;
	translation.y = placement.y - location.y;
	translation.z = placement.z - location.z;
	translation.e = placement.e - location.e;

	translation_discret.x = translation.x*STEPS_PER_X;
	translation_discret.y = translation.y*STEPS_PER_Y;
	translation_discret.z = translation.z*STEPS_PER_Z;
	translation_discret.e = translation.e*STEPS_PER_E;

	/*
		Calcutale length of vectors
	*/

	float translation_length = sqrt(
		translation.x*translation.x +
		translation.y*translation.y + 
		translation.z*translation.z + 
		translation.e*translation.e
	);

	translation_discret_length = sqrt(
		translation_discret.x*translation_discret.x +
		translation_discret.y*translation_discret.y + 
		translation_discret.z*translation_discret.z + 
		translation_discret.e*translation_discret.e
	);

	/*translation_discret_length = sqrt(
		translation.x*translation.x*STEPS_PER_X*STEPS_PER_X +
		translation.y*translation.y*STEPS_PER_Y*STEPS_PER_Y + 
		translation.z*translation.z*STEPS_PER_Z*STEPS_PER_Z + 
		translation.e*translation.e*STEPS_PER_E*STEPS_PER_E
	);*/


	/*
	Максимально достижимая скорость соответствует kappa = 0.5, следовательно F=6000 
	Смысл F - это скорость движения, выраженная в мм/мин	
	*/
	if (F>6000){F=6000;}
	//if (F<1000){F=1000;}	
	//F=6000;
	/*
		Calculate speed if F changed
	*/
	if ((F>0) && (translation_length > 0)){

		/*
			Check for speed limit
		*/
		
		if (F*abs(translation.x)/translation_length > MAX_ALLOWED_SPEED_X){
			F = MAX_ALLOWED_SPEED_X*translation_length/abs(translation.x);
		}
		if (F*abs(translation.y)/translation_length > MAX_ALLOWED_SPEED_Y){
			F = MAX_ALLOWED_SPEED_Y*translation_length/abs(translation.y);
		}
		if (F*abs(translation.z)/translation_length > MAX_ALLOWED_SPEED_Z){
			F = MAX_ALLOWED_SPEED_Z*translation_length/abs(translation.z);
		}
		if (F*abs(translation.e)/translation_length > MAX_ALLOWED_SPEED_E){
			F = MAX_ALLOWED_SPEED_E*translation_length/abs(translation.e);
		}
		
		/*
			Calculate new kappa
		*/
		/*kappa = translation_length*CPU_FREQURENCY*60/(F*TIMER1_DIVIDER*translation_discret_length);*/
		if (translation_discret_length == 0){
			kappa = (OVERFLOWS_PER_SECOND_TIMER1*60)/(STEPS_PER_X*((long) F));
		}else{
			kappa = (OVERFLOWS_PER_SECOND_TIMER1*60)/(STEPS_PER_X*((long) F)); /*По всем осям на один милиметр смещения приходится 5 шагов*/
		}
		
		/*if (F==10000) {PORTC ^= (1 << PC3);}*/
		/*if (translation_discret.e==100*STEPS_PER_E) {PORTC ^= (1 << PC3);}*/
	}
	/*
		Prepare variables
	*/
	progress.x = 0;
	progress.y = 0;
	progress.z = 0;
	progress.e = 0;
	/*
		Let's move!
	*/
	state0 = NEW_TASK;
}

void doStep(signed char motor){

	static uint8_t phases[] = {0, 0, 0, 0};
	int next_phase = 0;
	//PORTC |= (1 << PC2);
	if (motor > 0){
		next_phase = phases[motor - 1];
		if (next_phase == PHASE_LENGTH - 1){
			next_phase = 0;
		}else{
			next_phase++;
		}
		phases[motor - 1] = next_phase;
	}else{
		next_phase = phases[-1 - motor];
		if (next_phase == 0){
			next_phase = PHASE_LENGTH - 1;
		}else{
			next_phase--;
		}
		phases[-1 - motor] = next_phase;
	}
	/*PORTC |= (1 << PC1);*/
	switch(motor){
		case INC_X: 
			PORT_X &= CLEAR_X;
			PORT_X |= DRIVE_PHASES_X[next_phase];			
			break;
			
		case DEC_X: 
			PORT_X &= CLEAR_X;
			PORT_X |= DRIVE_PHASES_X[next_phase];
			break;		
		
		case INC_Y: 
			PORT_Y &= CLEAR_Y;
			PORT_Y |= DRIVE_PHASES_Y[next_phase];
			break;
		
		case DEC_Y: 
			PORT_Y &= CLEAR_Y;
			PORT_Y |= DRIVE_PHASES_Y[next_phase];
			break;

		case INC_Z: 
			PORT_Z &= CLEAR_Z;
			PORT_Z |= DRIVE_PHASES_Z[next_phase];
			break;

		case DEC_Z: 
			PORT_Z &= CLEAR_Z;
			PORT_Z |= DRIVE_PHASES_Z[next_phase];
			break;

		case INC_E: 
			PORT_E &= CLEAR_E;
			PORT_E |= DRIVE_PHASES_E[next_phase];
			break;

		case DEC_E: 
			PORT_E &= CLEAR_E;
			PORT_E |= DRIVE_PHASES_E[next_phase];
			break;
	}

}
