#include <Arduino.h>

// GPS libs
#include <TinyGPS++.h>
// #include <SoftwareSerial.h>

// I2C libs
#include <Wire.h>

// OLED libs
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 4
Adafruit_SSD1306 display(LED_BUILTIN);

//Wifi libs
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

//GeoHash libs
#include <arduino-geohash.h>
GeoHash hasher(8);

//ADS 1115 Libs
#include <Adafruit_ADS1015.h>
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
// Adafruit_ADS1015 ads;     /* Use thi for the 12-bit version */

const char* host = "130.149.67.141";
String database = "MESSCONTAINER";

// The TinyGPS++ object
TinyGPSPlus gps;

// For stats that happen every 5 seconds
unsigned long last = 0UL;

void configModeCallback (WiFiManager *myWiFiManager) {

  // OLED Output
  display.clearDisplay();
  display.println("Connection failed!");
  display.println("Entered config mode");
  display.println(" ");
  display.println("Accesspoint:");
  display.println(myWiFiManager->getConfigPortalSSID());
  display.println("Config-IP:");
  display.println(WiFi.softAPIP());
  display.display();

  // Serial Output
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
  Serial.begin(9600);
  delay(10);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0,0);
  display.println("Starting Wifi-Manager");
  display.display();

  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");
  if(!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("failed to connect and hit timeout");
    display.clearDisplay();
    display.println("failed to connect and hit timeout");
    display.display();
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  display.clearDisplay();
  display.println("Connected!");
  display.println(" ");
  display.println("IP:");
  display.println(WiFi.localIP());
  display.println(" ");
  display.print("Searching satellites");
  display.display();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(2000);
  ads.setGain(GAIN_TWO);
  ads.begin();
}

void loop() {
  while (Serial.available()){
    if (gps.encode.Serial.read()){
      if (gps.location.isUpdated()){
        Serial.print(F("Lat="));
        Serial.println(gps.location.lat(),6);
        Serial.print(F("Lng="));
        Serial.println(gps.location.lng(),6);

        const char* geohash = hasher.encode(gps.location.lat(), gps.location.lng());
        Serial.println(geohash);

        String lat = String(gps.location.lat(),6);
        String lng = String(gps.location.lng(),6);

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,0);
        display.print("Lat: ");
        display.println(lat);
        display.setCursor(0,10);
        display.print("Lng: ");
        display.println(lng);
        display.setCursor(0,20);
        display.print("#: ");
        display.println(geohash);
        display.setCursor(0,30);

    // Use WiFiClient class to create TCP connections
        WiFiClient client;
        const int httpPort = 8086;
        Serial.print("connecting to ");
        Serial.println(host);

        if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0,0);
        display.print("Lat: ");
        display.println(lat);
        display.setCursor(0,10);
        display.print("Lng: ");
        display.println(lng);
        display.setCursor(0,20);
        display.print("UPLOAD FAILED!");
        display.display();
        return;
        }

        int16_t adc0, adc1, adc2, adc3;
        float adc0_mv, adc1_mv, adc2_mv, adc3_mv, adc_faktor;

        adc0 = ads.readADC_SingleEnded(0);
        adc1 = ads.readADC_SingleEnded(1);
        adc2 = ads.readADC_SingleEnded(2);
        adc3 = ads.readADC_SingleEnded(3);

        adc_faktor = 0.0625;

        adc0_mv = adc0 * adc_faktor;
        adc1_mv = adc1 * adc_faktor;
        adc2_mv = adc2 * adc_faktor;
        adc3_mv = adc3 * adc_faktor;

        Serial.print("AIN0: "); Serial.println(adc0);
        Serial.print("AIN1: "); Serial.println(adc1);
        Serial.print("AIN2: "); Serial.println(adc2);
        Serial.print("AIN3: "); Serial.println(adc3);
        Serial.println(" ");

        Serial.print("MV_0: "); Serial.println(adc0_mv);
        Serial.print("MV_1: "); Serial.println(adc1_mv);
        Serial.print("MV_2: "); Serial.println(adc2_mv);
        Serial.print("MV_3: "); Serial.println(adc3_mv);
        Serial.println(" ");


        String content = "esp,host=esp8266,geohash=" + String(geohash)+" lat=" + lat + ",lng=" + lng +",tmp2=1.4";
        Serial.println(content);

     client.print("POST /write?db=MESSCONTAINER HTTP/1.1\r\n");
     client.print("User-Agent: esp8266/0.1\r\n");
     client.print("Host: localhost:8086\r\n");
     client.print("Accept: */*\r\n");
     client.print("Content-Length: " + String(content.length()) + "\r\n");
     client.print("Content-Type: application/x-www-form-urlencoded\r\n");
     client.print("\r\n");
     client.print(content + "\r\n");


     client.flush();

    delay(100);
     // Read all the lines of the reply from server and print them to Serial
    while(client.available()){
       String line = client.readStringUntil('\r');
       Serial.print(line);
    }
    //delay(500);
    Serial.println("closing connection");
    client.stop();
    Serial.println();
    }
      else{
        //Serial.println("No Fix");
      }
    }

  }
