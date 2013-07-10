/*
 * IR Decoder
 * Decode and respond to RC5 IR remote control commands, using a TSOP38238SS1V receiver IC
 * Copyright: Jerry Pommer <jerry@jerrypommer.com> 2012-12-04
 * License: GNU General Public License v3
 */

#include <msp430.h>

#define IRDATA BIT4             // IR receiver on P1.4
#define RED BIT0				// Red LED on P1.0
#define GREEN BIT6				// Green LED on P1.6
#define MOTOR BIT7
#define T_INTERVAL 2450			// Timing between IRDATA samples - should be closer to 1778 with 1MHz clock, but mine runs fast. YMMV
#define DEBUG_PIN BIT3

int data = 0;                   // Hold the incoming IRDATA stream
int command = 0;                // The command received
int address = 0;                // The device address received
int toggle = 0;                 // The toggle bit from the IRDATA stream

void main(void) {
	
	WDTCTL = WDTPW | WDTHOLD;    			// disable watchdog

	/*
	 * Set the system clock to use the internal DCO.
	 * Not very accurate, so adjust T_INTERVAL to get the timing right.
	 */
	DCOCTL = CALDCO_1MHZ;     				// select DCO approx. 1MHz
	BCSCTL1 |= CALBC1_1MHZ;
	BCSCTL2 &= ~SELS;		  				// SMCLK source

	/*
	 * Set up the I/O pins
	 */
	P1REN |= DEBUG_PIN ;            			// pullup resistor for timing debug pin
	P1OUT |= DEBUG_PIN | RED | GREEN;		// Set all these HIGH
	P1DIR |= DEBUG_PIN | RED | GREEN;		// Set all these as outputs
	P1DIR |= MOTOR;
	P1OUT &= ~MOTOR;
	P1DIR &= ~IRDATA;						// IRDATA is an input
	P1IE = IRDATA;							// enable interrupts, watching IRDATA for a change
	P1IES = IRDATA;							// watch for falling edge change on IRDATA

	TACCR0 = T_INTERVAL;					// Set the timing interval for TimerA
	__bis_SR_register(LPM0_bits + GIE);		// Go to sleep until Port1 interrupt happens
}


#pragma vector = PORT1_VECTOR;
void __interrupt Port_1(void)
{
	P1IE &= ~IRDATA;				// Turn off P1 interrupts while we work
	P1IFG &= ~IRDATA;				// clear the P1 interrupt flag for IRDATA
	data = 1;  						// first start bit, inverted from receiver
				//TODO: wait for second start bit within T_INTERVAL; abandon if it doesn't come.
	
	// start timer
    TACCTL0 |= CCIE;				// enable timer interrupts
	TACTL = TASSEL_2;   			// TimerA0 clock selection - SMCLK
	TACCR0 &= ~CCIFG;				// clear any pending timerA interrrupt flags
	TACTL |= MC_1;					// start the timer in UP mode
}

#pragma vector = TIMERA0_VECTOR;
void __interrupt Timer0(void)
{
	/*
	 * Need to track the state of a few things... the last toggle bit, the last command issued,
	 * the count of how many bits we have parsed.
	 */
	static int lastToggle;
	static int lastCommand;
	static char repeat = 0;
	static int count = 0;			// stop after all the bits have been read in

	TACCTL0 &= ~CCIE;				// clear the interrupt flag

	if (count >= 13) {				// all bits read in and shifted into the data variable

		TACTL &= MC_0;				// stop the timer
		TACTL |= TACLR;				// clear the timer settings to zero it out

		command = data & 0x3F;		
		address = (data >> 6) & 0x1F ;
		toggle = (data >> 11) & 1;

		P1IE |= IRDATA;				// we have our IR command, reset and start listening for the next one
		P1IFG &= ~IRDATA;
		count = 0;

		if (lastCommand == command && lastToggle == toggle) {		// if we only want one toggle per keypress
			repeat = 1;
		} else {
			lastCommand = command;
			lastToggle = toggle;
			repeat = 0;
		}
		
		switch (command) {			// do whatever we want to do with the incoming commands
			case 0:
				if (!repeat)
					P1OUT ^= RED;		// toggle the red LED when the 0 button is pressed
				break;
			case 1:
				if (!repeat)
					P1OUT ^= GREEN;		// toggle the green LED when the 1 button is pressed
				break;
			case 7:
				if (!repeat)
					P1OUT ^= MOTOR;
				break;
		}
	} else {
		// read IRDATA and store the value in DATA
		//P1OUT &= ~DEBUG_PIN;		// uncomment this and the one a few lines down to see P1.3 toggle with each sample of IRDATA. Use a two channel oscilloscope to adjust T_INTERVAL as necessary.
		if (data > 0) data <<= 1;	// shift left one bit
		if ((P1IN & IRDATA) != IRDATA) {  // invert IRDATA if it is low, because the receiver is active low - append 'data' with a 1
			data |= 1;
		}
		//P1OUT |= DEBUG_PIN;       // as above, uncomment for timing debug use
		count++;					
		TACCTL0 |= CCIE;			// turn the timer interrupts back on for the next bit
	}
}

