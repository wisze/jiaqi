/**
   Some constants read from a configuration file
*/
const char* ssid = "********";
const char* password = "********";
const char* sitename = "stof";

/*****************************************************************
   To get time from an NTP server
 **/
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;   //Replace with your GMT offset (seconds)
const int   daylightOffset_sec = 3600;  //Replace with your daylight offset (seconds
time_t epoch, epoch0; // Unix time in seconds since 1970
time_t sampleTime, lastCheck;

/****************************************
   Time series arrays
   Latitude, longitude hard coded for now
*/
float pm10, pm25, temperature, humidity;
int isample = 0;
const int nsamples = 288;
float pm25TS[nsamples];
float pm10TS[nsamples];
String timeTS[nsamples];

const int waitfor = 1000;
const int sampleinterval = 5; // sample every 5 minutes
