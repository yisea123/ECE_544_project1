/**
*
* @file ece544periph_test.c
*
* @author Roy Kravitz (roy.kravitz@pdx.edu)
* @copyright Portland State University, 2014-2015, 2016
*
* This file implements a test program for the Pmod544IOR2 and Nexys4IO
* custom peripherals.  The peripherals provides access to the Nexys4 pushbuttons
* and slide switches, the LEDs, the RGB LEDs, and the Seven Segment display
* on the Digilent Nexys4 board and the PmodCLP and PmodENC connected to the PMOD
* expansion connectors on the Nexys4
*
* The test is basic but covers all of the API functions:
*	o initialize the Nexys4IO driver
*	o Set the LED's to different values
*	o Checks that the duty cycles can be set for both RGB LEDs
*	o Write character codes to the digits of the seven segment display banks
*	o Checks that all of the switches and pushbuttons can be read
*	o Performs a basic test on the rotary encoder and LCD drivers
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a	rhk	12/22/14	First release of test program.  Builds on the nx4io_test program.
* </pre>
*
* @note
* The minimal hardware configuration for this test is a Microblaze-based system with at least 32KB of memory,
* an instance of Nexys4IO, an instance of the PMod544IOR2,  and an instance of the Xilinx
* UARTLite (used for xil_printf() console output)
*
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "platform.h"
#include "xparameters.h"
#include "xstatus.h"
#include "Nexys4IO.h"
#include "PMod544IOR2.h"

/************************** Constant Definitions ****************************/
#define NX4IO_DEVICE_ID		XPAR_NEXYS4IO_0_DEVICE_ID
#define NX4IO_BASEADDR		XPAR_NEXYS4IO_0_S00_AXI_BASEADDR
#define NX4IO_HIGHADDR		XPAR_NEXYS4IO_0_S00_AXI_HIGHADDR

#define PMD544IO_DEVICE_ID	XPAR_PMOD544IOR2_0_DEVICE_ID
#define PMD544IO_BASEADDR	XPAR_PMOD544IOR2_0_S00_AXI_BASEADDR
#define PMD544IO_HIGHADDR	XPAR_PMOD544IOR2_0_S00_AXI_HIGHADDR

/**************************** Type Definitions ******************************/

/***************** Macros (Inline Functions) Definitions ********************/

/************************** Variable Definitions ****************************/
unsigned long timeStamp = 0;


/************************** Function Prototypes *****************************/
void usleep(u32 usecs);

int do_init_nx4io(u32 BaseAddress);
int do_init_pmdio(u32 BaseAddress);

void RunTest1(void);
void RunTest2(void);
void RunTest3(void);
void RunTest4(void);
void RunTest5(void);

/************************** MAIN PROGRAM ************************************/
int main()
{
    init_platform();

	int sts;

	xil_printf("ECE 544 Nexys4 Peripheral Test Program R1.0\n");
	xil_printf("By Roy Kravitz.  31-December 2014\n\n");

	// initialize the Nexys4 driver and (some of)the devices
	sts = do_init_nx4io(NX4IO_BASEADDR);
	if (sts == XST_FAILURE)
	{
		exit(1);
	}

	// initialize the PMod544IO driver and the PmodENC and PmodCLP
	sts = do_init_pmdio(PMD544IO_BASEADDR);
	if (sts == XST_FAILURE)
	{
		exit(1);
	}

	// TEST 1 - Test the LD15..LD0 on the Nexys4
	RunTest1();
	// TEST 2 - Test RGB1 (LD16) and RGB2 (LD17) on the Nexys4
	RunTest2();
	// TEST 3 - test the seven segment display banks
	RunTest3();
	// TEST 4 - test the rotary encoder (PmodENC) and display (PmodCLP)
	RunTest4();

	// TEST 5 - The main act, at last
	// test the switches and pushbuttons
	// We will do this in a busy-wait loop
	// pressing BTN_C (the center button will
	// cause the loop to terminate
	timeStamp = 0;

	xil_printf("Starting Test 4...the buttons and switch test\n");
	xil_printf("Press the center pushbutton to exit\n");

	// blank the display digits and turn off the decimal points
	NX410_SSEG_setAllDigits(SSEGLO, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK, CC_BLANK, CC_BLANK, CC_BLANK, DP_NONE);
	// loop the test until the user presses the center button
	while (1)
	{
		// Run an iteration of the test
		RunTest5();
		// check whether the center button is pressed.  If it is then
		// exit the loop.
		if (NX4IO_isPressed(BTNC))
		{
			// show the timestamp on the seven segment display and quit the loop
			NX4IO_SSEG_putU32Dec((u32) timeStamp, true);
			break;
		}
		else
		{
			// increment the timestamp and delay 100 msecs
			timeStamp += 100;
			usleep(00100000);
		}
	}

	xil_printf("\nThat's All Folks!\n\n");
	PMDIO_LCD_wrstring("That's All Folks");
	usleep(05000000);
	NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK, CC_B, CC_LCY, CC_E, DP_NONE);
	NX410_SSEG_setAllDigits(SSEGLO, CC_B, CC_LCY, CC_E, CC_BLANK, DP_NONE);
	PMDIO_LCD_clrd();
    cleanup_platform();
    exit(0);
}

/************************ TEST FUNCTIONS ************************************/

/****************************************************************************/
/**
* Test 1 - Test the LEDS (LD15..LD0)
*
* Checks the functionality of the LEDs API with some constant patterns and
* a sliding patter.  Determine Pass/Fail by observing the LEDs on the Nexys4
*
* @param	*NONE*
*
* @return	*NONE*
*
*****************************************************************************/
void RunTest1(void)
{
	u16 ledvalue;

	xil_printf("Starting Test 1...the LED test\n");
	// test the LEDS (LD15..LD0) with some constant patterns
	NX4IO_setLEDs(0x00005555);
	usleep(02000000);
	NX4IO_setLEDs(0x0000AAAA);
	usleep(02000000);
	NX4IO_setLEDs(0x0000FF00);
	usleep(02000000);
	NX4IO_setLEDs(0x000000FF);
	usleep(02000000);

	// shift a 1 through all of the leds
	ledvalue = 0x0001;
	do
	{
		NX4IO_setLEDs(ledvalue);
		usleep(01000000);
		ledvalue = ledvalue << 1;
	} while (ledvalue != 0);
	return;
}


/****************************************************************************/
/**
* Test 2 - Test the RGB LEDS (LD17..LD16)
*
* Checks the functionality of the RGB LEDs API with a fixed duty cycle.
* Determine Pass/Fail by observing the RGB LEDs on the Nexys4.
*
* @param	*NONE*
*
* @return	*NONE*
*
*****************************************************************************/
void RunTest2(void)
{
	// Test the RGB LEDS (LD17..LD16)
	xil_printf("Starting Test 2...the RGB LED test\n");

	// For RGB1 turn on only the blue LED (e.g. Red and Green duty cycles
	// are set to 0 but enable all three PWM channels
	NX4IO_RGBLED_setChnlEn(RGB1, true, true, true);
	NX4IO_RGBLED_setDutyCycle(RGB1, 0, 0, 16);
	usleep(03000000);

	// For RGB2, only write a non-zero duty cycle to the green channel
	NX4IO_RGBLED_setChnlEn(RGB2, true, true, true);
	NX4IO_RGBLED_setDutyCycle(RGB2, 0, 32, 0);
	usleep(03000000);

	// Next make RGB1 red. This time we'll only enable the red PWM channel
	NX4IO_RGBLED_setChnlEn(RGB1, true, false, false);
	NX4IO_RGBLED_setDutyCycle(RGB1, 64, 64, 64);
	usleep(05000000);

	// Next make RGB2 BRIGHTpurple-ish by only changing the duty cycle
	NX4IO_RGBLED_setDutyCycle(RGB2, 255, 255, 255);
	usleep(03000000);

	// Finish by turning the both LEDs off
	// We'll do this by disabling all of the channels without changing
	// the duty cycles
	NX4IO_RGBLED_setDutyCycle(RGB1, 0, 0, 0);
	NX4IO_RGBLED_setDutyCycle(RGB2, 0, 0, 0);

return;
}

/****************************************************************************/
/**
* Test 3 - Test the seven segment display
*
* Checks the seven segment display by displaying DEADBEEF and lighting all of
* the decimal points. Determine Pass/Fail by observing the seven segment
* display on the Nexys4.
*
* @param	*NONE*
*
* @return	*NONE*
*
*****************************************************************************/
void RunTest3(void)
{
	xil_printf("Starting Test 3...The seven segment display test\n");

	NX4IO_SSEG_putU32Hex(0xBEEFDEAD);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT7, true);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT6, true);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT5, true);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT4, true);
	usleep(05000000);
	NX4IO_SSEG_putU32Hex(0xDEADBEEF);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT3, true);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT2, true);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT1, true);
	NX4IO_SSEG_setDecPt(SSEGLO, DIGIT0, true);
	usleep(05000000);
	return;
}


/****************************************************************************/
/**
* Test 4 - Test the PmodENC and PmodCLP
*
* Performs some basic tests on the PmodENC and PmodCLP.  Includes the following tests
* 	1.	check the rotary encoder by displaying the rotary encoder
* 		count in decimal and hex on the LCD display.  Rotate the knob
* 		to change the values up or down.  The pushbuttons can be used
* 		as follows:
* 			o 	press the rotary encoder pushbutton to exit
* 			o 	press BtnUp to clear the count
* 			o 	press BtnR to change rotary encoder
* 				mode to "stop at zero".  This does not appear
* 				to be reversible - not sure why.
* 			o 	press BTNL to change the increment/decrement
* 				value.  Use sw[3:0] to set the new value
*	6.	display the string "357#&CFsw" on the LCD display.  These values
* 		were chosen to check that the bit order is correct.  The screen will
* 		clear in about 5 seconds.
* 	7.	display "Looks Good" on the LCD.  The screen will clear
* 		in about 5 seconds.
*
*
* @param	*NONE*
*
* @return	*NONE*
*
*****************************************************************************/
void RunTest4(void)
{
	u16 sw;
	int RotaryCnt, RotaryIncr;
	bool RotaryNoNeg;
	char s[] = " Exiting Test 4 ";

	xil_printf("Starting Test 4...The PmodCLP and PmodENC Test\n");
	xil_printf("Turn PmodENC shaft.  Rotary Encoder count is displayed\n");
	xil_printf("BTNU - clear count, BNTR - Toggle NoNeg flag, BTNL - Inc/Dec count is set to sw[3:0]\n");
	xil_printf("Press Rotary encoder shaft or BTNC to exit\n");

	// test the rotary encoder functions
	RotaryIncr = 1;
	RotaryNoNeg = false;
	PMDIO_ROT_init(RotaryIncr, RotaryNoNeg);
	PMDIO_ROT_clear();

	// Set up the display output
	PMDIO_LCD_clrd();
	PMDIO_LCD_setcursor(1,0);
	PMDIO_LCD_wrstring("Enc: ");
	PMDIO_LCD_setcursor(2,0);
	PMDIO_LCD_wrstring("Hex: ");

	while(1) {
		// check if the rotary encoder pushbutton or BTNC is pressed
		// exit the loop if either one is pressed.
		if (PMDIO_ROT_isBtnPressed()) {
			break;
		}

		if (NX4IO_isPressed(BTNC)) {
			break;
		}

		// check the buttons and perform the appropriate action
		if (NX4IO_isPressed(BTNU)) {	// clear the rotary count
			PMDIO_ROT_clear();
		}  // end clear the rotary count
		else if (NX4IO_isPressed(BTNR)) {	// toggle no-neg flag (may not be reliable)
			if (RotaryNoNeg) {  //No Neg was enabled
					PMDIO_ROT_init(RotaryIncr, false);
					RotaryNoNeg = false;
				}
				else {  // No Neg was disabled
					PMDIO_ROT_init(RotaryIncr, true);
					RotaryNoNeg = true;
				}
		} else if (NX4IO_isPressed(BTNL)) { // load Inc/Dec count from switches
			sw = NX4IO_getSwitches();
			RotaryIncr = sw & 0xF;
			PMDIO_ROT_init(RotaryIncr, RotaryNoNeg);
		}

			PMDIO_ROT_readRotcnt(&RotaryCnt);
			NX4IO_setLEDs(RotaryCnt);
			PMDIO_LCD_setcursor(1,4);
			PMDIO_LCD_wrstring("      ");
			PMDIO_LCD_setcursor(1,4);
			PMDIO_LCD_putnum(RotaryCnt, 10);  // display the whole count,
			PMDIO_LCD_setcursor(2,4);
			PMDIO_LCD_puthex(RotaryCnt);

			// display the count on the LEDs and seven segment display, too
			NX4IO_setLEDs(RotaryCnt);
			NX4IO_SSEG_putU32Dec(RotaryCnt, true);
	} // rotary button has been pressed - exit the loop
	xil_printf("\nPmodENC test completed\n");

	// Write some character to the screen to check the ASCII translation
 	PMDIO_LCD_clrd();
	PMDIO_LCD_wrchar('3');
	PMDIO_LCD_wrchar('5');
	PMDIO_LCD_wrchar('7');
	PMDIO_LCD_wrchar('#');
	PMDIO_LCD_wrchar('&');
	PMDIO_LCD_wrchar('C');
	PMDIO_LCD_wrchar('F');
	PMDIO_LCD_wrchar('s');
	PMDIO_LCD_wrchar('w');
	usleep(05000000);

	// Write one final string
	PMDIO_LCD_clrd();
	PMDIO_LCD_wrstring(s);

	return;
}



/****************************************************************************/
/**
* Test 5 - Test the pushbuttons and switches
*
* Copies the slide switch values to LEDs and the pushbuttons to the decimal
* points in SSEGLO.  Also shows shows the value of the switches on SSEGLO.
* Doing this not only tests the putU16HEX() function but also lets the user
* try all of the character codes (they are displayed on DIGIT7. Determine
* Pass/Fail by flipping switches and pressing
* buttons and seeing if the results are reflected in the LEDs and decimal points
*
* @param	*NONE*
*
* @return	*NONE*
*
* @note
* This function does a single iteration. It should be inclosed in a loop
* if you want to repeat the test
*
*****************************************************************************/
void RunTest5()
{
	u16 ledvalue;
	u8 btnvalue;
	u32 regvalue;
	u32 ssegreg;

	// read the switches and write them to the LEDs and SSEGLO
	ledvalue = NX4IO_getSwitches();
	NX4IO_setLEDs(ledvalue);
	NX4IO_SSEG_putU16Hex(SSEGLO, ledvalue);

	// write sw[4:0] as a character code to digit 7 so we can
	// check that all of the characters are displayed correctly
	NX4IO_SSEG_setDigit(SSEGHI, DIGIT7, (ledvalue & 0x1F));

	// read the buttons and write them to the decimal points on SSEGHI
	// use the raw get and put functions for the seven segment display
	// to test them and to keep the functionality simple.  We want to
	// ignore the center button so mask that out while we're at it.
	btnvalue = NX4IO_getBtns() & 0x01F;
	ssegreg = NX4IO_SSEG_getSSEG_DATA(SSEGHI);

	// shift the button value to bits 27:24 of the SSEG_DATA register
	// these are the bits that light the decimal points.
	regvalue = ssegreg & ~NEXYS4IO_SSEG_DECPTS_MASK;
	regvalue  |=  btnvalue << 24;

	// write the SSEG_DATA register with the new decimal point values
	NX4IO_SSEG_setSSEG_DATA(SSEGHI, regvalue);
	return;
}


/*********************** HELPER FUNCTIONS ***********************************/

/****************************************************************************/
/**
* insert delay (in microseconds) between instructions.
*
* This function should be in libc but it seems to be missing.  This emulation implements
* a delay loop with (really) approximate timing; not perfect but it gets the job done.
*
* @param	usec is the requested delay in microseconds
*
* @return	*NONE*
*
* @note
* This emulation assumes that the microblaze is running @ 100MHz and takes 15 clocks
* per iteration - this is probably totally bogus but it's a start.
*
*****************************************************************************/

static const u32	DELAY_1US_CONSTANT	= 15;	// constant for 1 microsecond delay

void usleep(u32 usec)
{
	volatile u32 i, j;

	for (i = 0; i < usec; i++)
	{
		for (j = 0; j < DELAY_1US_CONSTANT; j++);
	}
	return;
}


/****************************************************************************/
/**
* initialize the Nexys4 LEDs and seven segment display digits
*
* Initializes the NX4IO driver, turns off all of the LEDs and blanks the seven segment display
*
* @param	BaseAddress is the memory mapped address of the start of the Nexys4 registers
*
* @return	XST_SUCCESS if initialization succeeds.  XST_FAILURE otherwise
*
* @note
* The NX4IO_initialize() function calls the NX4IO self-test.  This could
* cause the program to hang if the hardware was not configured properly
*
*****************************************************************************/
int do_init_nx4io(u32 BaseAddress)
{
	int sts;

	// initialize the NX4IO driver
	sts = NX4IO_initialize(BaseAddress);
	if (sts == XST_FAILURE)
		return XST_FAILURE;

	// turn all of the LEDs off using the "raw" set functions
	// functions should mask out the unused bits..something to check w/
	// the debugger when we bring the drivers up for the first time
	NX4IO_setLEDs(0xFFF0000);
	NX4IO_RGBLED_setRGB_DATA(RGB1, 0xFF000000);
	NX4IO_RGBLED_setRGB_DATA(RGB2, 0xFF000000);
	NX4IO_RGBLED_setRGB_CNTRL(RGB1, 0xFFFFFFF0);
	NX4IO_RGBLED_setRGB_CNTRL(RGB2, 0xFFFFFFFC);

	// set all of the display digits to blanks and turn off
	// the decimal points using the "raw" set functions.
	// These registers are formatted according to the spec
	// and should remain unchanged when written to Nexys4IO...
	// something else to check w/ the debugger when we bring the
	// drivers up for the first time
	NX4IO_SSEG_setSSEG_DATA(SSEGHI, 0x0058E30E);
	NX4IO_SSEG_setSSEG_DATA(SSEGLO, 0x00144116);

	return XST_SUCCESS;

}


/****************************************************************************/
/**
* initialize the PmodCLP and PmodENC
*
* Initializes the PMod544IOR2 driver, configures the rotary encoder and displays a
* welcome message on the display
*
* @param	BaseAddress is the memory mapped address of the start of the Nexys4 registers
*
* @return	XST_SUCCESS if initialization succeeds.  XST_FAILURE otherwise
*
* @note
* The PMDIO_initialize() function calls the PMDIO self-test.  This could
* cause the program to hang if the hardware was not configured properly
*
*****************************************************************************/
int do_init_pmdio(u32 BaseAddress)
{
	int sts;

	// initialize the PMod544IOR2 driver
	sts = PMDIO_initialize(BaseAddress);
	if (sts == XST_FAILURE)
		return XST_FAILURE;

	// Write a greeting on the LCD
	PMDIO_LCD_wrstring("Pmods are Ready");

	return XST_SUCCESS;

}
