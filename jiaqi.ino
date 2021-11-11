/******************************************************************
 * Wemos webserver showing fine particle sensor measurements
 *
 * Uses a Nova SDS011 dust particle sensor to measure PM10 and PM25
 * Uses a DHT22 humidity sensor to flag outliers
 * Sensors are read every n minutes, on the webpage of the device
 * it shows a table with the latest measurements and a graph for
 * the last couple of hours, depending on measurement cycle and
 * length of the time series.
 *
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "SdsDustSensor.h"
#include "jiaqi.h"

#define SDS_PIN_RX D6
#define SDS_PIN_TX D7
#define DHT_PIN    D8

ESP8266WebServer server(80);
WiFiClient webClient;
PubSubClient client(webClient);

SdsDustSensor sds(SDS_PIN_RX, SDS_PIN_TX);

/************************************
 * Setup
 **/
void setup() {
  Serial.begin(9600);
  Serial.println("Start Setup");

  // TODO: Get configuration from config.txt on SD

  // Begin sensors
  sds.begin();
  sds.setCustomWorkingPeriod(sampleinterval);

  // Connect to WiFi network
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(" as ");
  Serial.println(sitename);
  WiFi.hostname(sitename);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.print("Use this URL : ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  server.on("/", []() {
    String message = "<html><head>";
    message += styleHeader();
    message += "</head><body>";
    message += "<table>";
    message += "<p>Sensors</p>";
    message += tableHead("parameter", "value");
    message += tableRow("PM25 ", (String)pm25);
    message += tableRow("PM10 ", (String)pm10);
    message += tableRow("time",   asctime(localtime(&epoch)));
    message += "</table>";

    message += "<p>Measurements over a period of "+(String) (nsamples*sampleinterval)+" minutes.\n";
    message += "One measurement every "+(String) sampleinterval+" minutes";
    message += ", PM25 is green, PM10 is blue.</p>\n";
    message += "<p><center>\n";
    message += graph(nsamples);
    message += "</center>";

    message += "</body></html>";
    server.send(200, "text/html", message);
    Serial.println("Request for / handled");
  });

  // Start the server
  server.begin();
  Serial.println("Server started");
  // MQTT
  // client.setServer(mqtt_server, 1883);

  // udp.begin(localPort);
  // Serial.println(udp.localPort());
  // boolean gottime = getTimeNTP();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  epoch  = getTime();

  // TODO: Get location from geocoder if it was not in the config.txt file

  pinMode(LED_BUILTIN, OUTPUT);
}

/**************************************************
 * Main loop, only gets the time and sensor reading
 */
void loop() {
  /**
   * Get current time
   **/
  struct tm * timeinfo;
  time (&epoch);
  timeinfo = localtime (&epoch);

  /*************************
   * Handle a server request
   **/
  server.handleClient();

  /*******************
   * Get sensor values
   **/
  // getDust();
  PmResult pm = sds.readPm();
  if (pm.isOk()) {
    pm25 = pm.pm25;
    pm10 = pm.pm10;
    Serial.print("PM2.5 = ");
    Serial.print(pm25);
    Serial.print(", PM10 = ");
    Serial.print(pm10);
    // Get the current time
    // epoch  = getTime();
    Serial.print(", time = ");
    Serial.println(asctime(localtime(&epoch)));
    // store the current values into the array
    pm25TS[isample] = pm25;
    pm10TS[isample] = pm10;
    timeTS[isample] = asctime(localtime(&epoch));
    // TODO: Send out the measurement values to a web service
    // sendMeasurement(isample);
    isample++;
    if (isample==nsamples) {isample=0;};
  } else {
    Serial.print(".");
  }
}

/*****************************************************************
 * getTime() gets a rough time from the number of seconds running
 */
time_t getTime() {
  time_t rawtime;
  struct tm * timeinfo;
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  // Serial.println(asctime(timeinfo));
  delay(1000);
  return rawtime;
}
// From "2:00:00" to 7200
int timetosec(String d) {
  int s1 = d.indexOf(":");
  int s2 = d.indexOf(":",s1);
  int s3 = d.length();
  int h = d.substring(0,s1).toInt();
  int m = d.substring(s1+1,s2).toInt();
  int s = d.substring(s2+1,s3).toInt();
  int t = h*3600+m*60+s;
  return t;
}


/*****************************************************************
 * Send out the stylesheet header
 */
String styleHeader() {
  String out = "<style>";
  out += " body {background-color: #ffffff; font-family: sans-serif; font-size: 16pt;}";
  out += " table {width: 80%; margin-left:auto; margin-right:auto; font-size: 16pt;}";
  out += " th, td {border-bottom: 1px solid #ddd;}";
  out += " tr:nth-child(odd) {background-color: #f2f2f2}";
  out += " .buttonOn    {background-color: #4CAF50; border-radius: 10%; font-size: 16pt;}";
  out += " .buttonMaybe {background-color: #008CBA; border-radius: 10%; font-size: 16pt;}";
  out += " .buttonOff   {background-color: #f44336; border-radius: 10%; font-size: 16pt;}";
  out += "</style>";
  out += "<meta http-equiv=\"refresh\" content=\"60; url=/\">";
  return out;
}

/*****************************************************************
 * Print the table header
 * p  parameter
 * v  value
 */
String tableHead(char* p, char* v) {
  String out = "<tr><th>";
  out += p;
  out += "</th><th>";
  out += v;
  out += "</th></tr>";
  return out;
}

/*****************************************************************
 * Print a single table row with a parameter, value pair
 * t  text
 * v  value
 */
String tableRow(String t, String v) {
  String out = "<tr><td>";
  out += t;
  out += "</td><td>";
  out += v;
  out += "</td></tr>";
  return out;
}

/**************************************************************
 * Returns a graph of the measurements in SVG
 * First get minimum and maximum of all time series
 * The measurement arrays are filled and refilled with the
 * latest value at isample-1, the oldest is thus at isample.
 * So we start plotting from isample to isample-1 mod nsamples.
 */
String graph(int ns) {
  int width=600;
  int height=400;
  float pm25Min= 50.0; float pm25Max=    0.0;
  float pm10Min= 50.0; float pm10Max=    0.0;
  for (int i=0;i<nsamples;i++) {
      pm25Min = min(pm25TS[i],pm25Min);
      pm25Max = max(pm25TS[i],pm25Max);
      pm10Min = min(pm10TS[i],pm10Min);
      pm10Max = max(pm10TS[i],pm10Max);
  }
  String out = "<svg height=\""+(String) height+"\" width=\""+(String) width+"\">\n";
  out += "<polyline style=\"fill:none;stroke:green;stroke-width:3\" points=\"";
  for (int i=0;i<nsamples;i++) {
    int is = (i+isample) % nsamples;
    float x=width*i/nsamples;
    float y=height*(1.0-(pm25TS[is]-pm25Min)/(pm25Max-pm25Min));
    out += (String) x+","+(String) y+" ";
  }
  out += "\"/>\n";
  out += "<text x=\"0.0\" y=\"20.0\" text-anchor=\"start\" fill=\"green\">" + (String) pm25Max + "</text>\n";
  out += "<text x=\"0.0\" y=\"" + (String) height + "\" text-anchor=\"start\" fill=\"green\">" + (String) pm25Min + "</text>\n";
  out += "<polyline style=\"fill:none;stroke:blue;stroke-width:3\" points=\"";
  for (int i=0;i<nsamples;i++) {
    int is = (i+isample) % nsamples;
    float x=width*i/nsamples;
    float y=height*(1.0-(pm10TS[is]-pm10Min)/(pm10Max-pm10Min));
    out += (String) x+","+(String) y+" ";
  }
  out += "\"/>\n";
  out += "<text x=\""+ (String) (width-100.0)+"\" y=\"20.0\" text-anchor=\"start\" fill=\"blue\">"+ (String) pm10Max +"</text>\n";
  out += "<text x=\""+ (String) (width-100.0)+"\" y=\""+ (String) height+"\" text-anchor=\"start\" fill=\"blue\">" + (String) pm10Min + "</text>\n";
  out += "</svg>";
  return out;
}
