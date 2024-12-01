/* 

  When web server is setup, connect to the same network 
  on your client. Then, connect to:
    [localip]/webserial
  in your web browser.

  To-do:
   * Split core affinity across GPS parser and web server 
   * Prevent tcp/ip buffer overflow during signal drops
   * Label readings onto a map
   - Implement timezone-specific data determined from 
     lat/long data + new UTC_Offset custom enum
   - Get country and region based off lat/long data
  
*/

#define USE_SER_MTR 0
#define USE_WEB_INTF 1
#define USE_UTC_OFFSET 1
#define SCAN_INTERVAL 1000

#define RX D7
#define TX D6
#define GPS_BAUD 9600
#define ESP32_BAUD 115200

#include <TinyGPS++.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

const char *SSID = "SSID_HERE";
const char *PASSWORD = "PASS_HERE";
const uint8_t MAX_STA_CONN = 4;
const int8_t UTC_OFFSET = -8;

// Object instances
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);    // UART port 2
AsyncWebServer server(80);      // HTTP web server

// GPS Data; declare struct to avoid identifier conflicts 
  // with C-std library
typedef struct {
  double coords[2] = {0.0, 0.0};     // lat, lng
  float speed = 0.0;
  uint16_t date[3] = {0, 0, 0};     // year, month, day
  int8_t time[3] = {0, 0, 0};       // hour, minute, second
} GPSData;
GPSData gpsData = {};


void connectToNetwork();
void parseData();
void printToSerial();
void printToServer();


void setup(){

  // Flip the LED on during setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); 

  // initialize serial monitor & UART connection
  Serial.begin(ESP32_BAUD);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RX, TX);

  if (USE_WEB_INTF) {
    connectToNetwork();

    // Start web server
    WebSerial.begin(&server);
    server.begin();
  }

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("\nServer setup complete.");
}


void loop() {

  // keep reading from serial buffer as long as there's 
    // fresh data from the GPS in it
  while (gpsSerial.available())
    gps.encode(gpsSerial.read());

  if (gps.location.isUpdated()) {
    parseData();

    if (USE_WEB_INTF) printToServer();
    if (USE_SER_MTR) printToSerial();
  }

  delay(SCAN_INTERVAL);
}


void connectToNetwork() {

  Serial.printf("\nConnecting to %s", SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  // Print local IP once connected
  Serial.println("\nWiFi connected.");
  Serial.println("Local IP address:");
  Serial.println(WiFi.localIP());
}


void parseData() {
  gpsData.coords[0] = gps.location.lat();
  gpsData.coords[1] = gps.location.lng();
  gpsData.speed = gps.speed.kmph();
  gpsData.date[0] = gps.date.year();
  gpsData.date[1] = gps.date.month();
  gpsData.date[2] = gps.date.day();

  // hour logic
  if (USE_UTC_OFFSET) 
    // Apply UTC offset
    gpsData.time[0] = (gps.time.hour() + UTC_OFFSET) % 24;
    if (gpsData.time[0] < 0) 
        gpsData.time[0] += 24;     // Wrap negatives

    // Convert to 12-hour format
    if (gpsData.time[0] == 0) 
        gpsData.time[0] = 12;      // 0:00 -> 12:00
    else if (gpsData.time[0] > 12) 
        gpsData.time[0] -= 12;     // PM hours
  else {
    gpsData.time[0] = gps.time.hour();
  }

  gpsData.time[1] = gps.time.minute();
  gpsData.time[2] = gps.time.second();
}


void printToSerial() {

  Serial.printf("\nLAT: %.6f\nLONG: %.6f\nSPEED(km/h): %.2f\nTime in PST: %d/%d/%d, %d:%d:%d\n",
                   gpsData.coords[0], gpsData.coords[1],
                   gpsData.speed,
                   gpsData.date[0], gpsData.date[1], gpsData.date[2],
                   gpsData.time[0], gpsData.time[1], gpsData.time[2]);
}


void printToServer() {

  WebSerial.printf("\n\n\n\n\nLAT: %.6f\nLONG: %.6f\nSPEED(km/h): %.2f\nTime in PST: %d/%d/%d, %d:%d:%d\n\n",
                   gpsData.coords[0], gpsData.coords[1],
                   gpsData.speed,
                   gpsData.date[0], gpsData.date[1], gpsData.date[2],
                   gpsData.time[0], gpsData.time[1], gpsData.time[2]);
  WebSerial.flush();
}
