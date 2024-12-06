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

const char *SSID = "aiphone25";
const char *PASSWORD = "esp32s3neo6m";
const uint8_t MAX_STA_CONN = 4;
const int8_t UTC_OFFSET = -8;

// Object instances
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);    // UART port 2
AsyncWebServer server(80);      // HTTP web server

// GPS Data
typedef struct {
  double coords[2] = {0.0, 0.0};     // lat, lng
  float speed = 0.0;
  uint16_t date[3] = {0, 0, 0};      // year, month, day
  int8_t time[3] = {0, 0, 0};        // hour, minute, second
} GPSData;
GPSData gpsData = {};

void connectToNetwork();
void parseData();
void printToSerial();
void serveMapPage();
void updateGPSData();

void setup(){

  // Flip the LED on during setup
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); 

  // Initialize serial monitor & UART connection
  Serial.begin(ESP32_BAUD);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RX, TX);

  if (USE_WEB_INTF) {
    
    connectToNetwork();
    
    // Setup web routes
    // (request) is an object declared in ESPAsyncWebServer.h
    // server.on: registers new web route for the server.
      // Accepts parameters: ([url path], [web request], [callback function])
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
      serveMapPage(request);  // Serve the map page
    });

    // alternate web route for json-formatted data
    server.on("/gps", HTTP_GET, [](AsyncWebServerRequest *request){
      updateGPSData(request);  // Provide GPS data in JSON format
    });

    // Start server (begin listening for incoming requests)
    server.begin();
  }

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("\nServer setup complete.");
}

void loop() {

  // Load data from GPS serial buffer
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isUpdated()) {
    parseData();
    if (USE_WEB_INTF) {
      updateGPSData();  // Send the GPS data to the web page
    }
    if (USE_SER_MTR) {
      printToSerial();
    }
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

  if (USE_UTC_OFFSET) {
    gpsData.time[0] = (gps.time.hour() + UTC_OFFSET) % 24;
    if (gpsData.time[0] < 0) gpsData.time[0] += 24;
    if (gpsData.time[0] == 0) gpsData.time[0] = 12;
    else if (gpsData.time[0] > 12) gpsData.time[0] -= 12;
  } 
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

// JS code embedded in the HTML, updates map every second
void serveMapPage(AsyncWebServerRequest *request) {
  String html = "<!DOCTYPE html><html><head><title>GPS Location</title><link rel='stylesheet' href='https://unpkg.com/leaflet@1.7.1/dist/leaflet.css'/><script src='https://unpkg.com/leaflet@1.7.1/dist/leaflet.js'></script></head><body>";
  html += "<h2>GPS Location</h2>";
  html += "<div id='map' style='height: 400px;'></div>";
  html += "<script>";
  html += "var map = L.map('map').setView([0, 0], 2);";
  html += "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);";
  html += "var marker = L.marker([0, 0]).addTo(map);";
  html += "function updateGPS(lat, lon) { marker.setLatLng([lat, lon]); map.panTo([lat, lon]); }";
  html += "setInterval(function() {";
  html += "  fetch('/gps').then(response => response.json()).then(data => updateGPS(data.lat, data.lon));";
  html += "}, 1000);";
  html += "</script></body></html>";
  request->send(200, "text/html", html);
}

void updateGPSData() {
  // No-argument version for `loop()`
  // Serial.printf("Lat: %.6f, Lon: %.6f\n", gpsData.coords[0], gpsData.coords[1]);
}

void updateGPSData(AsyncWebServerRequest *request) {
  // Argument version for HTTP request handling
  String json = "{";
  json += "\"lat\": " + String(gpsData.coords[0], 6) + ",";
  json += "\"lon\": " + String(gpsData.coords[1], 6);
  json += "}";
  request->send(200, "application/json", json);
}
