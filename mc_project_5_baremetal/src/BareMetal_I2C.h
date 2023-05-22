#ifndef BAREMETAL_I2C_H_
#define BAREMETAL_I2C_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <stdbool.h>
// See datasheet page 450 and following
// Base address
#define BMI2C_INSTANCE_TWI0
#define BMI2C_INSTANCE_TWI1
// Register offsets - please complete!
#define BMI2C_TASK_STARTRX_REG
#define BMI2C_TASK_STARTTX_REG
#define BMI2C_TASK_STOP_REG
#define BMI2C_TASK_SUSPEND_REG
#define BMI2C_TASK_RESUME_REG
#define BMI2C_EVENTS_STOPPED_REG
#define BMI2C_EVENTS_RXDREADY_REG
#define BMI2C_EVENTS_TXDSENT_REG
#define BMI2C_EVENTS_ERROR_REG
#define BMI2C_EVENTS_BB_REG
#define BMI2C_EVENTS_SUSPENDED_REG
#define BMI2C_SHORTS_REG 0x200
#define BMI2C_INTENSET_REG 0x304
#define BMI2C_INTENCLR_REG 0x308
#define BMI2C_ERRORSRC_REG 0x4C4
#define BMI2C_ENABLE_REG 0x500
#define BMI2C_PSEL_SCL_REG 0x508
#define BMI2C_PSEL_SDA_REG 0x50C
#define BMI2C_RXD_REG 0x518
#define BMI2C_TXD_REG 0x51C
#define BMI2C_FREQUENCY_REG 0x524
#define BMI2C_ADDRESS_REG 0x588
// Constants
#define BMI2C_FREQ_100KBPS 0x01980000
#define BMI2C_FREQ_250KBPS 0x04000000
#define BMI2C_FREQ_400KBPS 0x06680000
// Function prototypes
void BMI2C_init_IOs(uint32_t instance, uint16_t type, uint8_t connect, uint8_t
port, uint8_t pin);
void BMI2C_set_Address(uint32_t instance, uint8_t address);
void BMI2C_set_Frequency(uint32_t instance, uint32_t frequency);
void BMI2C_enable(uint32_t instance);
void BMI2C_disable(uint32_t instance);
void BMI2C_trigger_Task(uint32_t instance, uint16_t task);
bool BMI2C_read_Event(uint32_t instance, uint16_t event);
bool BMI2C_Error_occured(uint32_t instance);
void BMI2C_Write(uint32_t instance, uint8_t * data, uint8_t length);
void BMI2C_Read(uint32_t instance, uint8_t * buffer, uint8_t length);
uint8_t BMI2C_get_Address(uint32_t instance);
#ifdef __cplusplus
}
#endif
#endif