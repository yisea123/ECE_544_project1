/**
*
* @file PMod544IOR2.h
*
* @author Roy Kravitz (roy.kravitz@pdx.edu)
* @copyright Portland State University, 2014, 2015
*
* This header file contains identifiers and driver function definitions for the PMod544IO
* custom peripheral.  The peripheral provides access to the PMOD's required for ECE544.  These
*    PMODCLP - A 2 line by 16 character LCD with an 8-bit parallel interface
*    PMODENC - A rotary encoder with a pushbutton and slide switch. 
*
* The PMODCLP and PMODENC are both controlled by Picoblaze-based firmware in the peripheral. 
* The rotary encoder pushbutton and slide switch are debounced in hardware with an instance 
* of debounce.v (also used in the Nexys4IO custom peripheral). 
*
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a	rhk	12/20/14	First release of driver
* </pre>
*
* @note
* The function prototypes were made to be (mostly) compatible with the N4EIF driver API used
* in previous terms.  This was done to simplify conversion of existing code.
******************************************************************************/
#ifndef PMOD544IOR2_H
#define PMOD544IOR2_H

/************************** Include Files *****************************/
#include "xil_types.h"
#include "xil_io.h"
#include "xstatus.h"
#include "stdbool.h"
#include "PMod544IOR2_l.h"


/************************** Constant Definitions *****************************/

/** @name Bit Masks
*
* Bit masks for the PModECE544IO registers.
*
* All of the registers in the PModECE544IO periheral are 32-bits wide
*
* @{
*/

// bit masks for the ROTLCD_STS register
#define PMOD544IO_SELFTEST_MSK 		0x80000000
#define PMOD544IO_LCDBUSY_MSK		0x00008000
#define PMOD544IO_ENCBUSY_MSK		0x00000080
#define PMOD544IO_ENCSW_MSK			0x00000002
#define PMOD544IO_ENCBTN_MSK		0x00000001

// bit masks for the ROT_CNTRL register
#define PMOD544IO_CLRCNT_MSK 		0x00000080
#define PMOD544IO_LDCFG_MSK			0x00000040
#define PMOD544IO_NONEG_MSK			0x00000010
#define PMOD544IO_INCDECCNT_MSK		0x0000000F

// bit masks for the ROT_COUNT register
#define PMOD544IO_ROTENC_COUNT_MSK	0x0000FFFF
#define PMOD544IO_ROTENC_CNTLO_MSK	0x000000FF
#define PMOD544IO_ROTENC_CNTHI_MSK	0x0000FF00

// bit mask for the LCD_CMD register
#define PMOD544IO_LCD_DOCMD_MSK		0x00000080
#define PMOD544IO_LCD_CMD_MSK		0x0000001F

// bits masks for the LCD_DATA register
#define PMOD544IO_LCD_DATA_MSK		0x000000FF

/* @} */

/**************************** Type Definitions *****************************/
/** @name Literals and constants
*
* Literals and constants used for selecting specific devices
*
*/

// LCD Commands
enum _PMOD544IO_lcdcmds {
	LCD_NOP      = 0x00,	LCD_SETCURSOR = 0x01,	LCD_WRITECHAR = 0x02,	LCD_READCHAR  = 0x03,
	LCD_CLRD     = 0x04,	LCD_HOME      = 0x05,	LCD_SETCGADDR = 0x06,	LCD_SETDDADDR = 0x07,
	LCD_SETMODE  = 0x08,	LCD_SETONOFF  = 0x09,	LCD_SHIFTLEFT = 0x0A,	LCD_SHIFTRGHT = 0x0B,
	LCD_MOVELEFT = 0x0C,	LCD_MOVERGHT  = 0x0D,	LCD_RSVD00    = 0x0E,   LCD_RSVD01    = 0x0F
};


/***************** Macros (Inline Functions) Definitions ********************/


/************************** Function Prototypes *****************************/

// Initialization functions
int PMDIO_initialize(u32 BaseAddr);

// Rotary Encoder functions
int PMDIO_ROT_init(int inc_dec_cnt, bool no_neg);
int PMDIO_ROT_clear(void);
int PMDIO_ROT_readRotcnt(int* RotaryCnt);
bool PMDIO_ROT_isBtnPressed(void);
bool PMDIO_ROT_isSwOn(void);

// LCD base funtions
int PMDIO_LCD_docmd(u32 lcdcmd, u32 lcddata);
int PMDIO_LCD_setcursor(u32 row, u32 col);
int PMDIO_LCD_wrchar(char ch);
int PMDIO_LCD_shiftl(void);
int PMDIO_LCD_shiftr(void);
int PMDIO_LCD_setcgadr(u32 addr);
int PMDIO_LCD_setddadr(u32 addr);
int PMDIO_LCD_clrd(void);

// LCD string-related functions
char* PMDIO_LCD_itoa(int value, char *string, int radix);
int PMDIO_LCD_wrstring(char* s);
int PMDIO_LCD_puthex(u32 num);
int PMDIO_LCD_putnum(int num, int radix);
#endif // PMOD544IOR2_H
