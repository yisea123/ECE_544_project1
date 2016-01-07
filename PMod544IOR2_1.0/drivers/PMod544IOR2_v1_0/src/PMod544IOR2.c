/**
*
* @file PMod544IOR2.c
*
* @author Roy Kravitz (roy.kravitz@pdx.edu)
* @copyright Portland State University, 2014, 2015
*
* This file implements the driver functions for the PModECE544IO
* custom peripheral.  The peripheral provides access to the PMOD's required for ECE544.  These
*    PMODCLP - A 2 line by 16 character LCD with an 8-bit parallel interface
*    PMODENC - A rotary encoder with a pushbutton and slide switch. 
*
* The PMODCLP and PMODENC are both controlled by Picoblaze-based firmware in the peripheral. 
* The rotary encoder pushbutton and slide switch are debounced in hardware with an instance 
* of debounce.v (also used in the Nexys4IO custom peripheral). 
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a	rhk	12/20/14	First release of driver
* </pre>
*
******************************************************************************/

/***************************** Include Files *******************************/
#include "PMod544IOR2.h"

/************************** Constant Definitions ****************************/

/**************************** Type Definitions ******************************/

/***************** Macros (Inline Functions) Definitions ********************/

/************************** Variable Definitions ****************************/
static u32 PMDIO_BaseAddress;	// Base Address of the NEXYS4IO register set
static bool IsReady = false;	// True if the driver has been initialized successfully

/************************** Function Prototypes *****************************/
void PMDIO_usleep(u32 usecs);

/************************** Driver Functions ********************************/

/****************** INITIALIZATION AND CONFIGURATION ************************/

/****************************************************************************/
/**
* Initialize the PModECE544IO peripheral driver
*
* Saves the Base address of the PModECE544IO registers and runs the selftest
* (only the first time the peripheral is initialized). If the self-test passes
* the function sets the rotary encoder mode and clears the rotary encoder count.
* Finishes off by clearing the LCD.
* 
* @param	BaseAddr is the base address of the NEXYS4IO register set
*
* @return
* 		- XST_SUCCESS	Initialization was successful.
*
* @note		This function can hang if the peripheral was not created correctly
* @note		The Base Address of the PmodECE544IO peripheral will be in xparameters.h
*****************************************************************************/
int PMDIO_initialize(u32 BaseAddr)
{
	XStatus sts;
	u32 rslt;
	
	// give interface 20ms to start up
	PMDIO_usleep(20000);
	
	// Save the Base Address so we know where to point the driver
	// Uses the global variable BaseAddr to store it.
	PMDIO_BaseAddress = BaseAddr;
	
	// Run the driver self-test.  Return on failure if it doesn't pass
	// We only do this the first time the peripheral is initialized
	if (!IsReady)
	{
		sts = PMOD544IO_Reg_SelfTest(PMDIO_BaseAddress);
		if (sts != XST_SUCCESS)
		{
			return XST_FAILURE;
		}
	}
	IsReady = true;
	
	// wait until peripheral self-test is complete
	// this is indicated by the SELFTEST bit in the ROTLCD_STS register
	rslt = PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	while ((rslt & PMOD544IO_SELFTEST_MSK) != 0)
	{  
		PMDIO_usleep(1000);  // wait 1ms and then try again
		rslt = PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	}
	
	// initialize the rotary encoder to incr/decr by 1, OK to go negative
	PMDIO_ROT_init(1, false);
	PMDIO_ROT_clear();
	
	// clear the LCD display
	PMDIO_LCD_clrd();
	PMDIO_usleep(10);
	
	return sts;
}


/*************************** ROTARY ENCODER  ********************************/

/****************************************************************************/
/**
* Initialize the Rotary Encoder control logic
*
* Configures the rotary encoder logic
*
* @param	inc_dec_cnt is the count for how much the rotary encorder increments
*			or decrements each time the rotary encoder is turned.  The count is
*			truncated to 4 bits
* @param	no_neg permits or prevents the rotary count from going below 0.,  no_neg
*			true say do not allow negative counts.
*
* @return	XST_SUCCESS
*
* @note
* Although it should be possible to change the configuration of the rotary encoder
* logic dynamically this is not recommended.  There is a subtle bug that prevents
* consistent results.  So it's best to cnly configure the rotary encoder logic during
* initialization.  You could, of course, try it but caveat emptor.
*****************************************************************************/
int PMDIO_ROT_init(int inc_dec_cnt, bool no_neg)
{
	int sts;
	u32 enc_state;

	// wait until rotary encoder control is ready - e.g. BUSY bit is 0	
	sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	while ((sts & PMOD544IO_ENCBUSY_MSK) != 0)
	{  
		PMDIO_usleep(10);
		sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	}
	
	// build the new rotary encoder control state
	enc_state = (inc_dec_cnt & PMOD544IO_INCDECCNT_MSK);
	if (no_neg)
		enc_state |= PMOD544IO_NONEG_MSK;
	
	// kick off the command by writing 1 to	"Load State" bit
	enc_state = (enc_state | PMOD544IO_LDCFG_MSK);
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_ROT_CNTRL_OFFSET, enc_state);
	
	// wait until command is complete - e.g. BUSY bit is 0 
	do  
	{
		PMDIO_usleep(10);
		sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	} while ((sts & PMOD544IO_ENCBUSY_MSK) != 0);
	
	// end the command by toggling (writing 0) to "Load State" bit
	// and waiting a bit (5us) to make sure rotary encoder controller "sees" the falling edge
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_ROT_CNTRL_OFFSET, (enc_state ^ PMOD544IO_LDCFG_MSK));
	PMDIO_usleep(10);
	
	return XST_SUCCESS;
}


/****************************************************************************/
/**
* Clear the rotary encoder count
*
* Sets the rotary encoder count back to 0
*
* @param	NONE
*
* @return	XST_SUCCESS
*****************************************************************************/
int PMDIO_ROT_clear(void)
{
	int sts;
	u32 enc_state;

	// wait until rotary encoder control is ready - e.g. BUSY bit is 0	
	sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	while ((sts & PMOD544IO_ENCBUSY_MSK) != 0)
	{  
		PMDIO_usleep(10);
		sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	}
	
	// kick off the command by writing 1 to	"Clear Count bit
	enc_state = PMOD544IO_CLRCNT_MSK;
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_ROT_CNTRL_OFFSET, enc_state );
	
	// wait until command is complete - e.g. BUSY bit is 0 
	do  
	{
		PMDIO_usleep(10);
		sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	} while ((sts & PMOD544IO_ENCBUSY_MSK) != 0);
	
	// end the command by toggling (writing 0) the Clear Count bit
	// and waiting a bit (5us) to make sure rotary encoder controller "sees" the falling edge
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_ROT_CNTRL_OFFSET, (enc_state ^ PMOD544IO_CLRCNT_MSK));
	PMDIO_usleep(10);
	
	return XST_SUCCESS;
}


/****************************************************************************/
/**
* Read the rotary encoder count into the u32 variable pointed to by rotaryCnt_p
*
* Returns the rotary count.  The rotary count is a 16-bit unsigned integer returned
* to an unsigned int.
*
* @param	rotaryCnt_p is a pointer to the u32 variable that will receive the rotary count
*
* @return	XST_SUCCESS
*****************************************************************************/
PMDIO_ROT_readRotcnt(int* rotaryCnt_p)
{
	u32 count;
	
	count = PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROT_COUNT_OFFSET);
	*rotaryCnt_p = count;
	PMDIO_usleep(10);
	
	return XST_SUCCESS;
}


/****************************************************************************/
/**
* returns the state of the rotary encoder pusbbutton
*
* Reads the ROTLCD_STS register to determine whether the rotary encoder shaft
* pushbutton is pressed
*
* @param	NONE
*
* @return	true if the button is pressed, false otherwise
*****************************************************************************/
bool PMDIO_ROT_isBtnPressed(void)
{
	u32 rot_sts;
	
	rot_sts = PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	
	return ((rot_sts & PMOD544IO_ENCBTN_MSK) != 0) ? true : false;
}


/****************************************************************************/
/**
* returns the state of the slide switch on the PmodENC
*
* Reads the ROTLCD_STS register to determine whether the slide switch is on (up) 
*
* @param	NONE
*
* @return	true if the slide switch is on, false otherwise
*****************************************************************************/
bool PMDIO_ROT_isSwOn(void)
{
	u32 rot_sts;
	
	rot_sts = PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	
	return ((rot_sts & PMOD544IO_ENCSW_MSK) != 0) ? true : false;
}


/*********************** LCD BASE FUNCTIONS  ***************************/

/****************************************************************************/
/**								
* Execute an LCD command
*
* Executes the LCD command in "lcdcmd" using the data in "lcdata".  Controls
* the handshaking between the driver and the peripheral.
*
* @param  lcdcmd ia rhw LCD command to execute
* @param  lcddata is the data for the command.  Not all commands have data
*
* @return XST_SUCCESS
*
* @note
* only the lower 8-bits of lcdcmd and lcdata are used
*****************************************************************************/
int PMDIO_LCD_docmd(u32 lcdcmd, u32 lcddata)
{
	int sts;

	// wait until LCD controller is ready - e.g. BUSY bit is 0	
	sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
sts = 0;
	while ((sts & PMOD544IO_LCDBUSY_MSK) != 0)
	{  
		PMDIO_usleep(10);
		sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
	}
	
	// write the LCD data to the LCD controller
	lcddata &= PMOD544IO_LCD_DATA_MSK;
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_LCD_DATA_OFFSET, lcddata);

	// write the LCD command into bits[4:0] of the LCD Command register
	lcdcmd &= PMOD544IO_LCD_CMD_MSK;
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_LCD_CMD_OFFSET, lcdcmd);
		
	// kick off the command by writing 1 to	"Do LCD command" bit
	lcdcmd |= PMOD544IO_LCD_DOCMD_MSK;
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_LCD_CMD_OFFSET, lcdcmd);
	
	// wait until command is complete - e.g. BUSY bit is 0 
	do  
	{
		PMDIO_usleep(10);
		sts = 	PMOD544IO_mReadReg(PMDIO_BaseAddress, PMOD544IO_ROTLCD_STS_OFFSET);
sts = 0;
	} while ((sts & PMOD544IO_LCDBUSY_MSK) != 0);
	
	// end the command by toggling (writing 0) to "Do LCD command" bit
	// and waiting at least 1.53ms to make sure LCD display controller "sees" the falling edge
	// and that the LCD has finished the operations.  1.53ms is the worst case command timing
	lcdcmd ^= PMOD544IO_LCD_DOCMD_MSK;
	PMOD544IO_mWriteReg(PMDIO_BaseAddress, PMOD544IO_LCD_CMD_OFFSET, lcdcmd);
	PMDIO_usleep(2000);
	
	return XST_SUCCESS;
}


/****************************************************************************/
/**								
 * Position the LCD cursor at {line, col}
 *
 * Position the cursor at the specified position.  The next
 * character will be written there.
 *
 * The display is formed of 2 lines of 16 characters and each
 * position has a corresponding address as indicated below.
 *
 *                   Character position
 *           0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
 * Line 1 - 80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F
 * Line 2 - C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF
 *
 * @param line is the row on the display to set the cursor to.  There are two lines
 * @param col in the character position on the line.  There are 16 characters per line
 *
 * @return XST_SUCCESS
 *****************************************************************************/
int PMDIO_LCD_setcursor(u32 row, u32 col)
{
	u32 pos;
	int sts;
	 
	pos = ((row & 0x0000000F) << 4) | (col & 0x0000000F);
	sts = PMDIO_LCD_docmd(LCD_SETCURSOR, pos);
	 
	return sts;
}


/****************************************************************************/
/**
* Write a character to the LCD display
*
* Writes an ASCII character to the LCD display at the current cursor position
*
* @param ch is the character to write to the display
*
* @return XST_SUCCESS
*****************************************************************************/
int PMDIO_LCD_wrchar(char ch)
{
	 int sts;
	 
	 sts = PMDIO_LCD_docmd(LCD_WRITECHAR, ch);
	 
	 return sts;
}



/****************************************************************************/
/**
* Shifts the entire display left one position
*
* Shifts the display left one position without changing display RAM contents.
* When the displayed data is shifted repeatedly, both lines move horizontally.
* The second display line does not shift into the first display line.
*
* @param NONE
*
* @return XST_SUCCESS
*****************************************************************************/
int PMDIO_LCD_shiftl(void)
{
	int sts;
	
	 sts = PMDIO_LCD_docmd(LCD_SHIFTLEFT, 0);
	
	return sts;
}


/****************************************************************************/
/**
* Shifts the entire display right one position
*
* Shifts the display right one position without changing display RAM contents.
* When the displayed data is shifted repeatedly, both lines move horizontally.
* The second display line does not shift into the first display line.
*
* @param NONE
*
* @return XST_SUCCESS
*****************************************************************************/
int PMDIO_LCD_shiftr(void)
{
	int sts;
	
	 sts = PMDIO_LCD_docmd(LCD_SHIFTRGHT, 0);
	
	return sts;
}


/****************************************************************************/
/**
* Set the character generator RAM Address
*
* Sets the CG RAM address to the value in "addr".  The function
* also serves the purpose of telling the LCD controller that
* the character data should be written to the character generator RAM instead
* of the data RAM.  The character generator RAM contains 8 user defined custom
* characters
*
* @param addr is the address in tha character generator ROM
*
* @return XST_SUCCESS
*
* @note:  only the low order 6-bits of the address are used
*****************************************************************************/
int PMDIO_LCD_setcgadr(u32 addr)
{
	int sts;
	
	sts = PMDIO_LCD_docmd(LCD_SETCGADDR, addr);
	
	return sts;
}


/****************************************************************************/
/**
* LCD_setddadr() - Set the data RAM Address
*
* Sets the data RAM address to the value in "addr".  The function
* also serves the purpose of telling the LCD controller that
* the character data should be written to the display RAM instead
* of the character generator RAM.  
*
* @param addr is the address in the diplay RAM
*
* @return XST_SUCCESS
*
* @note use LCD_setcursor() to set the position using {row, col} addressing
* @note only the low order 7-bits of the address are used
*****************************************************************************/
int PMDIO_LCD_setddadr(u32 addr)
{
	int sts;
	
	sts = PMDIO_LCD_docmd(LCD_SETDDADDR, addr);
	
	return sts;
}


/****************************************************************************/
/**
* LCD_clrd() - Clear the display
*
* Writes blanks to the display and returns the cursor home.
*
* @param  NONE
*
* @return XST_SUCCESS
*
*****************************************************************************/
int PMDIO_LCD_clrd(void)
{
	int sts;
	
	sts = PMDIO_LCD_docmd(LCD_CLRD, 0);
	
	return sts;
}



/****************** LCD STRING-RELATED FUNCTIONS  **********************/

/****************************************************************************/
/**
* Converts an integer to ASCII characters
*
* algorithm borrowed from ReactOS system libraries
*
* Converts an integer to ASCII in the specified base.  Assumes string[] is
* long enough to hold the result plus the terminating null
*
* @param 	value is the integer to convert
* @param 	*string is the address of buffer large enough to hold the converted number plus
*  			the terminating null
* @param	radix is the base to use in conversion, 
*
* @return  pointer to the return string
*
* @note
* No size check is done on the return string size.  Make sure you leave room
* for the full string plus the terminating null
*****************************************************************************/
char* PMDIO_LCD_itoa(int value, char *string, int radix)
{
	char tmp[33];
	char *tp = tmp;
	int i;
	u32 v;
	int sign;
	char *sp;

	if (radix > 36 || radix <= 1)
	{
		return 0;
	}

	sign = (radix == 10 && value < 0);
	if (sign)
		v = -value;
	else
		v = (u32) value;
  	while (v || tp == tmp)
  	{
		i = v % radix;
		v = v / radix;
		if (i < 10)
			*tp++ = i+'0';
		else
			*tp++ = i + 'a' - 10;
	}
	sp = string;
	
	if (sign)
		*sp++ = '-';

	while (tp > tmp)
		*sp++ = *--tp;
	*sp = 0;
	
  	return string;
}


/****************************************************************************/
/**
* Write a 0 terminated string to the LCD display
*
* Writes the null terminated string "s" to the LCD display
* starting at the current cursor position
*
* @param *s is a pointer to the string to display
*
* @return XST_SUCCESS
*
* @note
* No size checking is done to make sure the string will fit into a single line,
* or the entire display, for that matter.  Watch your string sizes.
*****************************************************************************/
int PMDIO_LCD_wrstring(char* s)
{
	int sts;
	int i = 0;
	
 	while (s[i]!='\0')
 	{
		sts = PMDIO_LCD_wrchar(s[i]);
		if (sts != XST_SUCCESS)
			break;
		i++;
 	}
		
	return sts;
}


/****************************************************************************/
/**
* Write a 32-bit unsigned hex number to LCD display in Hex
*       
* Writes  32-bit unsigned number to the LCD display starting at the current
* cursor position.
*
* @param num is the number to display as a hex value
*
* @return  XST_SUCCESS
*
* @note
* No size checking is done to make sure the string will fit into a single line,
* or the entire display, for that matter.  Watch your string sizes.
*****************************************************************************/ 
int PMDIO_LCD_puthex(u32 num)
{
  char  buf[9];
  int   cnt;
  char  *ptr;
  int   digit;
  XStatus	sts;
  
  ptr = buf;
  for (cnt = 7; cnt >= 0; cnt--) {
    digit = (num >> (cnt * 4)) & 0xF;
    
    if (digit <= 9)
      *ptr++ = (char) ('0' + digit);
    else
      *ptr++ = (char) ('a' - 10 + digit);
  }

  *ptr = (char) 0;
  PMDIO_LCD_wrstring(buf);
  
  return sts;
}


/****************************************************************************/
/**
* Write a 32-bit number in Radix "radix" to LCD display
*
* Writes a 32-bit number to the LCD display starting at the current
* cursor position. "radix" is the base to output the number in.
*
* @param num is the number to display
*
* @param radix is the radix to display number in
*
* @return XST_SUCCESS
* @note
* No size checking is done to make sure the string will fit into a single line,
* or the entire display, for that matter.  Watch your string sizes.
*****************************************************************************/ 
int PMDIO_LCD_putnum(int num, int radix)
{
  char  buf[16];
  int sts;
  
  PMDIO_LCD_itoa(num, buf, radix);
  sts = PMDIO_LCD_wrstring(buf);
  
  return sts;
}




/************************** Helper Functions ***************************/

/****************************************************************************/
/**
* Insert delay (in microseconds) between instructions.
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

void PMDIO_usleep(u32 usec)
{
	volatile u32 i, j;
	
	for (i = 0; i < usec; i++)
	{
		for (j = 0; j < DELAY_1US_CONSTANT; j++);
	}
	return;
} 
