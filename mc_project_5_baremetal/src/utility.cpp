#include  <Arduino.h>
// Definitions, cf. Datasheet
#define P0_BASE_ADDRESS 0x50000000
#define P1_BASE_ADDRESS 0x50000300
#define DIRSET_OFFSET 0x518
#define DIRCLR_OFFSET 0x51C
#define OUTSET_OFFSET 0x508
#define OUTCLR_OFFSET 0x50C
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

double MPRLS_binary_to_pressure(uint32_t binary) {
    
}

// Arduino IDE: Initialization
void setup(){
// Define P0.13 as an output
GPIO_set_to_Output(0,13);
// Define P0.13 output to be logic-’1’
GPIO_set_Output(0,13,true);
}
// Arduino IDE: Main loop
void loop(){
// To Do!
}