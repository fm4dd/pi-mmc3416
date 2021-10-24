/* ------------------------------------------------------------ *
 * file:        mmc3416.h                                       *
 * purpose:     header file for getmmc3416.c and i2c_mmc3416.c  *
 *                                                              *
 * author:      09/04/2021 Frank4DD                             *
 * ------------------------------------------------------------ */

#define I2CBUS        "/dev/i2c-1" // Raspi default I2C bus
#define I2C_ADDR           "0x30"  // The sensor default I2C addr
#define PRD_ID               0x06  // MMC3416 responds with 0x06
#define POWER_MODE_NORMAL    0x00  // sensor default power mode

/* ------------------------------------------------------------ *
 * Sensor register address information                          *
 * ------------------------------------------------------------ */
/* X-axis data register (read-only) */
#define MMC3416_XOUT_LSB_ADDR    0x00
#define MMC3416_XOUT_MSB_ADDR    0x01
/* Y-axis data register (read-only) */
#define MMC3416_YOUT_LSB_ADDR    0x02
#define MMC3416_YOUT_MSB_ADDR    0x03
/* Z-axis data register (read-only) */
#define MMC3416_ZOUT_LSB_ADDR    0x04
#define MMC3416_ZOUT_MSB_ADDR    0x05
/* Status register (read-only) */
#define MMC3416_STATUS_ADDR      0x06
/* Internal Control-0 register (wo) */
#define MMC3416_CTL0_ADDR        0x07
/* Internal Control-1 register (wo) */
#define MMC3416_CTL1_ADDR        0x08
/* Product ID register (read-only)  */
#define MMC3416_PRODUCT_ID_ADDR  0x20

/* ------------------------------------------------------------ *
 * Define byte-as-bits printing for debug output                *
 * ------------------------------------------------------------ */
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

/* ------------------------------------------------------------ *
 * global variables                                             *
 * ------------------------------------------------------------ */
int i2cfd;             // I2C file descriptor
int verbose;           // debug flag, 0 = normal, 1 = debug mode
float offset[3];       // sensor axis offset values
float declination;     // local declination value

/* ------------------------------------------------------------ *
 * MMC3416 status and control data structure                      *
 * ------------------------------------------------------------ */
struct mmc3416inf{
   char prd_id;      // reg 0x20 returns 0x06 for type MMC3416
   char ctl_0_mode;  // reg 0x07 cont mode, cont freq, boost
   char ctl_1_mode;  // reg 0x08 resolution, selftest
};

/* ------------------------------------------------------------ *
 * MMC3416 measurement data struct.                             *
 * ------------------------------------------------------------ */
struct mmc3416data{
   float X;        // X component
   float Y;        // Y component
   float Z;        // Z component
};

/* ------------------------------------------------------------ *
 * external function prototypes for I2C bus communication       *
 * ------------------------------------------------------------ */
extern void get_i2cbus(char*, char*);         // get the I2C bus file handle
extern void mmc3416_set();                    // charge CAP and execute SET
extern void mmc3416_reset();                  // charge CAP and execute RESET
extern int mmc3416_swreset();                 // SW reset clears registers
extern void mmc3416_init();                   // initialize the sensor
extern int mmc3416_dump();                    // dump the register map data
extern void mmc3416_info(struct mmc3416inf*); // print sensor information
extern char get_prdid();                      // get the sensor product id
extern int set_cmfreq(int);                   // set continuous read frequency
extern int mmc3416_read();                    // read sensor data
extern float get_heading();                   // calculate heading from raw data
extern int delay(long msec);                  // create a Arduino-style delay
