#ifndef BAREMETAL_GPIO_H_
#define BAREMETAL_GPIO_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>
// See datasheet page 151 and following to complete
// Base address
#define BMGPIO_P0
#define BMGPIO_P1
// Register offsets
#define BMGPIO_OUTSET
#define BMGPIO_OUTCLR
#define BMGPIO_DIRSET
#define BMGPIO_DIRCLR
#define BMGPIO_IN
// Function prototypes
void BMGPIO_set_Output(uint8_t port, uint8_t pin_number, bool value);
void BMGPIO_set_to_Output(uint8_t port, uint8_t pin_number);
bool BMGPIO_get_Input(uint8_t port, uint8_t pin_number);
void BMGPIO_set_to_Input(uint8_t port, uint8_t pin_number);
#ifdef __cplusplus
}
#endif
#endif
