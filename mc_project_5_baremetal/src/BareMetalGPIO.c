#include "BareMetalGPIO.h"
void BMGPIO_set_Output(uint8_t port, uint8_t pin_number, bool value){
uint32_t * tmp = (uint32_t*) ((port==1?BMGPIO_P1:BMGPIO_P0) + (value?
BMGPIO_OUTSET:BMGPIO_OUTCLR));
*tmp = (1<<pin_number);
}
void BMGPIO_set_to_Output(uint8_t port, uint8_t pin_number) {
uint32_t * tmp = (uint32_t*) (port==1?BMGPIO_P1:BMGPIO_P0 + BMGPIO_DIRSET);
*tmp = (1<<pin_number);
}
void BMGPIO_set_to_Input(uint8_t port, uint8_t pin_number) {
/* TO DO */
}
bool BMGPIO_get_Input(uint8_t port, uint8_t pin_number) {
/* TO DO */
}