//*********************************************************************************************************
//*    ESP32 MatrixClock                                                                                  *
//*********************************************************************************************************
//
// first release on 01/2019
// updated on    21.01.2019
// Version 1.1.0
//
//
// THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT.
// FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
// OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
//
//
// system libraries
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <SPI.h>
#include <Ticker.h>
#include <time.h>
#include "apps/sntp/sntp.h"     // #warning "This header file is deprecated, please include lwip/apps/sntp.h instead." [-Wcpp]
//#include "lwip/apps/sntp.h"   // espressif esp32/arduino V1.0.1-rc2 or higher

// Digital I/O used
#define SPI_MOSI      23
#define SPI_MISO      19    // not connected
#define SPI_SCK       18
#define MAX_CS        15

// Credentials ----------------------------------------
#define SSID         "mySSID";                      // Your WiFi credentials here
#define PW           "myWiFiPassword";

// Timezone -------------------------------------------
#define TZName       "CET-1CEST,M3.5.0,M10.5.0/3"   // Berlin (examples see at the bottom)
//#define TZName     "GMT0BST,M3.5.0/1,M10.5.0"     // London
//#define TZName     "IST-5:30"                     // New Delhi

// other defines --------------------------------------
#define BRIGHTNESS   7     // values can be 0...15
#define anzMAX       6     // number of cascaded MAX7219
#define FORMAT24H          // if not defined time will be displayed in 12h fromat
#define SCROLLDOWN         // if not defined it scrolls up
//-----------------------------------------------------
//global variables
String  _SSID = "";
String  _PW   = "";
String  _myIP = "0.0.0.0";
boolean _f_rtc = false;                // true if time from ntp is received
uint8_t _maxPosX = anzMAX * 8 - 1;      // calculated maxposition
uint8_t _LEDarr[anzMAX][8];             // character matrix to display (40*8)
uint8_t _helpArrMAX[anzMAX * 8];        // helperarray for chardecoding
uint8_t _helpArrPos[anzMAX * 8];        // helperarray pos of chardecoding
boolean _f_tckr1s = false;
boolean _f_tckr50ms = false;
boolean _f_tckr24h = false;
int16_t _zPosX = 0;                     //xPosition (display the time)
int16_t _dPosX = 0;                     //xPosition (display the date)
boolean _f_updown = false;              //scroll direction

String M_arr[12] = {"Jan.", "Feb.", "Mar.", "Apr.", "May", "June", "July", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
String WD_arr[7] = {"Sun,", "Mon,", "Tue,", "Wed,", "Thu,", "Fri,", "Sat,"};


// Font 5x8 for 8x8 matrix, 0,0 is above right
const uint8_t font_t[96][9] = { // font only for the time
        { 0x07, 0x1c, 0x22, 0x26, 0x2a, 0x32, 0x22, 0x1c, 0x00 },   // 0x30, 0
        { 0x07, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00 },   // 0x31, 1
        { 0x07, 0x1c, 0x22, 0x02, 0x04, 0x08, 0x10, 0x3e, 0x00 },   // 0x32, 2
        { 0x07, 0x1c, 0x22, 0x02, 0x0c, 0x02, 0x22, 0x1c, 0x00 },   // 0x33, 3
        { 0x07, 0x04, 0x0c, 0x14, 0x24, 0x3e, 0x04, 0x04, 0x00 },   // 0x34, 4
        { 0x07, 0x3e, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00 },   // 0x35, 5
        { 0x07, 0x0c, 0x10, 0x20, 0x3c, 0x22, 0x22, 0x1c, 0x00 },   // 0x36, 6
        { 0x07, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00 },   // 0x37, 7
        { 0x07, 0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00 },   // 0x38, 8
        { 0x07, 0x1c, 0x22, 0x22, 0x1e, 0x02, 0x04, 0x18, 0x00 },   // 0x39, 9
        { 0x04, 0x00, 0x06, 0x06, 0x00, 0x06, 0x06, 0x00, 0x00 },   // 0x3a, :

};

const uint8_t font_u[96][9] = { // universal proportional font
        { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x20, Space  *
        { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 },   // 0x21, !      *
        { 0x05, 0x09, 0x09, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x22, "      *
        { 0x05, 0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a, 0x00 },   // 0x23, #      *
        { 0x05, 0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04, 0x00 },   // 0x24, $      *
        { 0x05, 0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13, 0x00 },   // 0x25, %      *
        { 0x06, 0x0c, 0x22, 0x14, 0x18, 0x25, 0x22, 0x1d, 0x00 },   // 0x26, &      *
        { 0x02, 0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x27, '      *
        { 0x03, 0x01, 0x02, 0x04, 0x04, 0x04, 0x02, 0x01, 0x00 },   // 0x28, (      *
        { 0x03, 0x04, 0x02, 0x01, 0x01, 0x01, 0x02, 0x04, 0x00 },   // 0x29, )      *
        { 0x05, 0x04, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x04, 0x00 },   // 0x2a, *      *
        { 0x05, 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00, 0x00 },   // 0x2b, +      *
        { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02 },   // 0x2c, ,      *
        { 0x05, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00 },   // 0x2d, -      *
        { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00 },   // 0x2e, .      *
        { 0x05, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10, 0x00 },   // 0x2f, /      *
        { 0x05, 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e, 0x00 },   // 0x30, 0      *
        { 0x03, 0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0x31, 1      *
        { 0x05, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x32, 2      *
        { 0x05, 0x0e, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0e, 0x00 },   // 0x33, 3      *
        { 0x05, 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02, 0x00 },   // 0x34, 4      *
        { 0x05, 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e, 0x00 },   // 0x35, 5      *
        { 0x05, 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e, 0x00 },   // 0x36, 6      *
        { 0x05, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08, 0x00 },   // 0x37, 7      *
        { 0x05, 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e, 0x00 },   // 0x38, 8      *
        { 0x05, 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c, 0x00 },   // 0x39, 9      *
        { 0x02, 0x00, 0x03, 0x03, 0x00, 0x03, 0x03, 0x00, 0x00 },   // 0x3a, :      *
        { 0x02, 0x00, 0x00, 0x03, 0x03, 0x00, 0x03, 0x01, 0x02 },   // 0x3b, ;      *
        { 0x04, 0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01, 0x00 },   // 0x3c, <      *
        { 0x05, 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00, 0x00 },   // 0x3d, =      *
        { 0x04, 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08, 0x00 },   // 0x3e, >      *
        { 0x05, 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04, 0x00 },   // 0x3f, ?      *
        { 0x05, 0x0e, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0f, 0x00 },   // 0x40, @      *
        { 0x05, 0x04, 0x0a, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x00 },   // 0x41, A      *
        { 0x05, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e, 0x00 },   // 0x42, B      *
        { 0x05, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e, 0x00 },   // 0x43, C      *
        { 0x05, 0x1e, 0x09, 0x09, 0x09, 0x09, 0x09, 0x1e, 0x00 },   // 0x44, D      *
        { 0x05, 0x1f, 0x10, 0x10, 0x1c, 0x10, 0x10, 0x1f, 0x00 },   // 0x45, E      *
        { 0x05, 0x1f, 0x10, 0x10, 0x1f, 0x10, 0x10, 0x10, 0x00 },   // 0x46, F      *
        { 0x05, 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f, 0x00 },   // 0x37, G      *
        { 0x05, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11, 0x00 },   // 0x48, H      *
        { 0x05, 0x07, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0x49, I      *
        { 0x05, 0x1f, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c, 0x00 },   // 0x4a, J      *
        { 0x05, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11, 0x00 },   // 0x4b, K      *
        { 0x05, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f, 0x00 },   // 0x4c, L      *
        { 0x05, 0x11, 0x1b, 0x15, 0x11, 0x11, 0x11, 0x11, 0x00 },   // 0x4d, M      *
        { 0x05, 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x00 },   // 0x4e, N      *
        { 0x05, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x4f, O      *
        { 0x05, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10, 0x00 },   // 0x50, P      *
        { 0x05, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d, 0x00 },   // 0x51, Q      *
        { 0x05, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11, 0x00 },   // 0x52, R      *
        { 0x05, 0x0e, 0x11, 0x10, 0x0e, 0x01, 0x11, 0x0e, 0x00 },   // 0x53, S      *
        { 0x05, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x54, T      *
        { 0x05, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x55, U      *
        { 0x05, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x56, V      *
        { 0x05, 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11, 0x00 },   // 0x57, W      *
        { 0x05, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11, 0x00 },   // 0x58, X      *
        { 0x05, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x00 },   // 0x59, Y      *
        { 0x05, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f, 0x00 },   // 0x5a, Z      *
        { 0x03, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x07, 0x00 },   // 0x5b, [      *
        { 0x05, 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01, 0x00 },   // 0x5c, '\'    *
        { 0x03, 0x07, 0x01, 0x01, 0x01, 0x01, 0x01, 0x07, 0x00 },   // 0x5d, ]      *
        { 0x05, 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x5e, ^      *
        { 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x00 },   // 0x5f, _      *
        { 0x02, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },   // 0x60, `      *
        { 0x05, 0x00, 0x0e, 0x01, 0x0d, 0x13, 0x13, 0x0d, 0x00 },   // 0x61, a      *
        { 0x05, 0x10, 0x10, 0x16, 0x19, 0x11, 0x19, 0x16, 0x00 },   // 0x62, b      *
        { 0x05, 0x00, 0x00, 0x07, 0x08, 0x08, 0x08, 0x07, 0x00 },   // 0x63, c      *
        { 0x05, 0x01, 0x01, 0x0d, 0x13, 0x11, 0x13, 0x0d, 0x00 },   // 0x64, d      *
        { 0x05, 0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0e, 0x00 },   // 0x65, e      *
        { 0x05, 0x06, 0x09, 0x08, 0x1c, 0x08, 0x08, 0x08, 0x00 },   // 0x66, f      *
        { 0x05, 0x00, 0x0e, 0x0f, 0x11, 0x11, 0x0f, 0x01, 0x0e },   // 0x67, g      *
        { 0x05, 0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x68, h      *
        { 0x03, 0x00, 0x02, 0x00, 0x06, 0x02, 0x02, 0x07, 0x00 },   // 0x69, i      *
        { 0x04, 0x00, 0x01, 0x00, 0x03, 0x01, 0x01, 0x09, 0x06 },   // 0x6a, j      *
        { 0x04, 0x08, 0x08, 0x09, 0x0a, 0x0c, 0x0a, 0x09, 0x00 },   // 0x6b, k      *
        { 0x03, 0x06, 0x02, 0x02, 0x02, 0x02, 0x02, 0x07, 0x00 },   // 0x6c, l      *
        { 0x05, 0x00, 0x00, 0x1a, 0x15, 0x15, 0x11, 0x11, 0x00 },   // 0x6d, m      *
        { 0x05, 0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x00 },   // 0x6e, n      *
        { 0x05, 0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e, 0x00 },   // 0x6f, o      *
        { 0x04, 0x00, 0x00, 0x0e, 0x09, 0x09, 0x0e, 0x08, 0x08 },   // 0x70, p      *
        { 0x04, 0x00, 0x00, 0x07, 0x09, 0x09, 0x07, 0x01, 0x01 },   // 0x71, q      *
        { 0x05, 0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10, 0x00 },   // 0x72, r      *
        { 0x05, 0x00, 0x00, 0x0e, 0x10, 0x0e, 0x01, 0x1e, 0x00 },   // 0x73, s      *
        { 0x05, 0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06, 0x00 },   // 0x74, t      *
        { 0x05, 0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d, 0x00 },   // 0x75, u      *
        { 0x05, 0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04, 0x00 },   // 0x76, v      *
        { 0x05, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0a, 0x00 },   // 0x77, w      *
        { 0x05, 0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x00 },   // 0x78, x      *
        { 0x05, 0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x11, 0x0e },   // 0x79, y      *
        { 0x05, 0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f, 0x00 },   // 0x7a, z      *
        { 0x04, 0x03, 0x04, 0x04, 0x08, 0x04, 0x04, 0x03, 0x00 },   // 0x7b, {      *
        { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 },   // 0x7c, |      *
        { 0x04, 0x0c, 0x02, 0x02, 0x01, 0x02, 0x02, 0x0c, 0x00 },   // 0x7d, }      *
        { 0x05, 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00, 0x00 },   // 0x7e, ~      *
        { 0x05, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x00 }    // 0x7f, DEL    *
};

//*********************************************************************************************************
//*    Class RTIME                                                                                        *
//*********************************************************************************************************
//
extern __attribute__((weak)) void RTIME_info(const char*);

class RTIME{

public:
    RTIME();
    ~RTIME();
    boolean begin(String TimeZone="CET-1CEST,M3.5.0,M10.5.0/3");
    const char* gettime();
    const char* gettime_l();
    const char* gettime_s();
    const char* gettime_xs();
    uint8_t getweekday();
    uint8_t getyear();
    uint8_t getmonth();
    uint8_t getday();
    uint8_t gethour();
    uint8_t getminute();
    uint8_t getsecond();
protected:
    boolean obtain_time();
private:
    char sbuf[256];
    String RTIME_TZ="";
    struct tm timeinfo;
    time_t now;
    char strftime_buf[64];
    String w_day_l[7]={"Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"};
    String w_day_s[7]={"So","Mo","Di","Mi","Do","Fr","Sa"};
    String month_l[12]={"Januar","Februar","März","April","Mai","Juni","Juli","August","September","Oktober","November","Dezember"};
    String month_s[12]={"Jan","Feb","März","Apr","Mai","Jun","Jul","Sep","Okt","Nov","Dez"};
};


RTIME::RTIME(){
    timeinfo = { 0,0,0,0,0,0,0,0,0 };
    now=0;
}
RTIME::~RTIME(){
    sntp_stop();
}
boolean RTIME::begin(String TimeZone){
    RTIME_TZ=TimeZone;
    if (RTIME_info) RTIME_info("Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    char sbuf[20]="pool.ntp.org";
    sntp_setservername(0, sbuf);
    sntp_init();
    return obtain_time();
}

boolean RTIME::obtain_time(){
    time_t now = 0;
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        sprintf(sbuf, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        if (RTIME_info) RTIME_info(sbuf);
        vTaskDelay(uint16_t(2000 / portTICK_PERIOD_MS));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    //setenv("TZ","CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1); // automatic daylight saving time
    setenv("TZ", RTIME_TZ.c_str(), 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    if (RTIME_info) RTIME_info(strftime_buf);

    //log_i( "The current date/time in Berlin is: %s", strftime_buf);
    if(retry < retry_count) return true;
    else return false;
}

const char* RTIME::gettime(){
    setenv("TZ", RTIME_TZ.c_str(), 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    //log_i( "The current date/time in Berlin is: %s", strftime_buf);
    return strftime_buf;
}

const char* RTIME::gettime_l(){  // Montag, 04. August 2017 13:12:44
    time(&now);
    localtime_r(&now, &timeinfo);
//    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
//    log_i( "The current date/time in Beriln is: %s", strftime_buf);
    sprintf(strftime_buf,"%s, %02d.%s ", w_day_l[timeinfo.tm_wday].c_str(), timeinfo.tm_mday, month_l[timeinfo.tm_mon].c_str());
    sprintf(strftime_buf,"%s%d %02d:", strftime_buf, timeinfo.tm_year+1900, timeinfo.tm_hour);
    sprintf(strftime_buf,"%s%02d:%02d ", strftime_buf, timeinfo.tm_min, timeinfo.tm_sec);
    return strftime_buf;
}

const char* RTIME::gettime_s(){  // hh:mm:ss
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(strftime_buf,"%02d:%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return strftime_buf;
}

const char* RTIME::gettime_xs(){  // hh:mm
    time(&now);
    localtime_r(&now, &timeinfo);
    sprintf(strftime_buf,"%02d:%02d",  timeinfo.tm_hour, timeinfo.tm_min);
    return strftime_buf;
}

uint8_t RTIME::getweekday(){ //So=0, Mo=1 ... Sa=6
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_wday;
}

uint8_t RTIME::getyear(){ // 0...99
    time(&now);
    localtime_r(&now, &timeinfo);
    return (timeinfo.tm_year -100);
}

uint8_t RTIME::getmonth(){ // 0...11
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_mon;
}

uint8_t RTIME::getday(){ // 1...31
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_mday;
}

uint8_t RTIME::gethour(){ // 0...23  (0...12)
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_hour;
}

uint8_t RTIME::getminute(){ // 0...59
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_min;
}

uint8_t RTIME::getsecond(){ // 0...59
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_sec;
}

//*********************************************************************************************************

//objects
RTIME rtc;
WiFiMulti wifiMulti;
Ticker tckr;

//events
void RTIME_info(const char *info){
    Serial.printf("rtime_info : %s\n", info);
}
//*************************************************************************************************
const uint8_t InitArr[7][2] = { { 0x0C, 0x00 },    // display off
        { 0x00, 0xFF },    // no LEDtest
        { 0x09, 0x00 },    // BCD off
        { 0x0F, 0x00 },    // normal operation
        { 0x0B, 0x07 },    // start display
        { 0x0A, 0x04 },    // brightness
        { 0x0C, 0x01 }     // display on
};
//*********************************************************************************************************
void helpArr_init(void)  //helperarray init
{
    uint8_t i, j, k;
    j = 0;
    k = 0;
    for (i = 0; i < anzMAX * 8; i++) {
        _helpArrPos[i] = (1 << j);   //bitmask
        _helpArrMAX[i] = k;
        j++;
        if (j > 7) {
            j = 0;
            k++;
        }
    }
}
//*********************************************************************************************************
void max7219_init()  //all MAX7219 init
{
    uint8_t i, j;
    for (i = 0; i < 7; i++) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(InitArr[i][0]);  //register
            SPI.write(InitArr[i][1]);  //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void max7219_set_brightness(unsigned short br)  //brightness MAX7219
{
    uint8_t j;
    if (br < 16) {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = 0; j < anzMAX; j++) {
            SPI.write(0x0A);  //register
            SPI.write(br);    //value
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void clear_Display()   //clear all
{
    uint8_t i, j;
    for (i = 0; i < 8; i++)     //8 rows
    {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = anzMAX; j > 0; j--) {
            _LEDarr[j - 1][i] = 0;       //LEDarr clear
            SPI.write(i + 1);           //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
void refresh_display() //take info into LEDarr
{
    uint8_t i, j;

    for (i = 0; i < 8; i++)     //8 rows
    {
        digitalWrite(MAX_CS, LOW);
        delayMicroseconds(1);
        for (j = anzMAX; j > 0; j--) {
            SPI.write(i + 1);  //current row
            SPI.write(_LEDarr[j - 1][i]);
        }
        digitalWrite(MAX_CS, HIGH);
    }
}
//*********************************************************************************************************
uint8_t char2Arr_t(unsigned short ch, int PosX, short PosY) { //characters into arr, shows only the time
    int i, j, k, l, m, o1, o2, o3, o4=0;
    PosX++;
    k = ch - 0x30;                       //ASCII position in font
    if ((k >= 0) && (k < 11)){           //character found in font?
        o4 = font_t[k][0];               //character width
        o3 = 1 << (o4-1);
        for (i = 0; i < o4; i++) {
            if (((PosX - i <= _maxPosX) && (PosX - i >= 0)) && ((PosY > -8) && (PosY < 8))){ //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    if (((PosY >= 0) && (PosY <= j)) || ((PosY < 0) && (j < PosY + 8))){ //scroll vertical
                        l = font_t[k][j + 1];
                        m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
                        if (m > 0)
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] | (o1);  //set point
                        else
                            _LEDarr[o2][j - PosY] = _LEDarr[o2][j - PosY] & (~o1); //clear point
                    }
                }
            }
        }
    }
    return o4;
}
//*********************************************************************************************************
uint8_t char2Arr_p(unsigned short ch, int PosX) { //characters into arr, proportional font
    int i, j, k, l, m, o1, o2, o3, o4=0;
    PosX++;
    k = ch - 32;                        //ASCII position in font
    if ((k >= 0) && (k < 96)){          //character found in font?
        o4 = font_u[k][0];              //character width
        o3 = 1 << (o4 - 1);
        for (i = 0; i < o4; i++) {
            if ((PosX - i <= _maxPosX) && (PosX - i >= 0)){ //within matrix?
                o1 = _helpArrPos[PosX - i];
                o2 = _helpArrMAX[PosX - i];
                for (j = 0; j < 8; j++) {
                    l = font_u[k][j + 1];
                    m = (l & (o3 >> i));  //e.g. o4=7  0zzzzz0, o4=4  0zz0
                    if (m > 0)  _LEDarr[o2][j] = _LEDarr[o2][j] | (o1);  //set point
                    else        _LEDarr[o2][j] = _LEDarr[o2][j] & (~o1); //clear point
                }
            }
        }
    }
    return o4;
}
//*********************************************************************************************************
uint16_t scrolltext(int16_t posX, String txt)
{
    int16_t p=0;
    for(int i=0; i<txt.length(); i++){
        p+=char2Arr_p(txt.charAt(i), posX - p);
        p+=char2Arr_p(' ', posX - p);
        if(txt.charAt(i)==' ') p+=2;
    }
    return p;
}
//*********************************************************************************************************
void timer50ms()
{
    static unsigned int cnt50ms = 0;
    static unsigned int cnt1s = 0;
    static unsigned int cnt1h = 0;
    _f_tckr50ms = true;
    cnt50ms++;
    if (cnt50ms == 20) {
        _f_tckr1s = true; // 1 sec
        cnt1s++;
        cnt50ms = 0;
    }
    if (cnt1s == 3600) { // 1h
        cnt1h++;
        cnt1s = 0;
    }
    if (cnt1h == 24) { // 1d
        _f_tckr24h = true;
        cnt1h = 0;
    }
}
//*********************************************************************************************************
void setup()
{
    Serial.begin(115200); // For debug
    pinMode(MAX_CS, OUTPUT);
    digitalWrite(MAX_CS, HIGH);
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    WiFi.disconnect();
    WiFi.mode(WIFI_MODE_STA);
    _SSID=SSID;
    _PW=PW;
    wifiMulti.addAP(_SSID.c_str(), _PW.c_str());                // SSID and PW in code
    //wifiMulti.addAP(s_ssid.c_str(), s_password.c_str());      // more credential can be added here
    Serial.println("WiFI_info  : Connecting WiFi...");
    if(wifiMulti.run()==WL_CONNECTED){
        _myIP=WiFi.localIP().toString();
        Serial.printf("WiFI_info  : WiFi connected\n");
        Serial.printf("WiFI_info  : IP address %s\n", _myIP.c_str());
    }else{
        Serial.printf("WiFi credentials are not correct\n");
        _SSID = ""; _myIP="0.0.0.0";
        while(true){;} // endless loop
    }
    helpArr_init();
    max7219_init();
    clear_Display();
    max7219_set_brightness(BRIGHTNESS);
    _f_rtc= rtc.begin(TZName);
    if(!_f_rtc) Serial.println("no timepacket received from ntp");
    tckr.attach(0.05, timer50ms);    // every 50 msec
}
//*********************************************************************************************************
void loop()
{
    uint8_t sek1 = 0, sek2 = 0, min1 = 0, min2 = 0, std1 = 0, std2 = 0;
    uint8_t sek11 = 0, sek12 = 0, sek21 = 0, sek22 = 0;
    uint8_t min11 = 0, min12 = 0, min21 = 0, min22 = 0;
    uint8_t std11 = 0, std12 = 0, std21 = 0, std22 = 0;
    signed int x = 0; //x1,x2;
    signed int y = 0, y1 = 0, y2 = 0;
    unsigned int sc1 = 0, sc2 = 0, sc3 = 0, sc4 = 0, sc5 = 0, sc6 = 0;
    static int16_t sctxtlen=0;
    boolean f_scrollend_y = false;
    boolean f_scroll_x1 = false;
    boolean f_scroll_x2 = false;

#ifdef SCROLLDOWN
    _f_updown = true;
#else
    _f_updown = false;
#endif //SCROLLDOWN

    _zPosX = _maxPosX;
    _dPosX = -8;
    //  x=0; x1=0; x2=0;

    refresh_display();
    if (_f_updown == false) {
        y2 = -9;
        y1 = 8;
    }
    if (_f_updown == true) { //scroll  up to down
        y2 = 8;
        y1 = -8;
    }
    while (true) {
        if(_f_tckr24h == true) { //syncronisize RTC every day
            _f_tckr24h = false;
            _f_rtc= rtc.begin(TZName);
            if(_f_rtc==false) Serial.println("no timepacket received");
        }
        if (_f_tckr1s == true)        // flag 1sek
        {
            sek1 = (rtc.getsecond()%10);
            sek2 = (rtc.getsecond()/10);
            min1 = (rtc.getminute()%10);
            min2 = (rtc.getminute()/10);
#ifdef FORMAT24H
            std1 = (rtc.gethour()%10);  // 24 hour format
            std2 = (rtc.gethour()/10);
#else
            uint8_t h=rtc.gethour();    // convert to 12 hour format
            if(h>12) h-=12;
            std1 = (h%10);
            std2 = (h/10);
#endif //FORMAT24H

            y = y2;                 //scroll updown
            sc1 = 1;
            sek1++;
            if (sek1 == 10) {
                sc2 = 1;
                sek2++;
                sek1 = 0;
            }
            if (sek2 == 6) {
                min1++;
                sek2 = 0;
                sc3 = 1;
            }
            if (min1 == 10) {
                min2++;
                min1 = 0;
                sc4 = 1;
            }
            if (min2 == 6) {
                std1++;
                min2 = 0;
                sc5 = 1;
            }
            if (std1 == 10) {
                std2++;
                std1 = 0;
                sc6 = 1;
            }
            if ((std2 == 2) && (std1 == 4)) {
                std1 = 0;
                std2 = 0;
                sc6 = 1;
            }

            sek11 = sek12;
            sek12 = sek1;
            sek21 = sek22;
            sek22 = sek2;
            min11 = min12;
            min12 = min1;
            min21 = min22;
            min22 = min2;
            std11 = std12;
            std12 = std1;
            std21 = std22;
            std22 = std2;
            _f_tckr1s = false;
            if (rtc.getsecond() == 45) f_scroll_x1 = true; // scroll ddmmyy
//          if (rtc.getsecond() == 25) f_scroll_x2 = true; // scroll userdefined text
        } // end 1s
// ----------------------------------------------
        if (_f_tckr50ms == true) {
            _f_tckr50ms = false;
//          -------------------------------------
            if (f_scroll_x1 == true) {
                _zPosX++;
                _dPosX++;
                if (_dPosX == sctxtlen)  _zPosX = 0;
                if (_zPosX == _maxPosX){f_scroll_x1 = false; _dPosX = -8;}
            }
//          -------------------------------------
            if (f_scroll_x2 == true) {
                _zPosX++;
                _dPosX++;
                if (_dPosX == sctxtlen)  _zPosX = 0;
                if (_zPosX == _maxPosX){f_scroll_x2 = false; _dPosX = -8;}
            }
//          -------------------------------------
            if (sc1 == 1) {
                if (_f_updown == 1) y--;
                else                y++;
                char2Arr_t(48 + sek12, _zPosX - 42, y);
                char2Arr_t(48 + sek11, _zPosX - 42, y + y1);
                if (y == 0) {sc1 = 0; f_scrollend_y = true;}
            }
            else char2Arr_t(48 + sek1, _zPosX - 42, 0);
//          -------------------------------------
            if (sc2 == 1) {
                char2Arr_t(48 + sek22, _zPosX - 36, y);
                char2Arr_t(48 + sek21, _zPosX - 36, y + y1);
                if (y == 0) sc2 = 0;
            }
            else char2Arr_t(48 + sek2, _zPosX - 36, 0);
            char2Arr_t(':', _zPosX - 32, 0);
//          -------------------------------------
            if (sc3 == 1) {
                char2Arr_t(48 + min12, _zPosX - 25, y);
                char2Arr_t(48 + min11, _zPosX - 25, y + y1);
                if (y == 0) sc3 = 0;
            }
            else char2Arr_t(48 + min1, _zPosX - 25, 0);
//          -------------------------------------
            if (sc4 == 1) {
                char2Arr_t(48 + min22, _zPosX - 19, y);
                char2Arr_t(48 + min21, _zPosX - 19, y + y1);
                if (y == 0) sc4 = 0;
            }
            else char2Arr_t(48 + min2, _zPosX - 19, 0);
            char2Arr_t(':', _zPosX - 15 + x, 0);
//          -------------------------------------
            if (sc5 == 1) {
                char2Arr_t(48 + std12, _zPosX - 8, y);
                char2Arr_t(48 + std11, _zPosX - 8, y + y1);
                if (y == 0) sc5 = 0;
            }
            else char2Arr_t(48 + std1, _zPosX - 8, 0);
//          -------------------------------------
            if (sc6 == 1) {
                char2Arr_t(48 + std22, _zPosX - 2, y);
                char2Arr_t(48 + std21, _zPosX - 2, y + y1);
                if (y == 0) sc6 = 0;
            }
            else char2Arr_t(48 + std2, _zPosX - 2, 0);
//          -------------------------------------
            if(f_scroll_x1){ // day month year
                String txt= "   ";
                txt += WD_arr[rtc.getweekday()] + " ";
                txt += String(rtc.getday()) + ". ";
                txt += M_arr[rtc.getmonth()] + " ";
                txt += "20" + String(rtc.getyear()) + "   ";
                sctxtlen=scrolltext(_dPosX, txt);
            }
//          -------------------------------------
            if(f_scroll_x2){ // user defined text
                sctxtlen=scrolltext(_dPosX, "   Hello World   ");
            }
//          -------------------------------------
            refresh_display(); //all 50msec
            if (f_scrollend_y == true) f_scrollend_y = false;
        } //end 50ms
// -----------------------------------------------
        if (y == 0) {
            // do something else
        }
    }  //end while(true)
    //this section can not be reached
}
//*********************************************************************************************************
//    Examples for time zones                                                                             *
//*********************************************************************************************************
//    UTC                       GMT0
//    Africa/Abidjan            GMT0
//    Africa/Accra              GMT0
//    Africa/Addis_Ababa        EAT-3
//    Africa/Algiers            CET-1
//    Africa/Blantyre, Harare   CAT-2
//    Africa/Cairo              EEST
//    Africa/Casablanca         WET0
//    Africa/Freetown           GMT0
//    Africa/Johannesburg       SAST-2
//    Africa/Kinshasa           WAT-1
//    Africa/Lome               GMT0
//    Africa/Maseru             SAST-2
//    Africa/Mbabane            SAST-2
//    Africa/Nairobi            EAT-3
//    Africa/Tripoli            EET-2
//    Africa/Tunis              CET-1CEST,M3.5.0,M10.5.0/3
//    Africa/Windhoek           WAT-1WAST,M9.1.0,M4.1.0
//    America/Adak              HAST10HADT,M3.2.0,M11.1.0
//    America/Alaska            AKST9AKDT,M3.2.0,M11.1.0
//    America/Anguilla,Dominica AST4
//    America/Araguaina         BRT3
//    Argentina/San_Luis        ART3
//    America/Asuncion          PYT4PYST,M10.3.0/0,M3.2.0/0
//    America/Atka              HAST10HADT,M3.2.0,M11.1.0
//    America/Boa_Vista         AMT4
//    America/Bogota            COT5
//    America/Campo_Grande      AMT4AMST,M10.2.0/0,M2.3.0/0
//    America/Caracas           VET4:30
//    America/Catamarca         ART3ARST,M10.1.0/0,M3.3.0/0
//    America/Cayenne           GFT3
//    America/Chicago           CST6CDT,M3.2.0,M11.1.0
//    America/Costa_Rica        CST6
//    America/Los_Angeles       PST8PDT,M3.2.0,M11.1.0
//    America/Dawson_Creek      MST7
//    America/Denver            MST7MDT,M3.2.0,M11.1.0
//    America/Detroit           EST5EDT,M3.2.0,M11.1.0
//    America/Eirunepe          ACT5
//    America/Godthab           WGST
//    America/Guayaquil         ECT5
//    America/Guyana            GYT4
//    America/Havana            CST5CDT,M3.3.0/0,M10.5.0/1
//    America/Hermosillo        MST7
//    America/Jamaica           EST5
//    America/La_Paz            BOT4
//    America/Lima              PET5
//    America/Miquelon          PMST3PMDT,M3.2.0,M11.1.0
//    America/Montevideo        UYT3UYST,M10.1.0,M3.2.0
//    America/Noronha           FNT2
//    America/Paramaribo        SRT3
//    America/Phoenix           MST7
//    America/Santiago          CLST
//    America/Sao_Paulo         BRT3BRST,M10.2.0/0,M2.3.0/0
//    America/Scoresbysund      EGT1EGST,M3.5.0/0,M10.5.0/1
//    America/St_Johns          NST3:30NDT,M3.2.0/0:01,M11.1.0/0:01
//    America/Toronto           EST5EDT,M3.2.0,M11.1.0
//    Antarctica/Casey          WST-8
//    Antarctica/Davis          DAVT-7
//    Antarctica/DumontDUrville DDUT-10
//    Antarctica/Mawson         MAWT-6
//    Antarctica/McMurdo        NZST-12NZDT,M9.5.0,M4.1.0/3
//    Antarctica/Palmer         CLST
//    Antarctica/Rothera        ROTT3
//    Antarctica/South_Pole     NZST-12NZDT,M9.5.0,M4.1.0/3
//    Antarctica/Syowa          SYOT-3
//    Antarctica/Vostok         VOST-6
//    Arctic/Longyearbyen       CET-1CEST,M3.5.0,M10.5.0/3
//    Argentina/Buenos_Aires    ART3ARST,M10.1.0/0,M3.3.0/0
//    Asia/Almaty               ALMT-6
//    Asia/Amman                EET-2EEST,M3.5.4/0,M10.5.5/1
//    Asia/Anadyr               ANAT-12ANAST,M3.5.0,M10.5.0/3
//    Asia/Aqtau, Aqtobe        AQTT-5
//    Asia/Ashgabat             TMT-5
//    Asia/Ashkhabad            TMT-5
//    Asia/Baku                 AZT-4AZST,M3.5.0/4,M10.5.0/5
//    Asia/Bangkok              ICT-7
//    Asia/Bishkek              KGT-6
//    Asia/Brunei               BNT-8
//    Asia/Calcutta             IST-5:30
//    Asia/Choibalsan           CHOT-9
//    Asia/Chongqing            CST-8
//    Asia/Colombo              IST-5:30
//    Asia/Dacca                BDT-6
//    Asia/Damascus             EET-2EEST,M4.1.5/0,J274/0
//    Asia/Dili                 TLT-9
//    Asia/Dubai                GST-4
//    Asia/Dushanbe             TJT-5
//    Asia/Gaza                 EET-2EEST,J91/0,M9.2.4
//    Asia/Ho_Chi_Minh          ICT-7
//    Asia/Hong_Kong            HKT-8
//    Asia/Hovd                 HOVT-7
//    Asia/Irkutsk              IRKT-8IRKST,M3.5.0,M10.5.0/3
//    Asia/Jakarta, Pontianak   WIT-7
//    Asia/Jayapura             EIT-9
//    Asia/Jerusalem            IDDT
//    Asia/Kabul                AFT-4:30
//    Asia/Kamchatka            PETT-12PETST,M3.5.0,M10.5.0/3
//    Asia/Karachi              PKT-5
//    Asia/Katmandu             NPT-5:45
//    Asia/Kolkata              IST-5:30
//    Asia/Krasnoyarsk          KRAT-7KRAST,M3.5.0,M10.5.0/3
//    Asia/Kuala_Lumpur         MYT-8
//    Asia/Kuching              MYT-8
//    Asia/Kuwait, Bahrain      AST-3
//    Asia/Magadan              MAGT-11MAGST,M3.5.0,M10.5.0/3
//    Asia/Makassar             CIT-8
//    Asia/Manila               PHT-8
//    Asia/Mideast/Riyadh87     zzz-3:07:04
//    Asia/Muscat               GST-4
//    Asia/Novosibirsk          NOVT-6NOVST,M3.5.0,M10.5.0/3
//    Asia/Omsk                 OMST-6OMSST,M3.5.0,M10.5.0/3
//    Asia/Oral                 ORAT-5
//    Asia/Phnom_Penh           ICT-7
//    Asia/Pyongyang            KST-9
//    Asia/Qyzylorda            QYZT-6
//    Asia/Rangoon              MMT-6:30
//    Asia/Saigon               ICT-7
//    Asia/Sakhalin             SAKT-10SAKST,M3.5.0,M10.5.0/3
//    Asia/Samarkand            UZT-5
//    Asia/Seoul                KST-9
//    Asia/Singapore            SGT-8
//    Asia/Taipei               CST-8
//    Asia/Tashkent             UZT-5
//    Asia/Tbilisi              GET-4
//    Asia/Tehran               IRDT
//    Asia/Tel_Aviv             IDDT
//    Asia/Thimbu               BTT-6
//    Asia/Thimphu              BTT-6
//    Asia/Tokyo                JST-9
//    Asia/Ujung_Pandang        CIT-8
//    Asia/Ulaanbaatar          ULAT-8
//    Asia/Ulan_Bator           ULAT-8
//    Asia/Urumqi               CST-8
//    Asia/Vientiane            ICT-7
//    Asia/Vladivostok          VLAT-10VLAST,M3.5.0,M10.5.0/3
//    Asia/Yekaterinburg        YAKT-9YAKST,M3.5.0,M10.5.0/3
//    Asia/Yerevan              AMT-4AMST,M3.5.0,M10.5.0/3
//    Atlantic/Azores           AZOT1AZOST,M3.5.0/0,M10.5.0/1
//    Atlantic/Canary           WET0WEST,M3.5.0/1,M10.5.0
//    Atlantic/Cape_Verde       CVT1
//    Atlantic/Jan_Mayen        CET-1CEST,M3.5.0,M10.5.0/3
//    Atlantic/South_Georgia    GST2
//    Atlantic/St_Helena        GMT0
//    Atlantic/Stanley          FKT4FKST,M9.1.0,M4.3.0
//    Australia/Adelaide        CST-9:30CST,M10.1.0,M4.1.0/3
//    Australia/Brisbane        EST-10
//    Australia/Darwin          CST-9:30
//    Australia/Eucla           CWST-8:45
//    Australia/LHI             LHST-10:30LHST-11,M10.1.0,M4.1.0
//    Australia/Lindeman        EST-10
//    Australia/Lord_Howe       LHST-10:30LHST-11,M10.1.0,M4.1.0
//    Australia/Melbourne       EST-10EST,M10.1.0,M4.1.0/3
//    Australia/North           CST-9:30
//    Australia/Perth, West     WST-8
//    Australia/Queensland      EST-10
//    Brazil/Acre               ACT5
//    Brazil/DeNoronha          FNT2
//    Brazil/East               BRT3BRST,M10.2.0/0,M2.3.0/0
//    Brazil/West               AMT4
//    Canada/Central            CST6CDT,M3.2.0,M11.1.0
//    Canada/Eastern            EST5EDT,M3.2.0,M11.1.0
//    Canada/Newfoundland       NST3:30NDT,M3.2.0/0:01,M11.1.0/0:01
//    Canada/Pacific            PST8PDT,M3.2.0,M11.1.0
//    Chile/Continental         CLST
//    Chile/EasterIsland        EASST
//    Europe/Berlin             CET-1CEST,M3.5.0,M10.5.0/3
//    Europe/Athens             EET-2EEST,M3.5.0/3,M10.5.0/4
//    Europe/Belfast            GMT0BST,M3.5.0/1,M10.5.0
//    Europe/Kaliningrad        EET-2EEST,M3.5.0,M10.5.0/3
//    Europe/Lisbon             WET0WEST,M3.5.0/1,M10.5.0
//    Europe/London             GMT0BST,M3.5.0/1,M10.5.0
//    Europe/Minsk              EET-2EEST,M3.5.0,M10.5.0/3
//    Europe/Moscow             MSK-3MSD,M3.5.0,M10.5.0/3
//    Europe/Samara             SAMT-4SAMST,M3.5.0,M10.5.0/3
//    Europe/Volgograd          VOLT-3VOLST,M3.5.0,M10.5.0/3
//    Indian/Chagos             IOT-6
//    Indian/Christmas          CXT-7
//    Indian/Cocos              CCT-6:30
//    Indian/Kerguelen          TFT-5
//    Indian/Mahe               SCT-4
//    Indian/Maldives           MVT-5
//    Indian/Mauritius          MUT-4
//    Indian/Reunion            RET-4
//    Mexico/General            CST6CDT,M4.1.0,M10.5.0
//    Pacific/Apia              WST11
//    Pacific/Auckland          NZST-12NZDT,M9.5.0,M4.1.0/3
//    Pacific/Chatham           CHAST-12:45CHADT,M9.5.0/2:45,M4.1.0/3:45
//    Pacific/Easter            EASST
//    Pacific/Efate             VUT-11
//    Pacific/Enderbury         PHOT-13
//    Pacific/Fakaofo           TKT10
//    Pacific/Fiji              FJT-12
//    Pacific/Funafuti          TVT-12
//    Pacific/Galapagos         GALT6
//    Pacific/Gambier           GAMT9
//    Pacific/Guadalcanal       SBT-11
//    Pacific/Guam              ChST-10
//    Pacific/Honolulu          HST10
//    Pacific/Johnston          HST10
//    Pacific/Kiritimati        LINT-14
//    Pacific/Kosrae            KOST-11
//    Pacific/Kwajalein         MHT-12
//    Pacific/Majuro            MHT-12
//    Pacific/Marquesas         MART9:30
//    Pacific/Midway            SST11
//    Pacific/Nauru             NRT-12
//    Pacific/Niue              NUT11
//    Pacific/Norfolk           NFT-11:30
//    Pacific/Noumea            NCT-11
//    Pacific/Pago_Pago         SST11
//    Pacific/Palau             PWT-9
//    Pacific/Pitcairn          PST8
//    Pacific/Ponape            PONT-11
//    Pacific/Port_Moresby      PGT-10
//    Pacific/Rarotonga         CKT10
//    Pacific/Saipan            ChST-10
//    Pacific/Samoa             SST11
//    Pacific/Tahiti            TAHT10
//    Pacific/Tarawa            GILT-12
//    Pacific/Tongatapu         TOT-13
//    Pacific/Truk              TRUT-10
//    Pacific/Wake              WAKT-12
//    Pacific/Wallis            WFT-12
//    Pacific/Yap               TRUT-10
//    SystemV/HST10             HST10
//    SystemV/MST7              MST7
//    SystemV/PST8              PST8
//    SystemV/YST9              GAMT9
//    US/Aleutian               HAST10HADT,M3.2.0,M11.1.0
//    US/Arizona                MST7
//    US/Eastern                EST5EDT,M3.2.0,M11.1.0
//    US/East-Indiana           EST5EDT,M3.2.0,M11.1.0
//    US/Hawaii                 HST10
//    US/Michigan               EST5EDT,M3.2.0,M11.1.0
//    US/Samoa                  SST11
