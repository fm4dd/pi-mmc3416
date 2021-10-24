/* ------------------------------------------------------------ *
 * file:        getmmc3416.c                                    *
 * purpose:     Sensor control and data extraction program      *
 *              for the MEMSIC MMC3416 Magnetic Field Sensor    *
 *                                                              *
 * return:      0 on success, and -1 on errors.                 *
 *                                                              *
 * requires:	I2C headers, e.g. sudo apt install libi2c-dev   *
 *                                                              *
 * compile:	gcc -o getmmc3416 i2c_mmc3416.c getmmc3416.c    *
 *                                                              *
 * example:	./getmmc3416 -t -o mmc3416.htm                  *
 *                                                              *
 * author:      09/09/2021 Frank4DD                             *
 * ------------------------------------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include "mmc3416.h"

/* ------------------------------------------------------------ *
 * Global variables and defaults                                *
 * ------------------------------------------------------------ */
int verbose = 0;
int outflag = 0;
int argflag = 0;          // 1=dump, 2=info, 3=reset, 4=data, 5=continuous
                          // 6=set_ cont_read_freq
int cm_status = 0;        // continuous read mode enabler on/off
int cmfreq_mode = 0;      // continuous read frequency mode setting
int noboost_status = 0;   // No Boost CAP setting
int outres_mode = 0;      // output resolution mode
char outres_set[4] = {0}; // set output resolution mode value
char status[7]    = {0};  // device status
char i2c_bus[256] = I2CBUS;
char htmfile[256] = {0};

/* ------------------------------------------------------------ *
 * print_usage() prints the programs commandline instructions.  *
 * ------------------------------------------------------------ */
void usage() {
   static char const usage[] = "Usage: getmmc3416 [-b i2c-bus] [-c 0..3] [-d] [-i] [-m mode] [-t] [-l decl] [-r] [-o htmlfile] [-v]\n\
\n\
Command line parameters have the following format:\n\
   -b   I2C bus to query, Example: -b /dev/i2c-1 (default)\n\
   -c   start continuous read with a given frequency 0..3. examples:\n\
             -c 0 = read at 1.5 Hz (1 sample every 1.5 seconds - default)\n\
             -c 1 = read at 13 Hz (1 sample every 77 milliseconds)\n\
             -c 2 = read at 25 Hz (1 sample every 40 milliseconds)\n\
             -c 3 = read at 50 Hz (1 sample every 20 milliseconds)\n\
   -d   dump the complete sensor register map content\n\
   -i   print sensor information\n\
   -l   local declination offset value (requires -t/-c), example: -l 7.73\n\
        see http://www.ngdc.noaa.gov/geomag-web/#declination\n\
   -m   set sensor output resolution mode. arguments: 12/14/16/16h. examples:\n\
             -m 12   = output resolution 12 bit (1.20ms read time)\n\
             -m 14   = output resolution 14 bit (2.16ms read time)\n\
             -m 16   = output resolution 16 bit (4.08ms read time)\n\
             -m 16h  = output resolution 16 bit (7.92ms read time)\n\
   -r   reset sensor\n\
   -t   take a single measurement\n\
   -o   output data to HTML table file (requires -t/-c), example: -o ./mmc3416.html\n\
   -h   display this message\n\
   -v   enable debug output\n\
\n\
\n\
Usage examples:\n\
./getmmc3416 -b /dev/i2c-0 -i\n\
./getmmc3416 -t -v\n\
./getmmc3416 -c 1\n\
./getmmc3416 -t -l 7.73 -o ./mmc3416.html\n\n";
   printf(usage);
}

/* ------------------------------------------------------------ *
 * parseargs() checks the commandline arguments with C getopt   *
 * -d = argflag 1     -i = argflag 2       -r = argflag 3       *
 * -t = argflag 4     -c = argflag 5       -o = outflag 1       *
 * ------------------------------------------------------------ */
void parseargs(int argc, char* argv[]) {
   int arg;
   opterr = 0;

   if(argc == 1) { usage(); exit(-1); }

   while ((arg = (int) getopt (argc, argv, "b:c:dil:m:rto:hv")) != -1) {
      switch (arg) {
         // arg -b + I2C bus device name, type: string, example: "/dev/i2c-1"
         case 'b':
            if(verbose == 1) printf("Debug: arg -b, value %s\n", optarg);
            if (strlen(optarg) >= sizeof(i2c_bus)) {
               printf("Error: I2C bus argument to long.\n");
               exit(-1);
            }
            strncpy(i2c_bus, optarg, sizeof(i2c_bus));
            break;

         // arg -c starts continuous read with given frequency, type: int 0..3
         case 'c':
            if(verbose == 1) printf("Debug: arg -c, value %s\n", optarg);
            argflag = 5;
            if (strlen(optarg) > 1) {
               printf("Error: continuous read frequency mode arg must be between 0..3.\n");
               exit(-1);
            }
            cmfreq_mode = atoi(optarg);
            if(cmfreq_mode < 0 || cmfreq_mode > 3) {
               printf("Error: continuous read frequency mode arg must be between 0..3.\n");
               exit(-1);
            }
            break;

         // arg -d dumps the complete register map data
         case 'd':
            if(verbose == 1) printf("Debug: arg -d\n");
            argflag = 1;
            break;

         // arg -i prints sensor information
         case 'i':
            if(verbose == 1) printf("Debug: arg -i\n");
            argflag = 2;
            break;

         // arg -l sets local declination value, type: float example: 7.37
         case 'l':
            if(verbose == 1) printf("Debug: arg -l\n");
            declination = atoi(optarg);
            // Check delination range, value should be between -30..30
            if (declination < -30.0 || declination > 30.0) {
               printf("Error: Cannot get valid -l declination (should be -30..30).\n");
               exit(-1);
            }
            break;

         // arg -m sets output resolution mode, type: string values 12/14/16/16h
         case 'm':
            if(verbose == 1) printf("Debug: arg -m, value %s\n", optarg);
            if (strlen(optarg) >= sizeof(outres_set)) {
               printf("Error: output resolution mode argument to long.\n");
               exit(-1);
            }
            if(optarg[0] != '1') {
               printf("Error: output resolution mode arg should start with '1'.\n");
               exit(-1);
            }
            strncpy(outres_set, optarg, sizeof(outres_set));
            break;

         // arg -r
         // optional, resets sensor
         case 'r':
            if(verbose == 1) printf("Debug: arg -r\n");
            argflag = 3;
            break;

         // arg -t reads the sensor data
         case 't':
            if(verbose == 1) printf("Debug: arg -t\n");
            argflag = 4;
            break;

         // arg -o + dst HTML file, type: string, requires -t
         // writes the sensor output to file. example: /tmp/sensor.htm
         case 'o':
            outflag = 1;
            if(verbose == 1) printf("Debug: arg -o, value %s\n", optarg);
            if (strlen(optarg) >= sizeof(htmfile)) {
               printf("Error: html file argument to long.\n");
               exit(-1);
            }
            strncpy(htmfile, optarg, sizeof(htmfile));
            break;

         // arg -h usage, type: flag, optional
         case 'h':
            usage(); exit(0);
            break;

         // arg -v verbose
         case 'v':
            verbose = 1; break;

         case '?':
            if(isprint (optopt))
               printf ("Error: Unknown option `-%c'.\n", optopt);
            else
               printf ("Error: Unknown option character `\\x%x'.\n", optopt);
            usage();
            exit(-1);
            break;

         default:
            usage();
            break;
      }
   }
}

int main(int argc, char *argv[]) {
   int res = -1;       // res = function retcode: 0=OK, -1 = Error
   declination = 0;    // local declination value

   /* ---------------------------------------------------------- *
    * Process the cmdline parameters                             *
    * ---------------------------------------------------------- */
   parseargs(argc, argv);

   /* ----------------------------------------------------------- *
    * get current time (now), output at program start if verbose  *
    * ----------------------------------------------------------- */
   time_t tsnow = time(NULL);
   if(verbose == 1) printf("Debug: ts=[%lld] date=%s", (long long) tsnow, ctime(&tsnow));

   /* ----------------------------------------------------------- *
    * Open the I2C bus and connect to the sensor i2c address 0x30 *
    * ----------------------------------------------------------- */
   get_i2cbus(i2c_bus, I2C_ADDR);

   /* ----------------------------------------------------------- *
    *  "-d" dump the register map content and exit the program    *
    * ----------------------------------------------------------- */
    if(argflag == 1) {
      res = mmc3416_dump();
      if(res != 0) {
         printf("Error: could not dump the register maps.\n");
         exit(-1);
      }
      exit(0);
   }

   /* ----------------------------------------------------------- *
    *  "-i" print sensor information and exit the program         *
    * ----------------------------------------------------------- */
    if(argflag == 2) {
      struct mmc3416inf mmc3416i = {0};
      mmc3416_info(&mmc3416i);

      /* ----------------------------------------------------------- *
       * print the formatted output strings to stdout                *
       * ----------------------------------------------------------- */
      printf("----------------------------------------------\n");
      printf("MMC3416 Information %s", ctime(&tsnow));
      printf("----------------------------------------------\n");
      printf("    Sensor Product ID = 0x%02X ", mmc3416i.prd_id);
      if(mmc3416i.prd_id == 0x06) printf("MEMSIC MMC3416xPJ\n");
      else printf("Product ID unknown\n");

      /* continuous read status from reg 0x07 bit-1 */
      cm_status = ((mmc3416i.ctl_0_mode >> 1) & 0x01);
      printf("Continuous Read State = 0x%02X ", cm_status);
      if(cm_status == 0) printf("Disabled\n");
      else  printf("Enabled\n");

      /* cont read frequency mode from reg 0x07 bit-2 and 3 */
      cmfreq_mode = ((mmc3416i.ctl_0_mode >> 2) & 0x03);
      printf("Continuous Read Freq. = 0x%02X ", cmfreq_mode);
      if(cmfreq_mode == 0x00) printf("1.5 Hz (1 sample every 1.5 seconds)\n");
      else if(cmfreq_mode == 0x01) printf("13 Hz (1 sample every 77 milliseconds)\n");
      else if(cmfreq_mode == 0x02) printf("25 Hz (1 sample every 40 milliseconds\n");
      else if(cmfreq_mode == 0x03) printf("50 Hz (1 sample every 20 milliseconds)\n");

      /* "no Boost" status from reg 0x07 bit-4 */
      noboost_status = ((mmc3416i.ctl_0_mode >> 4) & 0x01);
      printf("No Boost CAP charging = 0x%02X ", noboost_status);
      if(noboost_status == 0) printf("CAP charge pump enabled\n");
      else  printf("CAP charged from VDD\n");

      /* output resolution mode from reg 0x08 bit-0 and 1 */
      outres_mode = mmc3416i.ctl_1_mode & 0x03;
      printf("    Output Resolution = 0x%02X ", outres_mode);
      if(outres_mode == 0x00) printf("16 bit (7.92ms read time)\n");
      else if(outres_mode == 0x01) printf("16 bit (4.08ms read time)\n");
      else if(outres_mode == 0x02) printf("14 bit (2.16ms read time)\n");
      else if(outres_mode == 0x03) printf("12 bit (1.20ms read time)\n");

      exit(0);
   }

   /* ----------------------------------------------------------- *
    *  "-r" reset the sensor and exit the program                 *
    * ----------------------------------------------------------- */
    if(argflag == 3) {
      res = mmc3416_swreset();
      if(res != 0) {
         printf("Error: could not reset the sensor.\n");
         exit(-1);
      }
      exit(0);
   }

   /* ----------------------------------------------------------- *
    *  "-t" read single measurement, then exit the program        *
    * ----------------------------------------------------------- */
   if(argflag == 4) {
      struct mmc3416data mmc3416d;

      mmc3416_init(&mmc3416d);
      res = mmc3416_read(&mmc3416d);
      if(res != 0) {
         printf("Error: could not read data from the sensor.\n");
         exit(-1);
      }
      float angle = get_heading(&mmc3416d);
      /* ----------------------------------------------------------- *
       * print the formatted output string to stdout (Example below) *
       * 1584280335 Heading=337.2 degrees                            *
       * Note the sensor has a accuracy of +/-1 degree, fractions    *
       * don't make much sense. Consider taking them off...          *
       * ----------------------------------------------------------- */
         printf("%lld Heading=%3.1f degrees\n", (long long) tsnow, angle);
      exit(0);
   }

   /* ----------------------------------------------------------- *
    *  "-c" start continuous read with given frequency. Run until *
    * ctl-c is received.                                          *
    * ----------------------------------------------------------- */
   if(argflag == 5) {
      res = set_cmfreq(cmfreq_mode);
      if(res != 0) {
         printf("Error: could not set continuous mode %d.\n", cmfreq_mode);
         exit(-1);
      }
      exit(0);
   }
}
