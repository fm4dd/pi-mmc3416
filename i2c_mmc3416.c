/* ------------------------------------------------------------ *
 * file:        i2c_mmc3416.c                                   *
 * purpose:     Extract data from MEMSIC MMC3416 sensor modules *
 *              Functions for I2C bus communication, get and    *
 *              set sensor register data. Ths file belongs to   *
 *              the pi-mmc3416 package. Functions are called    *
 *              from getmmc3416.c, globals are in mmc3416.h.    *
 *                                                              *
 * Requires:	I2C development packages i2c-tools libi2c-dev   *
 *                                                              *
 * author:      13/09/2021 Frank4DD                             *
 * note:	MMC3416 auto-increments only XYZ registers      *
 * ------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include "mmc3416.h"

/* ------------------------------------------------------------ *
 * get_i2cbus() - Enables the I2C bus communication. RPi 2,3,4  *
 * use /dev/i2c-1, RPi 1 used i2c-0, NanoPi Neo also uses i2c-0 *
 * ------------------------------------------------------------ */
void get_i2cbus(char *i2cbus, char *i2caddr) {

   if((i2cfd = open(i2cbus, O_RDWR)) < 0) {
      printf("Error failed to open I2C bus [%s].\n", i2cbus);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: I2C bus device: [%s]\n", i2cbus);
   /* --------------------------------------------------------- *
    * Set I2C device (MMC3416 I2C address is 0x30)              *
    * --------------------------------------------------------- */
   int addr = (int)strtol(i2caddr, NULL, 16);
   if(verbose == 1) printf("Debug: Sensor address: [0x%02X]\n", addr);

   if(ioctl(i2cfd, I2C_SLAVE, addr) != 0) {
      printf("Error can't find sensor at address [0x%02X].\n", addr);
      exit(-1);
   }
   /* --------------------------------------------------------- *
    * I2C communication test is the only way to confirm success *
    * --------------------------------------------------------- */
   if(get_prdid(addr) == 0) {
      printf("Error: No response from I2C. addr [0x%02X]?\n", addr);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: Got data @addr: [0x%02X]\n", addr);
}

/* --------------------------------------------------------------- *
 * get_prdid() returns the MMC3416 product id from register 0x20.  *
 * --------------------------------------------------------------- */
char get_prdid() {
   char reg = MMC3416_PRODUCT_ID_ADDR;
   char buf = 0;
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
   }

   if(read(i2cfd, &buf, 1) != 1) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
   }
   return buf;
}

/* --------------------------------------------------------------- *
 * mmc3416_set() initialize the magnetization in normal direction  *
 * --------------------------------------------------------------- */
void mmc3416_set() {
   char  buf[2] = {MMC3416_CTL0_ADDR, 0x80}; // set bit-8 in reg 0x07
   if(verbose == 1) printf("Debug: Write databyte: [0x%02X] to   [0x%02X]\n", buf[1], buf[0]);
   if(write(i2cfd, buf, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", buf[0]);
      exit(-1);
   }
   delay(60);                    // wait >50ms for the CAP charge to finish

   buf[0] = MMC3416_CTL0_ADDR;   // ctl-0 register 0x07
   buf[1] = 0x20;                // bit-6: send SET CMD
   if(verbose == 1) printf("Debug: Write databyte: [0x%02X] to   [0x%02X]\n", buf[1], buf[0]);
   if(write(i2cfd, buf, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", buf[0]);
      exit(-1);
   }
}

/* --------------------------------------------------------------- *
 * mmc3416_reset()  reverses magnetization (180 degrees opposed)   *
 * --------------------------------------------------------------- */
void mmc3416_reset() {
   char  buf[2] = {MMC3416_CTL0_ADDR, 0x80}; // set bit-8 in reg 0x07
   if(verbose == 1) printf("Debug: Write databyte: [0x%02X] to   [0x%02X]\n", buf[1], buf[0]);
   if(write(i2cfd, buf, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", buf[0]);
      exit(-1);
   }
   delay(60);                    // wait >50ms for the CAP charge to finish

   buf[0] = MMC3416_CTL0_ADDR;   // ctl-0 register 0x07
   buf[1] = 0x40;                // bit-6: send RESET CMD
   if(verbose == 1) printf("Debug: Write databyte: [0x%02X] to   [0x%02X]\n", buf[1], buf[0]);
   if(write(i2cfd, buf, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", buf[0]);
      exit(-1);
   }
}

/* --------------------------------------------------------------- *
 * mmc3416_init() identifies the initial sensor offset, runs the   *
 * SET/RESET function for Null Field output temp compensation, and *
 * clears the sensor residual from strong external magnet exposure *
 * --------------------------------------------------------------- */
void mmc3416_init(struct mmc3416data *mmc3416d) {
   float ds1[3] = {0, 0, 0};
   float ds2[3] = {0, 0, 0};

   if(verbose == 1) printf("Debug: mmc3416_init(): ...\n");
   offset[0] = 0; offset[1] = 0; offset[2] = 0; // clear offset

   mmc3416_set();
   delay(10);
   /* ------------------------------------------------------------ *
    * The reading after at SET will contain the external magnetic  *
    * field data, plus the Offset: ds1 = +H + Offset               *
    * ------------------------------------------------------------ */
   mmc3416_read(mmc3416d);
   ds1[0] = mmc3416d->X;
   ds1[1] = mmc3416d->Y;
   ds1[2] = mmc3416d->Z;

   /* ------------------------------------------------------------ *
    * Reset reverses magnetization (180 degrees opposed) to SET    *
    * ------------------------------------------------------------ */
   mmc3416_reset();
   delay(10);
   /* ------------------------------------------------------------ *
    * The reading after RESET will contain the reversed magnetic   *
    * field data, plus the Offset: ds1 = -H + Offset               *
    * ------------------------------------------------------------ */
   mmc3416_read(mmc3416d);
   ds2[0] = mmc3416d->X;
   ds2[1] = mmc3416d->Y;
   ds2[2] = mmc3416d->Z;

   /* ------------------------------------------------------------ *
    * Calculate offset by adding 2 measurements, and divide by two *
    * ------------------------------------------------------------ */
   for (int i=0; i<3; i++) {
       offset[i] = (ds1[i]+ds2[i])/2;
       if(verbose == 1) printf("Debug: Offset Value-%d: [%3.02f]\n",i, offset[i]);
   }

   /* ------------------------------------------------------------ *
    * Set the magnetic orientation back to normal, and exit init() *
    * ------------------------------------------------------------ */
   mmc3416_set();
   if(verbose == 1) printf("Debug: mmc3416_init(): done\n");
}

/* --------------------------------------------------------------- *
 * mmc3416_dump() dumps the complete register map data (15 bytes). *
 * --------------------------------------------------------------- */
int mmc3416_dump() {
   char buf1[9] = {0};  // 9 bytes sensor register data buffer
   char buf2[5] = {0};  // 5 bytes factory register data buffer
   char buf3    = 0;    // 1 byte product ID register data buffer

   /* ------------------------------------------------------ *
    * Read 9 bytes sensor reg data starting at 0x00          *
    * ------------------------------------------------------ */
   char reg = 0x00;
   for(int i=0; i<9; i++) {
      if(write(i2cfd, &reg, 1) != 1) {
         printf("Error: I2C write failure for register 0x%02X\n", reg);
         exit(-1);
      }
   
      if(read(i2cfd, &buf1[i], 9) != 9) {
         printf("Error: I2C read failure for register 0x%02X\n", reg);
         exit(-1);
      }
      reg++;
   }

   /* ------------------------------------------------------ *
    * Factory register data starts from 0x1B until 0x1F.     * 
    * ------------------------------------------------------ */
   reg = 0x1b;
   for(int i=0; i<5; i++) {
      if(write(i2cfd, &reg, 1) != 1) {
         printf("Error: I2C write failure for register 0x%02X\n", reg);
         exit(-1);
      }
   
      if(read(i2cfd, &buf2[i], 5) != 5) {
         printf("Error: I2C read failure for register 0x%02X\n", reg);
         exit(-1);
      }
      reg++;
   }

   /* ------------------------------------------------------ *
    * Product ID register is located at 0x20.                *
    * ------------------------------------------------------ */
   reg = 0x20;
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
      exit(-1);
   }

   if(read(i2cfd, &buf3, 1) != 1) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
      exit(-1);
   }

   printf("------------------------------------------------------\n");
   printf("MEMSIC MMC3416xPJ register dump:\n");
   printf("------------------------------------------------------\n");
   printf(" reg    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
   printf("------------------------------------------------------\n");
   printf("[0x00]");
   for(int i=0; i<9; i++) {
      printf(" %02X", buf1[i]);
   }
   printf(" -- -- -- -- -- -- --\n");
   printf("[0x10] -- -- -- -- -- -- -- -- -- -- --");
   for(int i=0; i<5; i++) {
      printf(" %02X", buf2[i]);
   }
   printf("\n");
   printf("[0x20] %02X\n", buf3);

   /* ------------------------------------------------------ *
    * Display register name table with hex and binary data   *
    * ------------------------------------------------------ */
   printf("\nSensor Reg: hex  binary\n");
   printf("---------------------------\n");
   for(int i=0; i<9; i++) {
      switch(i) {
         case 0:
            printf("  Xout Low"); break;
         case 1:
            printf(" Xout High"); break;
         case 2:
            printf("  Yout Low"); break;
         case 3:
            printf(" Yout High"); break;
         case 4:
            printf("  Zout Low"); break;
         case 5:
            printf(" Zout High"); break;
         case 6:
            printf("    Status"); break;
         case 7:
            printf(" Control-0"); break;
         case 8:
            printf(" Control-1"); break;
         default:
            continue;
      }
      printf(": 0x%02X 0b"BYTE_TO_BINARY_PATTERN"\n", buf1[i], BYTE_TO_BINARY(buf1[i]));
   }
   exit(0);
}

/* --------------------------------------------------------------- *
 * mmc3416_swreset() resets the sensor, and clears config settings *
 * --------------------------------------------------------------- */
int mmc3416_swreset() {
   char data[2];
   data[0] = MMC3416_CTL1_ADDR;
   data[1] = 0xB6;
   if(write(i2cfd, data, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", data[0]);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: Sensor SW Reset complete\n");
   exit(0);
}

/* ------------------------------------------------------------ *
 * mmc3416_info() - read sensor ID and settings from registers  *
 * 0x07, 0x08, 0x20:                                            *
 * char prd_id;      // reg 0x20 returns 0x06 as product ID     *
 * char cm_freq;     // reg 0x07 cont measurement freq bit-2,3  *
 * char boost_mode;  // reg 0x07 disable CAP charge pump bit-4  *
 * char outres_mode; // reg 0x08 output resolution mode bit-0,1 *
 * ------------------------------------------------------------ */
void mmc3416_info(struct mmc3416inf *mmc3416i) {
   mmc3416i->prd_id = get_prdid();

   /* Read MMC3416_CTL0_ADDR data */ 
   char reg = MMC3416_CTL0_ADDR;
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
      exit(-1);
   }

   if(read(i2cfd, &mmc3416i->ctl_0_mode, 1) != 1) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: Got ctl-0 byte: [0x%02X]\n",
                            mmc3416i->ctl_0_mode);

   /* Read MMC3416_CTL1_ADDR data */ 
   reg = MMC3416_CTL1_ADDR;
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
      exit(-1);
   }

   if(read(i2cfd, &mmc3416i->ctl_1_mode, 1) != 1) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: Got ctl-1 byte: [0x%02X]\n",
                            mmc3416i->ctl_1_mode);

}

/* --------------------------------------------------------------- *
 * set_cmfreq() set the continuous read frequency in register 0x07 *
 * --------------------------------------------------------------- */
int set_cmfreq(int new_mode) {
   /* ---------------------------------------- */
   /* Check current freq from ctl-0 register   */
   /* ---------------------------------------- */
   if(verbose == 1) printf("Debug: Set  Read Freq: [0x%02X]\n", new_mode);
   char reg = MMC3416_CTL0_ADDR;
   char regdata = 0;
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
      exit(-1);
   }
   if(read(i2cfd, &regdata, 1) != 1) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
      exit(-1);
   }
   if(verbose == 1) printf("Debug: Read data byte: [0x%02X] from [0x%02X]\n", regdata, reg);

   /* ---------------------------------------- */
   /* frequency mode from reg 0x07 bit-2 and 3 */
   /* ---------------------------------------- */
   int current_mode = ((regdata >> 2) & 0x03);
   if(verbose == 1) printf("Debug: Cont Read Freq: [0x%02X]\n", current_mode);

   /* ---------------------------------------- */
   /* Check if update is needed, or just exit  */
   /* ---------------------------------------- */
   if(new_mode == current_mode) {
      if(verbose == 1) printf("Debug: New freq = current freq, no change.\n");
      return(0);
   }
 
   /* ---------------------------------------- */
   /* Set new freq bits 2, 3 in ctl-0 register */
   /* ---------------------------------------- */
   regdata |= 1 << 0;       // bit-0: 1 start measuring
   regdata |= 1 << 1;       // bit-1: 1 enable continous mode = on

   if(new_mode == 0) {
      regdata &= ~(1 << 2); // bit-2: 0
      regdata &= ~(1 << 3); // bit-3: 0
   }
   else if(new_mode == 1) {
      regdata |= 1 << 2;    // bit-2: 1
      regdata &= ~(1 << 3); // bit-3: 0
   }
   else if(new_mode == 2) {
      regdata &= ~(1 << 2); // bit-2: 0
      regdata |= 1 << 3;    // bit-3: 1
   }
   else if(new_mode == 3) {
      regdata |= 1 << 2;    // bit-2: 1
      regdata |= 1 << 3;    // bit-3: 1
   }
   regdata |= 1 << 5;       // bit-5: Set the data

   /* ---------------------------------------- */
   /* write new setting to ctl-0 register 0x07 */
   /* ---------------------------------------- */
   char buf[2] = {0};
   buf[0] = reg;
   buf[1] = regdata;
   if(verbose == 1) printf("Debug: Write databyte: [0x%02X] to   [0x%02X]\n", buf[1], buf[0]);
   if(write(i2cfd, buf, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", buf[0]);
      return(-1);
   }

   /* ---------------------------------------- */
   /* read the changed data back from register */
   /* ---------------------------------------- */
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
   }

   regdata = 0;
   if(read(i2cfd, &regdata, 1) != 1) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
   }
   if(verbose == 1) printf("Debug: Read data byte: [0x%02X] from [0x%02X]\n", regdata, reg);
   /* cont read frequency mode from reg 0x07 bit-2 and 3 */
   current_mode = ((regdata >> 2) & 0x03);
   if(new_mode != current_mode) {
      if(verbose == 1) printf("Debug: Update failed. New mode %d\n", current_mode);
      return -1;
   }
   if(verbose == 1) printf("Debug: Update sucess. New mode %d\n", current_mode);
   return(0);
}


/* ------------------------------------------------------------ *
 *  mmc3416_read() - take a single data read over the XYZ axis  *
 *  convert to Milli Gauss, and store under the mmc3416 object. *
 * ------------------------------------------------------------ */
int mmc3416_read(struct mmc3416data *mmc3416d) {
   /* ---------------------------------------- */
   /* Request new measurement through reg 0x07 */
   /* ---------------------------------------- */
   char buf[2] = {0};
   buf[0] = MMC3416_CTL0_ADDR;   // ctl-0 register 0x07
   buf[1] = 0x01;                // bit-0: 1 request a new measurement
   if(verbose == 1) printf("Debug: Write databyte: [0x%02X] to   [0x%02X]\n", buf[1], buf[0]);
   if(write(i2cfd, buf, 2) != 2) {
      printf("Error: I2C write failure for register 0x%02X\n", buf[0]);
      return(-1);
   }
   if(verbose == 1) printf("Debug: Wait for measurement:\n");

   /* ---------------------------------------- */
   /* Check status "result ready" in reg 0x06  */
   /* ---------------------------------------- */
   char reg = MMC3416_STATUS_ADDR;
   char regdata = 0;
   while(1) {
      if(write(i2cfd, &reg, 1) != 1) {
         printf("Error: I2C write failure for register 0x%02X\n", reg);
      }

      if(read(i2cfd, &regdata, 1) != 1) {
         printf("Error: I2C read failure for register 0x%02X\n", reg);
      }
      if(verbose == 1) printf("Debug: Read data byte: [0x%02X] from [0x%02X]\n", regdata, reg);

      if((regdata & 0x01) == 1) break; // if the last bit=1, data is ready
      delay(10);                       // wait time
   }
   if(verbose == 1) printf("Debug: measurement is ready.\n");

   /* ---------------------------------------- */
   /* Data is ready to read from 0x00..0x05    */
   /* ---------------------------------------- */
   reg = MMC3416_XOUT_LSB_ADDR;
   char measure[6] = {0, 0, 0, 0, 0, 0};
   if(write(i2cfd, &reg, 1) != 1) {
      printf("Error: I2C write failure for register 0x%02X\n", reg);
   }
   if(read(i2cfd, &measure, 6) != 6) {
      printf("Error: I2C read failure for register 0x%02X\n", reg);
   }

   for(int i=0; i<7; i++) {
      if(verbose == 1) {
         printf("Debug: Read data byte: [0x%02X] from [0x%02X]\n", measure[i], reg+i);
      }
   }

   /* ---------------------------------------- */
   /* Combine LSB/MSB into 16-bit value X Y Z  */
   /* ---------------------------------------- */
   uint16_t measured_data[3];
   measured_data[0] = measure[1] << 8 | measure[0]; // X
   measured_data[1] = measure[3] << 8 | measure[2]; // Y
   measured_data[2] = measure[5] << 8 | measure[4]; // Z

   /* ---------------------------------------- */
   /* Convert raw X Y Z data to milli Gauss    */
   /* ---------------------------------------- */
   mmc3416d->X = 0.48828125 * (float) measured_data[0] - offset[0];
   mmc3416d->Y = 0.48828125 * (float) measured_data[1] - offset[1];
   mmc3416d->Z = 0.48828125 * (float) measured_data[2] - offset[2];
   if(verbose == 1) printf("Debug: Measured value: X-[%3.02f] Y-[%3.02f] Z-[%3.02f]\n",
                            mmc3416d->X, mmc3416d->Y, mmc3416d->Z);

   return(0);
}

/* ------------------------------------------------------- */
/* get_heading() convert two-axis value to compass heading */
/* ------------------------------------------------------- */
float get_heading(struct mmc3416data *mmc3416d) {
   float temp0 = 0; // partial result 0
   float temp1 = 0; // partial result 1
   float deg = 0;   // final result

   /* -------------------------------------------- */
   /* Calculate heading from magnetic field data.  */
   /* each quadrant has its own formula. Quadrant1 */
   /* -------------------------------------------- */
   if (mmc3416d->X < 0) {
      if (mmc3416d->Y > 0) { //Quadrant 1
         temp0 = mmc3416d->Y;
         temp1 = -mmc3416d->X;
         deg = 90 - atan(temp0 / temp1) * (180 / 3.14159);
      }
      else { //Quadrant 2
         temp0 = -mmc3416d->Y;
         temp1 = -mmc3416d->X;
         deg = 90 + atan(temp0 / temp1) * (180 / 3.14159);
      }
   }
   else { 
      if (mmc3416d->Y < 0) { //Quadrant 3
         temp0 = -mmc3416d->Y;
         temp1 = mmc3416d->X;
         deg = 270 - atan(temp0 / temp1) * (180 / 3.14159);
      }
      else { //Quadrant 4
         temp0 = mmc3416d->Y;
         temp1 = mmc3416d->X;
         deg = 270 + atan(temp0 / temp1) * (180 / 3.14159);
      }
   }
   deg += declination;
   if (declination > 0) {
      if (deg > 360) deg -= 360;
   } else {
      if (deg < 0) deg += 360;
   }
   return deg;
}

/* ------------------------------------------------------- */
/* delay() Sleep for the requested number of milliseconds. */
/* ------------------------------------------------------- */
int delay(long msec) {
   struct timespec ts;
   int res;

   if (msec < 0) { errno = EINVAL; return -1; }

   ts.tv_sec = msec / 1000;
   ts.tv_nsec = (msec % 1000) * 1000000;

   do { res = nanosleep(&ts, &ts); }
   while (res && errno == EINTR);

   return res;
}
