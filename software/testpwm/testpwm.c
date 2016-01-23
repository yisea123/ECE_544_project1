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

NOTE TO ECE 544 STUDENTS:  

This program could serve as a template for your Project 1 but may need to be customized to your system.
In particular the #defines that map the parameters may be specific to my xparameters.h, and could be different for yours	 

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

#define GPIO_DEVICE_ID			XPAR_AXI_GPIO_0_DEVICE_ID
#define GPIO_INPUT_CHANNEL		1
#define GPIO_OUTPUT_CHANNEL		2									
		
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

// PWM selected frequencies in Hz

#define PWM_FREQ_10HZ			10
#define PWM_FREQ_100HZ			100
#define PWM_FREQ_1KHZ			1000
#define PWM_FREQ_5KHZ			5000
#define PWM_FREQ_10KHZ			10000
#define PWM_FREQ_50KHZ			50000
#define PWM_FREQ_100KHZ			100000
#define PWM_FREQ_200KHZ			200000
#define PWM_FREQ_500KHZ			500000

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
XGpio	GPIOInst;							// GPIO instance

// The following variables are shared between non-interrupt processing and
// interrupt processing such that they must be global(and declared volatile)
// These variables are controlled by the FIT timer interrupt handler
// "clkfit" toggles each time the FIT interrupt handler is called so its frequency will
// be 1/2 FIT_CLOCK_FREQ_HZ.  timestamp increments every 1msec and is used in delay_msecs()

volatile unsigned int	clkfit;				// clock signal is bit[0] (rightmost) of gpio 0 output port									
volatile unsigned long	timestamp;			// timestamp since the program began

volatile u32			gpio_in;			// GPIO input port

// The following variables are shared between the functions in the program
// such that they must be global

int						pwm_freq;			// PWM frequency 
int						pwm_duty;			// PWM duty cycle
bool					new_perduty;		// new period/duty cycle flag
				
/*---------------------------------------------------------------------------*/					
int						debugen = 0;		// debug level/flag
/*---------------------------------------------------------------------------*/


/************************** Function Prototypes ******************************/

int		do_init(void);											// initialize system
void	delay_msecs(unsigned int msecs);						// busy-wait delay for "msecs" miliseconds
void	voltstostrng(float v, char* s);							// converts volts to a string
void	update_lcd(int freq, int dutyccyle, u32 linenum);		// update LCD display
				
void	FIT_Handler(void);										// fixed interval timer interrupt handler

/************************** MAIN PROGRAM ************************************/

int main() {

	XStatus 	status;
	u16			sw, oldSw =0xFFFF;		// 0xFFFF is invalid - makes sure the PWM freq is updated the first time
	int			rotcnt, oldRotcnt = 0x1000;	
	bool		done = false;
	
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
    PMDIO_LCD_wrstring("Vavg:           ");

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
				
				switch (sw) {
					
					case 0x00:	pwm_freq = PWM_FREQ_100HZ;	break;
					case 0x01:	pwm_freq = PWM_FREQ_1KHZ;	break;
					case 0x02:	pwm_freq = PWM_FREQ_5KHZ;	break;
					case 0x03:	pwm_freq = PWM_FREQ_10KHZ;	break;
					case 0x04:	pwm_freq = PWM_FREQ_50KHZ;	break;
					case 0x05:	pwm_freq = PWM_FREQ_100KHZ;	break;
					case 0x06:	pwm_freq = PWM_FREQ_200KHZ;	break;
					case 0x07:	pwm_freq = PWM_FREQ_500KHZ;	break;

				}
				
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
				
				pwm_duty = MAX(0, MIN(rotcnt, 99));
				oldRotcnt = rotcnt;
				new_perduty = true;
			}

			// update generated frequency and duty cycle	
			
			if (new_perduty) {
				
				u32 freq, dutycycle;
				float vavg;
				char  s[10];
			
				// set the new PWM parameters - PWM_SetParams stops the timer
				
				status = PWM_SetParams(&PWMTimerInst, pwm_freq, pwm_duty);
				
				if (status == XST_SUCCESS) {
					
					PWM_GetParams(&PWMTimerInst, &freq, &dutycycle);
					update_lcd(freq, dutycycle, 1);
					
					vavg = dutycycle * .01 * 3.3;
					
					voltstostrng(vavg, s);
					PMDIO_LCD_setcursor(2,5);
					PMDIO_LCD_wrstring(s); 
										
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

	PMDIO_LCD_clrd();
	NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	NX410_SSEG_setAllDigits(SSEGLO, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	NX4IO_RGBLED_setChnlEn(RGB1, false, false, false);

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
	
	
	// initialize the GPIO instance

	status = XGpio_Initialize(&GPIOInst, GPIO_DEVICE_ID);
	
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	// GPIO channel 1 is an 8-bit input port.  bit[7:1] = reserved, bit[0] = PWM output (for duty cycle calculation)
	// GPIO channel 2 is an 8-bit output port.  bit[7:1] = reserved, bit[0] = FIT clock
	
	XGpio_SetDataDirection(&GPIOInst, GPIO_OUTPUT_CHANNEL, 0xFE);
			
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

/* voltstostrng - converts volts to a fixed format string
 
accepts an float voltage reading and turns it into a 5 character string
of the following format:

	(+/-)x.yy 
 
where (+/-) is the sign, x is the integer part of the voltage and yy is
the decimal part of the voltage.

'v' is the voltage to convert; 's' is a pointer to the buffer receiving the converted string
	
Assumes that s points to an array of at least 6 bytes. No error checking is done

*/

void voltstostrng(float v, char* s) {

	float	dpf, ipf;
	u32		dpi;	
	u32		ones, tenths, hundredths;

	// form the fixed digits 

	dpf = modff(v, &ipf);
	dpi = dpf * 100;
	ones = abs(ipf) + '0';
	tenths = (dpi / 10) + '0';
	hundredths = (dpi - ((tenths - '0') * 10)) + '0';
	 
	// form the string and return

	if (ipf == 0 && dpf == 0) {
		*s++ = ' ';
	}

	else { 
		*s++ = ipf >= 0 ? '+' : '-';
	}

	*s++ = (char) ones;
	*s++ = '.';
	*s++ = (char) tenths;
	*s++ = (char) hundredths;
	*s   = 0;

	return;
};
 
/****************************************************************************/

/* update_lcd - update the frequency/duty cycle LCD display
 
writes the frequency and duty cycle to the specified line.  Assumes the
static portion of the display is already written and the format of each
line of the display is the same.

freq is the  PWM frequency to be displayed

dutycycle is the PM duty cycle to be displayed

linenum is the line (1 or 2) in the display to update

*/

void update_lcd(int freq, int dutycycle, u32 linenum) {

	PMDIO_LCD_setcursor(linenum, 5);
	PMDIO_LCD_wrstring("    ");
	PMDIO_LCD_setcursor(linenum, 5);

	if (freq < 1000) {							// display Hz if frequency < 1Khz
		PMDIO_LCD_putnum(freq, 10);
	}

	else {										// display frequency in KHz
		PMDIO_LCD_putnum((freq / 1000), 10);
		PMDIO_LCD_wrstring("K");
	}

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
		
	static	int			ts_interval = 0;			// interval counter for incrementing timestamp

	// toggle FIT clock

	clkfit ^= 0x01;
	XGpio_DiscreteWrite(&GPIOInst, GPIO_OUTPUT_CHANNEL, clkfit);	

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

	gpio_in = XGpio_DiscreteRead(&GPIOInst, GPIO_INPUT_CHANNEL);

	if (gpio_in & 0x00000001) {
		NX4IO_RGBLED_setChnlEn(RGB1, true, true, true);
	} 

	else {
		NX4IO_RGBLED_setChnlEn(RGB1, false, false, false);
	}

}