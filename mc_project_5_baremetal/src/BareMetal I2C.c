#include "BareMetal_I2C.h"
#include "BareMetalGPIO.h"

// Set to input and assign the special SCL or SDA function to these pins
void BMI2C_init_IOs(uint32_t instance, uint16_t type, uint8_t connect, uint8_t
port, uint8_t pin) {
BMGPIO_set_to_Input(port, pin);
uint32_t * tmp = (uint32_t*) (instance + type);
*tmp = ((connect>0?1:0)<<31) | ((port>0?1:0)<<5) | ((pin>31?31:pin)<<0);
}

void BMI2C_set_Address(uint32_t instance, uint8_t address) {
/* TO DO */
}

uint8_t BMI2C_get_Address(uint32_t instance) {
/* TO DO */
}

void BMI2C_set_Frequency(uint32_t instance, uint32_t frequency) {
/* TO DO */
}

void BMI2C_enable(uint32_t instance) {
uint32_t * tmp = (uint32_t*) (instance + BMI2C_ENABLE_REG);
*tmp = 5; //see datasheet page 456
}

void BMI2C_disable(uint32_t instance) {
/* TO DO */
}

void BMI2C_trigger_Task(uint32_t instance, uint16_t task) {
/* TO DO */
}

bool BMI2C_read_Event(uint32_t instance, uint16_t event) {
/* TO DO */
}

bool BMI2C_Error_occured(uint32_t instance) {
/* TO DO */
}

void BMI2C_Write(uint32_t instance, uint8_t * data, uint8_t length) {
    volatile uint32_t * tmp;

    // trigger START condition
    BMI2C_trigger_Task(instance, BMI2C_TASK_STARTTX_REG);

    // send the rest of the data
    for(uint8_t i = 0; i<length; i++){
        // write next byte
        tmp = (uint32_t*) (instance + BMI2C_TXD_REG);
        *tmp = data[i];
        // wait for transmission to complete, then clear flag
        tmp = (uint32_t*) (instance + BMI2C_EVENTS_TXDSENT_REG);
    while((*tmp) == 0){
     /* wait! */
    }
    *tmp = 0;
}

// when done: trigger STOP condition
BMI2C_trigger_Task(instance, BMI2C_TASK_STOP_REG);
}
void BMI2C_Read(uint32_t instance, uint8_t * buffer, uint8_t length) {
// trigger START condition
BMI2C_trigger_Task(instance, BMI2C_TASK_STARTRX_REG);
volatile uint32_t * tmp;
// read the data
for(uint8_t i = 0; i<length; i++){
// wait for prior transmission to complete
tmp = (uint32_t*) (instance + BMI2C_EVENTS_RXDREADY_REG);
while((*tmp) == 0){ /* wait! */}
*tmp = 0;
// prior to reading last byte: Stop condition, then NACK is returned instead of ACK to terminate sequence.
if(i == length-1){
BMI2C_trigger_Task(instance, BMI2C_TASK_STOP_REG);
}
// read next byte and write to buffer
tmp = (uint32_t*) (instance + BMI2C_RXD_REG);
buffer[i] = (uint8_t) (*tmp);
}
}