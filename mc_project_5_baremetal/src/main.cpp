//#include <stdint.h>
//#include <stdbool.h>
//#include <Adafruit_TinyUSB.h>
#include <Arduino.h>

// Definitions, cf. Datasheet
#define P0_BASE_ADDRESS 0x50000000
#define P1_BASE_ADDRESS 0x50000300
#define DIRSET_OFFSET 0x518
#define DIRCLR_OFFSET 0x51C
#define OUTSET_OFFSET 0x508
#define OUTCLR_OFFSET 0x50C
#define PIN_CNF_OFFSET 0x700
#define IN 0x510 
// MPRS Conversion Constants
#define OUT_MAX 15099494    // (90% of 224 counts or 0xE66666)
#define OUT_MIN  1677722    // (10% of 224 counts or 0x19999A) 
#define P_MAX 25
#define P_MIN 0

// Make a PIN an output
void GPIO_set_to_Output(uint8_t port, uint8_t pin_number) {
    uint32_t * tmp = (uint32_t*) ((port==1?P1_BASE_ADDRESS:P0_BASE_ADDRESS) + DIRSET_OFFSET);
    *tmp = (1<<pin_number);
}
// Set the value of an output PIN
void GPIO_set_Output(uint8_t port, uint8_t pin_number, bool value){
    uint32_t * tmp = (uint32_t*) ((port==1?P1_BASE_ADDRESS:P0_BASE_ADDRESS) + (value?OUTSET_OFFSET:OUTCLR_OFFSET));
    *tmp = (1<<pin_number);
}
// Make a PIN an input
void GPIO_set_to_Input(uint8_t port, uint8_t pin_number) {
    uint32_t * tmp = (uint32_t*) ((port==1?P1_BASE_ADDRESS:P0_BASE_ADDRESS) + DIRSET_OFFSET);
    *tmp = (1<<pin_number);
}

void GPIO_set_Input_Pullup(uint8_t pin_number){
    uint32_t * tmp = (uint32_t*) (P0_BASE_ADDRESS + PIN_CNF_OFFSET + pin_number* 0x04);
    *tmp = 0xC;
}

bool GPIO_get_Input(uint8_t port, uint8_t pin_number) {
    uint32_t * tmp = (uint32_t*) ((port==1?P1_BASE_ADDRESS:P0_BASE_ADDRESS) + IN);
    return (1<<pin_number) & *tmp;
}

double MPRLS_binary_to_pressure(uint32_t binary) {
    return ((binary - OUT_MIN)*(P_MAX - P_MIN))/(OUT_MAX - OUT_MIN) + P_MIN;
}


void setup(){
    GPIO_set_to_Output(0,6);
    GPIO_set_Output(0,6,true);
    GPIO_set_to_Input(0,29);
    GPIO_set_Input_Pullup(29);
}

void loop(){
    GPIO_set_Output(0,6,GPIO_get_Input(0,29));
    delay(100);
}