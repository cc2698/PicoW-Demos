/**
 * Hunter Adams (vha3@cornell.edu)
 * 
 *
 */

#define ADDRESS 0x68
#define I2C_CHAN i2c0
#define SDA_PIN  8
#define SCL_PIN  9
#define I2C_BAUD_RATE 400000

// VGA primitives - usable in main
void mpu6050_reset(void) ;
void mpu6050_read_raw(float accel[3], float gyro[3]) ;