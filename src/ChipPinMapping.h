typedef unsigned char byte;
//YM_DATA
byte YM_DATA[8] = {0,1,2,3,4,5,6,7};

//YM_CONTROL
#define YM_IC 38
#define YM_CS 37
#define YM_WR 36
#define YM_RD 35
#define YM_A0 34
#define YM_A1 33

//SN_DATA
byte SN_DATA[8] = {24, 25, 26, 27, 28, 29, 30, 31};

//SN_CONTROL
#define SN_WE 39

//LTC6903 External Clocks
//#define SN_CLOCK_CS 32
//#define YM_CLOCK_CS 14

//External Buttons
#define FWD_BTN 23
#define RNG_BTN 22
#define PRV_BTN 21
#define PSE_BTN 20

//UART Bluetooth module
//Serial2
//#define BT_RX 9
//#define BT_TX 10
