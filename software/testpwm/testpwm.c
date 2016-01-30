/* testpwm.c - PWM & PWDET application for ECE 544 Project #1

Author: Roy Kravitz (roy.kravitz@pdx.edu)
Modified by: Rehan Iqbal (riqbal@pdx.edu)
Organization: Portland State University

Description:

This program tests the Xilinx timer/counter PWM library for ECE 544.   The hardware for PWM is done with
a Xilinx Timer/Counter module set in PWM mode.   The PWM library builds on the Timer/Counter drivers
provided by Xilinx and encapsulates common PWM functions.  The program also provides a working example
of how to use the xps_s3eif driver to control the buttons, switches, rotary encoder, and display.

The test program uses the rotary encoder and switches to choose a PWM frequency and duty cycle.  The selected
frequency and duty cycle are displayed on line 1 of the LCD.   The program also illustrates the use of a Xilinx
fixed interval timer module to generate a periodic interrupt for handling time-based (maybe) and/or sampled inputs/outputs

Configuration Notes:

The minimal hardware configuration for this test is a Microblaze-based system with at least 32KB of memory,
an instance of Nexys4IO, an instance of the PMod544IOR2, an instance of an axi_timer, an instance of an axi_gpio
and an instance of an axi_uartlite (used for xil_printf() console output)

*/

/************************ Include Files **************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xparameters.h"
#include "xintc.h"
#include "xtmrctr.h"
#include "xgpio.h"
#include "mb_interface.h"
#include "platform.h"
#include "Nexys4IO.h"
#include "PMod544IOR2.h"
#include "pwm_tmrctr.h"

/************************** Constant Definitions ****************************/

// Clock frequencies

#define CPU_CLOCK_FREQ_HZ		XPAR_CPU_CORE_CLOCK_FREQ_HZ
#define AXI_CLOCK_FREQ_HZ		XPAR_CPU_M_AXI_DP_FREQ_HZ

// PWM and pulse detect timer parameters

#define PWM_TIMER_DEVICE_ID		XPAR_TMRCTR_0_DEVICE_ID

// Nexys4 I/O parameters

#define NX4IO_DEVICE_ID			XPAR_NEXYS4IO_0_DEVICE_ID
#define NX4IO_BASEADDR			XPAR_NEXYS4IO_0_S00_AXI_BASEADDR
#define NX4IO_HIGHADDR			XPAR_NEXYS4IO_0_S00_AXI_HIGHADDR

// Pmod544 I/O parameters

#define PMDIO_DEVICE_ID			XPAR_PMOD544IOR2_0_DEVICE_ID
#define PMDIO_BASEADDR			XPAR_PMOD544IOR2_0_S00_AXI_BASEADDR
#define PMDIO_HIGHADDR			XPAR_PMOD544IOR2_0_S00_AXI_HIGHADDR

// GPIO parameters

#define GPIO_0_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID
#define GPIO_0_INPUT_CHANNEL	1
#define GPIO_0_OUTPUT_CHANNEL	2

#define GPIO_1_DEVICE_ID		XPAR_AXI_GPIO_1_DEVICE_ID
#define GPIO_1_HIGH_COUNT		1
#define GPIO_1_LOW_COUNT		2									
		
// Interrupt Controller parameters

#define INTC_DEVICE_ID			XPAR_INTC_0_DEVICE_ID
#define FIT_INTERRUPT_ID		XPAR_MICROBLAZE_0_AXI_INTC_FIT_TIMER_0_INTERRUPT_INTR
#define PWM_TIMER_INTERRUPT_ID	XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR

// Fixed Interval timer - 100 MHz input clock, 40KHz output clock
// FIT_COUNT_1MSEC = FIT_CLOCK_FREQ_HZ * .001

#define FIT_IN_CLOCK_FREQ_HZ	CPU_CLOCK_FREQ_HZ
#define FIT_CLOCK_FREQ_HZ		40000
#define FIT_COUNT				(FIT_IN_CLOCK_FREQ_HZ / FIT_CLOCK_FREQ_HZ)
#define FIT_COUNT_1MSEC			40	

// PWM selected frequencies in Hertz

#define PWM_FREQ_10HZ			10
#define PWM_FREQ_100HZ			100
#define PWM_FREQ_1KHZ			1000
#define PWM_FREQ_5KHZ			5000
#define PWM_FREQ_10KHZ			10000
#define PWM_FREQ_50KHZ			50000
#define PWM_FREQ_100KHZ			100000
#define PWM_FREQ_200KHZ			200000
#define PWM_FREQ_500KHZ			500000
#define PWM_FREQ_1MHZ			1000000
#define PWM_FREQ_2MHZ			2000000
#define PWM_FREQ_5MHZ			5000000
#define PWM_FREQ_10MHZ			10000000

#define INITIAL_FREQUENCY		PWM_FREQ_1KHZ
#define INITIAL_DUTY_CYCLE		50
#define DUTY_CYCLE_CHANGE		5

#define	PWM_SIGNAL_MSK			0x01
#define CLKFIT_MSK				0x01
#define PWM_FREQ_MSK			0x03
#define PWM_DUTY_MSK			0xFF

/**************************** Type Definitions ******************************/


/***************** Macros (Inline Functions) Definitions ********************/

#define MIN(a, b)  ( ((a) <= (b)) ? (a) : (b) )
#define MAX(a, b)  ( ((a) >= (b)) ? (a) : (b) )

/************************** Variable Definitions ****************************/	

// Microblaze peripheral instances

XIntc 	IntrptCtlrInst;						// Interrupt Controller instance
XTmrCtr	PWMTimerInst;						// PWM timer instance
XGpio	GPIOInst0;							// GPIO instance - used for PWM duty & AXI Timer
XGpio	GPIOInst1;							// GPIO instance 1 - used by hw_detect


// The following variables are shared between non-interrupt processing and
// interrupt processing such that they must be global(and declared volatile)
// These variables are controlled by the FIT timer interrupt handler
// "clkfit" toggles each time the FIT interrupt handler is called so its frequency will
// be 1/2 FIT_CLOCK_FREQ_HZ.  timestamp increments every 1msec and is used in delay_msecs()


volatile unsigned int	clkfit;					// clock signal is bit[0] (rightmost) of gpio 0 output port									
volatile unsigned long	timestamp;				// timestamp since the program began

volatile u32			gpio_in;				// GPIO input port
volatile unsigned int	hw_high_count;			// high count from hw_detect on GPIO 1 (Channel 1)
volatile unsigned int	hw_low_count; 			// low count from hw_detect on GPIO 1 (Channel 2)

unsigned  int 			sw_high_count = 0;		// high count from sw detect in FIT interrupt routine	
unsigned  int 			sw_low_count = 0; 		// low count for sw detect in FIT interrupt routine


// The following variables are shared between the functions in the program
// such that they must be global

int						pwm_freq;			// PWM frequency 
int						pwm_duty;			// PWM duty cycle
bool					new_perduty;		// new period/duty cycle flag
				
/*---------------------------------------------------------------------------*/					
int						debugen = 0;		// debug level/flag
/*---------------------------------------------------------------------------*/


/************************** Function Prototypes ******************************/

int				do_init(void);															// initialize system
void			delay_msecs(unsigned int msecs);										// busy-wait delay for "msecs" miliseconds
void			update_lcd(int freq, int dutycycle, u32 linenum);						// update LCD display
				
void			FIT_Handler(void);														// fixed interval timer interrupt handler
unsigned int 	calc_freq(unsigned int high, unsigned int low, bool hw_switch); 		// calculates frequency from high & low counts
unsigned int	calc_duty(unsigned int high, unsigned int low);							// calculates duty cycle from high & low counts


/************************** MAIN PROGRAM ************************************/

int main() {

	XStatus 		status;
	u16				sw, oldSw =0xFFFF;				// 0xFFFF is invalid --> makes sure the PWM freq is updated 1st time
	int				rotcnt, oldRotcnt = 0x1000;	
	bool			done = false;
	bool 			hw_switch = 0;
	
	init_platform();

	// initialize devices and set up interrupts, etc.

	status = do_init();
	
	if (status != XST_SUCCESS) 	{

		PMDIO_LCD_setcursor(1,0);
		PMDIO_LCD_wrstring("****** ERROR *******");
		PMDIO_LCD_setcursor(2,0);
		PMDIO_LCD_wrstring("INIT FAILED- EXITING");
		exit(XST_FAILURE);
	}
	
	// initialize the global variables

	timestamp = 0;							
	pwm_freq = INITIAL_FREQUENCY;
	pwm_duty = INITIAL_DUTY_CYCLE;
	clkfit = 0;
	new_perduty = false;
	
	// start the PWM timer and kick of the processing by enabling the Microblaze interrupt

	PWM_SetParams(&PWMTimerInst, pwm_freq, pwm_duty);	
	PWM_Start(&PWMTimerInst);
	microblaze_enable_interrupts();
	
	// display the greeting   

	PMDIO_LCD_setcursor(1,0);
	PMDIO_LCD_wrstring("ECE544 Project 1");
	PMDIO_LCD_setcursor(2,0);
	PMDIO_LCD_wrstring(" by Rehan Iqbal ");
	NX4IO_setLEDs(0x0000FFFF);
	delay_msecs(2000);
	NX4IO_setLEDs(0x00000000);
		
   // write the static text to the display

	PMDIO_LCD_clrd();
	PMDIO_LCD_setcursor(1,0);
	PMDIO_LCD_wrstring("G|FR:    DCY:  %");
	PMDIO_LCD_setcursor(2,0);
	PMDIO_LCD_wrstring("D|FR:    DCY:  %");

	// turn off the LEDs and clear the seven segment display

	NX4IO_setLEDs(0x00000000);
	NX410_SSEG_setAllDigits(SSEGLO, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	  
	// main loop

	do	{ 
		
		// check rotary encoder pushbutton to see if it's time to quit
		
		if (PMDIO_ROT_isBtnPressed()) {
			done = true;
		}

		else {
			
			new_perduty = false;
			
			// get the switches and mask out all but the switches that determine the PWM timer frequency
			
			sw &= PWM_FREQ_MSK;
			sw = NX4IO_getSwitches();
			
			if (sw != oldSw) {	 
				
				// check the status of sw[2:0] and assign appropriate PWM output frequency

				switch (sw & 0x07) {
					
					case 0x00:	pwm_freq = PWM_FREQ_100HZ;	break;
					case 0x01:	pwm_freq = PWM_FREQ_1KHZ;	break;
					case 0x02:	pwm_freq = PWM_FREQ_10KHZ;	break;
					case 0x03:	pwm_freq = PWM_FREQ_50KHZ;	break;
					case 0x04:	pwm_freq = PWM_FREQ_100KHZ;	break;
					case 0x05:	pwm_freq = PWM_FREQ_500KHZ;	break;
					case 0x06:	pwm_freq = PWM_FREQ_1MHZ;	break;
					case 0x07:	pwm_freq = PWM_FREQ_5MHZ;	break;

				}
				
				// check the status of sw[3] and assign to global variable

				hw_switch = (sw & 0x08);

				// update global variable indicating there are new changes

				oldSw = sw;
				new_perduty = true;
			}
		
			// read rotary count and handle duty cycle changes
			// limit duty cycle to 0% to 99%
			
			PMDIO_ROT_readRotcnt(&rotcnt);

			if (rotcnt != oldRotcnt) {
				
				// show the rotary count in hex on the seven segment display
				
				NX4IO_SSEG_putU16Hex(SSEGLO, rotcnt);

				// change the duty cycle
				
				pwm_duty = MAX(1, MIN(rotcnt, 99));
				oldRotcnt = rotcnt;
				new_perduty = true;
			}

			// update generated frequency and duty cycle	
			
			if (new_perduty) {
				
				u32 			freq, 
								dutycycle;

				unsigned int 	detect_freq = 0x00;
				unsigned int 	detect_duty = 0x00;
			
				// set the new PWM parameters - PWM_SetParams stops the timer
				
				status = PWM_SetParams(&PWMTimerInst, pwm_freq, pwm_duty);
				
				if (status == XST_SUCCESS) {
					
					PWM_GetParams(&PWMTimerInst, &freq, &dutycycle);

					update_lcd(freq, dutycycle, 1);

					// check if sw[3] is high or low (HWDET / SWDET)
					// pass functions different args depending on which mode is selected

					if (hw_switch) {

						detect_freq = calc_freq(hw_high_count, hw_low_count, hw_switch);
						detect_duty = calc_duty(hw_high_count, hw_low_count);
					}

					else {

						detect_freq = calc_freq(sw_high_count, sw_low_count, hw_switch);
						detect_duty = calc_duty(sw_high_count, sw_low_count);
					}

					// update the LCD display with detected frequency & duty cycle

					update_lcd(detect_freq, detect_duty, 2);
										
					PWM_Start(&PWMTimerInst);
				}
			}
		}

	} while (!done);
	
	// wait until rotary encoder button is released	

	do {
		delay_msecs(10);
	} while (PMDIO_ROT_isBtnPressed());

	// we're done,  say goodbye

	xil_printf("\nThat's All Folks!\n\n");
	
	PMDIO_LCD_setcursor(1,0);
	PMDIO_LCD_wrstring("That's All Folks");
	PMDIO_LCD_setcursor(2,0);
	PMDIO_LCD_wrstring("                ");
	
	NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK, CC_B, CC_LCY, CC_E, DP_NONE);
	NX410_SSEG_setAllDigits(SSEGLO, CC_B, CC_LCY, CC_E, CC_BLANK, DP_NONE);

	delay_msecs(5000);

	// turn the lights out

	PMDIO_LCD_clrd();
	NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	NX410_SSEG_setAllDigits(SSEGLO, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);

	NX4IO_RGBLED_setDutyCycle(RGB1, 0, 0, 0);
	NX4IO_RGBLED_setChnlEn(RGB1, false, false, false);

	// exit gracefully

	cleanup_platform();

	exit(0);
}

/**************************** HELPER FUNCTIONS ******************************/
		
/* do_init - initializes the system
 
This function is executed once at start-up and after resets.  It initializes
the peripherals and registers the interrupt handler(s)

*/

int do_init(void) {

	int status;				// status from Xilinx Lib calls
	
	// initialize the Nexys4IO and Pmod544IO hardware and drivers
	// rotary encoder is set to increment from 0 by DUTY_CYCLE_CHANGE 
	
	status = NX4IO_initialize(NX4IO_BASEADDR);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	
	status = PMDIO_initialize(PMDIO_BASEADDR);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	
	// successful initialization.  Set the rotary encoder
	// to increment from 0 by DUTY_CYCLE_CHANGE counts per rotation

	PMDIO_ROT_init(DUTY_CYCLE_CHANGE, true);
	PMDIO_ROT_clear();
	
	
	// initialize the GPIO instances

	status = XGpio_Initialize(&GPIOInst0, GPIO_0_DEVICE_ID);
	
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	status = XGpio_Initialize(&GPIOInst1, GPIO_1_DEVICE_ID);
	
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// GPIO_0 channel 1 is an 8-bit input port.  bit[7:1] = reserved, bit[0] = PWM output (for duty cycle calculation)
	// GPIO_0 channel 2 is an 8-bit output port.  bit[7:1] = reserved, bit[0] = FIT clock

	XGpio_SetDataDirection(&GPIOInst0, GPIO_0_INPUT_CHANNEL, 0xFF);
	XGpio_SetDataDirection(&GPIOInst0, GPIO_0_OUTPUT_CHANNEL, 0xFE);

	// GPIO_1 channel 1 is a 32-bit input port - used to pass hw_detect 'high' count to application
	// GPIO_1 channel 2 is an 8-bit output port - used to pass hw_detect 'low' count to application
	
	XGpio_SetDataDirection(&GPIOInst1, GPIO_1_HIGH_COUNT, 0xFFFFFFFF);
	XGpio_SetDataDirection(&GPIOInst1, GPIO_1_LOW_COUNT, 0xFFFFFFFF);

	// initialize the PWM timer/counter instance but do not start it
	// do not enable PWM interrupts.  Clock frequency is the AXI clock frequency
	
	status = PWM_Initialize(&PWMTimerInst, PWM_TIMER_DEVICE_ID, false, AXI_CLOCK_FREQ_HZ);
	
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}
	
	// initialize the interrupt controller
	
	status = XIntc_Initialize(&IntrptCtlrInst, INTC_DEVICE_ID);
	
	if (status != XST_SUCCESS) {
	   return XST_FAILURE;
	}

	// connect the fixed interval timer (FIT) handler to the interrupt
	
	status = XIntc_Connect(&IntrptCtlrInst, FIT_INTERRUPT_ID, (XInterruptHandler)FIT_Handler, (void *)0);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}
 
	// start the interrupt controller such that interrupts are enabled for
	// all devices that cause interrupts

	status = XIntc_Start(&IntrptCtlrInst, XIN_REAL_MODE);
	
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// enable the FIT interrupt

	XIntc_Enable(&IntrptCtlrInst, FIT_INTERRUPT_ID);

	// set the duty cycles for RGB1.  The channels will be enabled/disabled
	// in the FIT interrupt handler.  Red and Blue make purple

	NX4IO_RGBLED_setDutyCycle(RGB1, 64, 0, 64);
	NX4IO_RGBLED_setChnlEn(RGB1, false, false, false);

	return XST_SUCCESS;
}
		
/****************************************************************************/

/* delay_msecs - delay execution for "n" msecs
 
Uses a busy-wait loop to delay execution.  Timing is approximate but we're not 
looking for precision here, just a uniform delay function.  The function uses the 
global "timestamp" which is incremented every msec by FIT_Handler().

Assumes that this loop is running faster than the fit_interval ISR 

If your program seems to hang it could be because the function never returns
Possible causes for this are almost certainly related to the FIT timer.  Check
your connections...is the timer clocked?  is it stuck in reset?  is the interrupt 
output connected? You would not be the first student to face this...not by a longshot 

*/

void delay_msecs(unsigned int msecs) {

	unsigned long target;

	if ( msecs == 0 ) {
		return;
	}

	target = timestamp + msecs;

	while (timestamp != target)
	{
		// spin until delay is over
	}
}
 
/****************************************************************************/

/* update_lcd - update the frequency/duty cycle LCD display
 
writes the frequency and duty cycle to the specified line.  Assumes the
static portion of the display is already written and the format of each
line of the display is the same.

freq is the  PWM frequency to be displayed

dutycycle is the PWM duty cycle to be displayed

linenum is the line (1 or 2) in the display to update

*/

void update_lcd(int freq, int dutycycle, u32 linenum) {

	PMDIO_LCD_setcursor(linenum, 5);
	PMDIO_LCD_wrstring("    ");
	PMDIO_LCD_setcursor(linenum, 5);

	// write the frequency 

	if (freq < 1000) {								// display Hz if frequency < 1KHz
		PMDIO_LCD_putnum(freq, 10);
	}

	else if (freq < 1000000) {						// display Hz if frequency < 1MHz
		PMDIO_LCD_putnum((freq / 1000), 10);
		PMDIO_LCD_wrstring("K");
	}

	else {											// otherwise, use the MHz suffix
		PMDIO_LCD_putnum((freq / 1000000), 10);
		PMDIO_LCD_wrstring("M");
	}

	// write the duty cycle

	PMDIO_LCD_setcursor(linenum, 13);
	PMDIO_LCD_wrstring("  %");
	PMDIO_LCD_setcursor(linenum, 13);
	PMDIO_LCD_putnum(dutycycle, 10);
}

/**************************** INTERRUPT HANDLERS ******************************/

/* FIT_Handler - Fixed interval timer interrupt handler 
  
updates the global "timestamp" every millisecond.  "timestamp" is used for the delay_msecs() function
and as a time stamp for data collection and reporting.  Toggles the FIT clock which can be used as a visual
indication that the interrupt handler is being called.  Also makes RGB1 a PWM duty cycle indicator

ECE 544 students - When you implement your software solution for pulse width detection in
Project 1 this could be a reasonable place to do that processing.

*/

void FIT_Handler(void) {
		
	static	unsigned int	ts_interval = 0;			// interval counter for incrementing timestamp
	static 	unsigned int	debug_count = 0; 			// counter used for debugging GPIO read

	static 	unsigned int	count = 0;					// used for sw counting high & low intervals

	static 	bool		 	prev_pwm = 0; 				// boolean to store previous PWM value (high / low)
	static 	bool		 	curr_pwm = 0; 				// boolean to store current PWM value (high / low)

	// toggle FIT clock

	clkfit ^= 0x01;
	XGpio_DiscreteWrite(&GPIOInst0, GPIO_0_OUTPUT_CHANNEL, clkfit);	

	// update timestamp	

	ts_interval++;	

	if (ts_interval > FIT_COUNT_1MSEC) {
		timestamp++;
		ts_interval = 1;
	}

	// Use an RGB LED (RGB1) as a PWM duty cycle indicator
	// we can read the current state of PWM out on GPIO[0] because we
	// fed it back around in the top level of our hardware design.
	// Note that this won't work well as the PWM frequency approaches
	// or exceeds 10KHz

	gpio_in = XGpio_DiscreteRead(&GPIOInst0, GPIO_0_INPUT_CHANNEL);
	curr_pwm = (gpio_in & 0x00000001);

	// use tri-color LED RGB1 as an indicator of PWM duty
	// this will breakdown at higher frequencies (e.g. higher than 10kHz)

	if (curr_pwm) {

		NX4IO_RGBLED_setChnlEn(RGB1, true, true, true);
	} 

	else {

		NX4IO_RGBLED_setChnlEn(RGB1, false, false, false);
	}

	// update HWDET high & low counts by reading GPIO

	hw_high_count = XGpio_DiscreteRead(&GPIOInst1, GPIO_1_HIGH_COUNT);
	hw_low_count  = XGpio_DiscreteRead(&GPIOInst1, GPIO_1_LOW_COUNT);

	// update the SWDET high & low counts through state machine
	// this detect low-to-high and high-to-low transitions
	// then places the count into one of two registers

	if (curr_pwm) {

		if (curr_pwm != prev_pwm) {
			sw_low_count = count;
			prev_pwm = curr_pwm;
			count = 0;
		}

		else {
			count += 1;
		}
	}

	else {

		if (curr_pwm != prev_pwm) {
			sw_high_count = count;
			prev_pwm = curr_pwm;
			count = 0;
		}

		else {
			count += 1;
			}
		}

	// debugging counts through terminal statements every ~ 3 sec:

/*	debug_count++;

	if (debug_count == 120000) {

		xil_printf("sw high count: %d \n", sw_high_count);
		xil_printf("sw low count: %d \n\n", sw_low_count);
		debug_count = 0;
	}*/
}

/****************************************************************************/

/* 	calc_freq - calculates frequency given counts for high & low intervals
 	
 	depending on sw[3] state, will use either CPU clock frequency or FIT Timer frequency
 	uses integer math only, so there may be some rounding error
*/

unsigned int calc_freq(unsigned int high, unsigned int low, bool hw_switch) {

	unsigned int sum;
	unsigned int frq;

	sum = (high + 1) + (low + 1);
	frq = hw_switch ? (CPU_CLOCK_FREQ_HZ / sum) : (FIT_CLOCK_FREQ_HZ / sum);

	return frq;
};

/****************************************************************************/

/* 	calc_duty - calculates duty cycle given counts for high & low intervals
 
  	uses integer math only, so there may be some rounding error
*/

unsigned int calc_duty(unsigned int high, unsigned int low) {

	unsigned int sum;
	unsigned int duty;

	sum = (high + 1) + (low + 1);
	duty = (100 * (high + 1)) / sum;

	return duty;
};