/*
 * tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)


/************************ variables used in this file *********************/
struct tux_bottons{
	spinlock_t lock;
	unsigned long buttons;
};
static struct tux_bottons buttons_status;
static unsigned long LED_status, handle_bottons;
static unsigned int ack_check;


const static unsigned char seven_sement_information[16] = {0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD,0xED,0x86, 0xEF, 0xAF, 0xEE, 0x6D, 0xE1, 0x4F, 0xE9, 0xE8};
//this store the information about how the number display in the seven sement


/************************function implemented in this file*****************/
int tuxctl_ioctl_tux_initial(struct tty_struct* tty);
int tuxctl_ioctl_tux_buttons(struct tty_struct* tty, unsigned long arg);
int tuxctl_ioctl_set_LED(struct tty_struct* tty, unsigned long arg);



/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in
 * tuxctl-ld.c. It calls this function, so all warnings there apply
 * here as well.
 */
void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet) {
    unsigned a, b, c;

	//if(handle_bottons) return;
    a = packet[0]; /* Avoid printk() sign extending the 8-bit */
    b = packet[1]; /* values when printing them. */
    c = packet[2];

    /*printk("packet : %x %x %x\n", a, b, c); */
	switch(a){
		case MTCP_ACK:
			ack_check = 1;
			break;
		case MTCP_RESET:
			tuxctl_ioctl_tux_initial(tty);
			if(!ack_check)	
				return
			tuxctl_ioctl_set_LED(tty,LED_status);
			break;
		case MTCP_BIOC_EVENT:
			//handle_bottons = 1;
			unsigned long flags;
			unsigned int LEFT, DOWN;
			b = ~b;
			c = ~c;
			LEFT = (c & 0x02) >>1;  //because left is the second bit of c 
			DOWN = (c & 0x04) >>2;  //because down is the third bit of c

			spin_lock_irqsave(&(buttons_status.lock), flags);
			
			//fill the value in button, change the order of left and down
			buttons_status.buttons = ~((((b &0x0F) | ((c & 0x0F) << 4)) &0x9F) | (LEFT << 6) | (DOWN << 5));
			
			spin_unlock_irqrestore(&(buttons_status.lock), flags);
			//handle_bottons = 0;
			break;
		default:
			return;
	}
	return;
}




/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/

int tuxctl_ioctl(struct tty_struct* tty, struct file* file,
                 unsigned cmd, unsigned long arg) {
    switch (cmd) {
        case TUX_INIT:
			return tuxctl_ioctl_tux_initial(tty);
        case TUX_BUTTONS:
			return tuxctl_ioctl_tux_buttons(tty,arg);
        case TUX_SET_LED:
			return tuxctl_ioctl_set_LED(tty,arg);
		case TUX_LED_ACK:
			return 0;
		case TUX_LED_REQUEST:
			return 0;
		case TUX_READ_LED:
			return 0;
        default:
            return -EINVAL;
    }
}

/*
 * tuxctl_ioctl_tux_initial
 *   DESCRIPTION: initial all tux variable that will be used in this file

 *   INPUTS: tty -- the object that needs to get input 
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: all initial value will be initialed
 */

int tuxctl_ioctl_tux_initial(struct tty_struct* tty){
	unsigned char initial_value[2];
	
	initial_value[0] = MTCP_BIOC_ON;
	initial_value[1] = MTCP_LED_USR;
	
	tuxctl_ldisc_put(tty, &initial_value[0],1);			//just one bytes
	tuxctl_ldisc_put(tty, &initial_value[1],1);			//just one bytes
	
	buttons_status.buttons = 0xFF;						//set all buttons to be not pressed
	buttons_status.lock = SPIN_LOCK_UNLOCKED;
	ack_check = 0;
	LED_status = 0;
	handle_bottons = 0;
	return 0;
}

/*
 * tuxctl_ioctl_tux_buttons
 *   DESCRIPTION: store the buttons status in arg

 *   INPUTS: tty -- the object that needs to get input 
 *			 arg -- the address that will store the buttons status 
 *   OUTPUTS: -EFAULT of wrong
 *   RETURN VALUE: none
 *   SIDE EFFECTS: the buttons status will be saved
 */
int tuxctl_ioctl_tux_buttons(struct tty_struct* tty, unsigned long arg){
	unsigned long flags;
	unsigned long * pointer_to_buttons_status;
	int result;
	pointer_to_buttons_status = &(buttons_status.buttons);
	
	spin_lock_irqsave(&(buttons_status.lock), flags);
	result = copy_to_user((void *)arg, (void*)pointer_to_buttons_status, sizeof(long));			
	
	spin_unlock_irqrestore(&(buttons_status.lock), flags);
	
	if(result>0) return -EFAULT;
	else 
		return 0;
	
	
	
}

/*
 * tuxctl_ioctl_set_LED
 *   DESCRIPTION: send the led meesage to the tux 

 *   INPUTS: tty -- the object that needs to get input 
 *			 arg -- the message that will be displayed in the tux
 *   OUTPUTS: -none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: the led status will be saved
 */

int tuxctl_ioctl_set_LED(struct tty_struct* tty, unsigned long arg){
	unsigned char display[4];
	unsigned long bitmask;
	unsigned i;
	unsigned char led_number, dp;
	unsigned char led_buffer[6];
	
	if(!ack_check) return -1;
	ack_check = 0;                
	bitmask = 0x000F;      //all 1 in the last bit, use to check arg
	
	//load 4 value from arg to display
	for(i=0; i<4; i++){
		display[i] = (bitmask & arg) >>(4*i);
		bitmask <<=4;
	}
	
	led_number = (arg &(0x0F<<16)) >> 16;    //just check the low 4 bits of the third byte
	dp = (arg &(0x0F<<24)) >> 24;			//just check the low 4 bit of the highest byte
	
	led_buffer[0] = MTCP_LED_USR;
	tuxctl_ldisc_put(tty, &led_buffer[0], 1);
	
	led_buffer[0] = MTCP_LED_SET;
	led_buffer[1] = led_number;
	
	bitmask = 0x01;
	for(i = 0; i < 4; i++){				//4 different led
		if(led_number & bitmask){
			display[i] = seven_sement_information[display[i]];
			if(dp & bitmask) 	display[i] |=0x10;		//set the dp bit to 1
			led_buffer[2+i] = display[i];
		}
		else{
			led_buffer[2+i] = 0x0;
		}
		bitmask <<= 1;
	}
	
	if(display[3] == 0){
		tuxctl_ldisc_put(tty, led_buffer, 5);
	}else{
		tuxctl_ldisc_put(tty, led_buffer, 6);
	}
	
	LED_status = arg;
	
	return 0;
	
}














