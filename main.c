#include "msp.h"


/**
 * main.c
 */
void blankLED(){
     P4->OUT = 0xFF;
}
void selectLEDDigit(int k){
    unsigned int LEDDigit[4]={~0b00000100, ~0x08, ~0x10, ~0x20}; //Digit Look-up Table
    P8->OUT = LEDDigit[3-k];
}
void outputSegments(int hexIndex){
    unsigned int sseg_table[20]={0b11000000,0b11111001,0b10100100,0b10110000,
                       0b10011001,0b10010010,0b010000010,0b11111000,
                       0b10000000,0b10010000,0b10001000,0b10000011,
                       0b11000110,0b10100001,0b11000111,0b11111111,0b11000000,0b10001100,0b10000110,0b10101011};

     P4->OUT = sseg_table[hexIndex];
}
int readKeypad(){
    return P9->IN & 0b00001111;
}
void wait(int count){
    int i = 0;
    while(i<count){
        i+=1;
    }
}

struct keypadStruct{
    int key;
    int inNum[4];
    int debounceCounter;
    int k;
    int state;
    int rowx;
    int keyx;
    int ptr;
};

struct lockboxStruct{
    int state;
    int passcode[4];
    int open_flag;
    int lock_flag;
    int cancel_flag;
    int count;
    int wrong_passcode_count;//wrong passcode count
    int correct_passcode;//correct passcode count
};

void keypad_fsm(struct keypadStruct *kp, struct lockboxStruct *lb);
void lockbox_fsm(struct keypadStruct *kp, struct lockboxStruct *lb);

void main(void)
{
	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;		// stop watchdog timer

	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;     // stop watchdog timer

	    //Set Input and Output Pins
	    //Set P8.2-5 to output for digits
	    P8->DIR |= BIT2 | BIT3 | BIT4 | BIT5;
	    //Set P4.0-6 to output for segments
	    P4->DIR |= BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5 | BIT6;
	    //Set P9.0-3 to input for keypad
	    P9->DIR &= ~(BIT0 | BIT1 | BIT2 | BIT3);
	    //Set GPIO Pins for solenoid
	    P2->DIR |= BIT5;
	    //Set GPIO Pins for LED
	    P5->DIR |= BIT0;

	    struct keypadStruct kp = {0, {16,16,16,16}, 0, 0, 0, 0, 0, 0};
	    struct lockboxStruct lb = {0, {0,0,0,0}, 0, 0, 0, 0, 0, 0};

	    //main program loop
	    while(1){
	        //Output an 'all blank' 7 segment pattern to Port 4
	        blankLED();


	        //Output to Port 8 to enable digit
	        selectLEDDigit(kp.k);

	        //Get keypad Input
	        kp.key = readKeypad();

	        //Output digit's pattern to Port 4
	        outputSegments(kp.inNum[kp.k]);
	        //wait(1000);

	        keypad_fsm(&kp, &lb);
	        lockbox_fsm(&kp, &lb);

	        //Increment k 0-3
	        if(kp.k >= 3)
	            kp.k=0;
	        else
	            kp.k+=1;
	    }
}

void display0(struct keypadStruct *kp){
    kp->inNum[0] = 16;
    kp->inNum[1] = 16;
    kp->inNum[2] = 16;
    kp->inNum[3] = 16;
    kp->ptr = 0;
}

void process_open_flag(struct lockboxStruct *lb){
    lb->state = 1;
    lb->open_flag = 0;
}

void process_lock_flag(struct keypadStruct *kp, struct lockboxStruct *lb){
    int i;
    for(i=0; i<4; i+=1){
        lb->passcode[i] = kp->inNum[i];
    }

    lb->state = 2;
    lb->lock_flag = 0;
}

void energize_solenoid(struct lockboxStruct *lb){
    P2->OUT = ~BIT5;
    lb->count += 1;
}

void de_energize_solenoid(struct keypadStruct *kp, struct lockboxStruct *lb){
    P2->OUT = BIT5;
    display0(kp);
    lb->count = 0;
    lb->state = 0;
}

void flash_led(struct lockboxStruct *lb){
    if(lb->count/1000 % 2 == 1)
        P5->OUT = BIT0;
    else
        P5->OUT = ~BIT0;
}

void check_cancel(struct keypadStruct *kp, struct lockboxStruct *lb){
    if(lb->cancel_flag == 1){
        lb->state = 0;
        lb->count = 0;
        P5->OUT = BIT0;
        display0(kp);
        lb->cancel_flag = 0;
    }
    else{
        lb->count += 1;
    }
}

void display_LOC(struct keypadStruct *kp){
    kp->inNum[0] = 15;
    kp->inNum[1] = 14;
    kp->inNum[2] = 0;
    kp->inNum[3] = 12;
}

void lock_box(struct lockboxStruct *lb){
    lb->state = 3;
    lb->count = 0;
}

void check_passcode(struct keypadStruct *kp, struct lockboxStruct *lb){
    lb->open_flag = 0;
    int temp_wrong_count = lb->wrong_passcode_count;
    int i;
    for(i=0; i<4; i+=1){
        if(kp->inNum[i] != lb->passcode[i]){
            lb->wrong_passcode_count += 1;
            display_LOC(kp);
            kp->ptr = 0;
            break;
        }
    }
    if(lb->wrong_passcode_count == temp_wrong_count){
        lb->correct_passcode = 1;
    }
}

void display_open(struct keypadStruct *kp){
    kp->inNum[0] = 0;
    kp->inNum[1] = 17;
    kp->inNum[2] = 18;
    kp->inNum[3] = 19;
}

void unlock_box(struct keypadStruct *kp, struct lockboxStruct *lb){
    display_open(kp);
    lb->correct_passcode = 0;
    lb->wrong_passcode_count = 0;
    lb->state = 1;
}

void set_lockdown(struct lockboxStruct *lb){
    lb->open_flag = 0;
    lb->wrong_passcode_count = 0;
    lb->state = 4;
}

void display_Ld(struct keypadStruct *kp){
    kp->inNum[0] = 15;
    kp->inNum[1] = 14;
    kp->inNum[2] = 13;
    kp->inNum[3] = 15;
}

void return_to_lock(struct lockboxStruct *lb){
    lb->count = 0;
    lb->wrong_passcode_count = 0;
    lb->state = 3;
}

void lockbox_fsm(struct keypadStruct *kp, struct lockboxStruct *lb){
    switch(lb->state){
        case 0:
        {
            if(lb->open_flag == 1){
                process_open_flag(lb);
                break;
            }
            else if(lb->lock_flag == 1){
                process_lock_flag(kp, lb);
                break;
            }
            else
                break;
        }
        case 1:
        {
            if(lb->count < 30000){//30000
                energize_solenoid(lb);
                break;
            }
            else{
                de_energize_solenoid(kp, lb);
                break;
            }
        }
        case 2:
        {
            if(lb->count < 30000){//30000
                flash_led(lb);
                check_cancel(kp, lb);
                break;
            }
            else{
                display_LOC(kp);
                lock_box(lb);
                break;
            }
        }
        case 3:
        {
            if(lb->open_flag == 1){
                check_passcode(kp, lb);
            }
            if(lb->correct_passcode == 1){
                unlock_box(kp, lb);
                break;
            }
            if(lb->wrong_passcode_count >= 5){
                set_lockdown(lb);
                break;
            }
            else
                break;
        }
        case 4:
        {
            if(lb->count < 80000){
                display_Ld(kp);
                lb->count += 1;
            }
            else{
                display_LOC(kp);
                return_to_lock(lb);
            }
        }
    }
}

void input_keypress(struct keypadStruct *kp, int digit){
    kp->inNum[kp->ptr] = digit;

    if(kp->ptr >= 3)
        kp->ptr=0;
    else
        kp->ptr+=1;
}

int full_input(struct keypadStruct *kp, int check_char){
    if(kp->inNum[3] != check_char){
        return 1;
    }
    else
        return 0;
}

void keypad_fsm(struct keypadStruct *kp, struct lockboxStruct *lb){
    int keypad_table[4][9]= {{0,10,3,999,2,999,999,999,1},
                             {0,11,6,999,5,999,999,999,4},
                             {0,12,9,999,8,999,999,999,7},
                             {0,13,15,999,0,999,999,999,14},};

    switch (kp->state){
        case 0://Wait for key press
        {
            if (kp->key != 0){
                kp->rowx = kp->k;
                kp->keyx = kp->key;
                kp->state = 1;
                kp->debounceCounter = 0;
            }
            break;
        }
        case 1://press debounce
        {
            if(kp->k==kp->rowx & kp->key==kp->keyx)
                kp->debounceCounter+=1;
            if(kp->k==kp->rowx & kp->key != kp->keyx)
                kp->state = 0;
            if(kp->debounceCounter>5)
                kp->state=2;
            else
                break;
        }
        case 2://process keypress
        {
            int digit = keypad_table[kp->rowx][kp->keyx];
            if(lb->state == 0){
                if(digit == 10){
                    lb->open_flag = 1;
                }
                else if(digit == 11 && full_input(kp,16) == 1){
                    lb->lock_flag = 1;
                }
                else if(digit != 10 && digit != 11){
                    input_keypress(kp, digit);
                }
            }
            else if(lb->state == 2){
                if(kp->key != 0){
                    lb->cancel_flag = 1;
                }
            }
            else if(lb->state == 3){
                if(digit == 10 && full_input(kp,12) == 1){
                    lb->open_flag = 1;
                }
                else if(digit !=10 && digit != 11){
                    input_keypress(kp, digit);
                }
            }


            kp->debounceCounter=0;
            kp->state=3;
            break;
        }
        case 3:
        {
            if(kp->k==kp->rowx & kp->key==0)
                kp->debounceCounter+=1;
            if(kp->k==kp->rowx & kp->key!=0)
                kp->debounceCounter=0;
            if(kp->debounceCounter>5)
                kp->state=0;
            else
                break;
        }
    }
}
