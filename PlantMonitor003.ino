#include <Arduino.h>
#include <HTTPSRedirect.h>

#include <Wire.h>
#include <Adafruit_AHT10.h>
Adafruit_AHT10 aht;
// address of AHT10: 0x38

/* data logging to Google Sheets based off of:
 *  https://github.com/StorageB/Google-Sheets-Logging
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


const String deviceID = "PlantMonitor0001";

// spreadsheetID
const char* spreadsheetID = "GoogleSheetID";
// deploymentID
const char* GScriptId = "GoogleScriptID";

// Enter command (insert_row or append_row) and your Google Sheets sheet name (default is Sheet1):
String payload_base =  "{\"command\": \"insert_row\", \"sheet_name\": \"Sheet1\", \"values\": ";
String payload = "";

// Google Sheets setup (do not edit)
const char* host = "script.google.com";
const int httpsPort = 443;
const char* fingerprint = "";
String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client = nullptr;

// Declare variables that will be published to Google Sheets
int timestamp = 0;
String device = "";
String reading = "";
float value = 0;

// Define the pins
const int probe = A0;
const int redLED = D3;
const int blueLED = D0;
const int button = D5;

bool buttonON = LOW;

// refresh rate in ms
const int refresh = 10*60*1000;
int lastMillis = 0;

//SSID and Password of your WiFi router
const char* ssid = "WiFiSSID";
const char* password = "WiFiPassword";


const long utcOffsetInSeconds = 19*3600;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);



/***************************************************************
* SETUP
**************************************************************/
void setup(void){

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(probe,INPUT);
  pinMode(button,INPUT);
  pinMode(redLED,OUTPUT);
  pinMode(blueLED,OUTPUT);

  digitalWrite(redLED,HIGH);
  digitalWrite(blueLED,HIGH);

  
  Serial.begin(115200);
  Serial.println("");
  Serial.println("RESET ---------------------------------");
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  

  timeClient.begin();

  Serial.println("Scanning for AHT10");

  if (! aht.begin()) {
    Serial.println("Could not find AHT10? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 found");

  digitalWrite(LED_BUILTIN,LOW);

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  Serial.print("Connecting to ");
  Serial.println(host);
  
  // Try to connect for a maximum of 5 times
  bool flag = false;
  for (int i=0; i<5; i++){ 
    int retval = client->connect(host, httpsPort);
    if (retval == 1){
       flag = true;
       Serial.println("Connected");
       break;
    }
    else
      Serial.println("Connection failed. Retrying...");
  }
  if (!flag){
    Serial.print("Could not connect to server: ");
    Serial.println(host);
    return;
  }
  delete client;    // delete HTTPSRedirect object
  client = nullptr; // delete HTTPSRedirect object


  timeClient.update();
  String payload = String(timeClient.getEpochTime()) +", "+deviceID+",device reset, -1";
  sendPayload(payload);
  

  digitalWrite(redLED,LOW);
  digitalWrite(blueLED,LOW);

}
/***************************************************************
* LOOP
**************************************************************/
void loop(void){

  if (millis()>(lastMillis+refresh)) {
     measure();

    lastMillis = millis();
  }

  if (buttonON==LOW & digitalRead(button)==HIGH) {

    buttonON =HIGH;
    onButton();
  }
  if (buttonON==HIGH & digitalRead(button)==LOW) {

    buttonON = LOW;
  }
  digitalWrite(blueLED,LOW);

}

void sendPayload(String pay) {

  Serial.println("attempting to send:  " + pay);
  
  static bool flag = false;
  if (!flag){
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client != nullptr){
    if (!client->connected()){
      client->connect(host, httpsPort);
    }
  }
  else{
    Serial.println("Error creating client object!");
  }

  // Create json object string to send to Google Sheets
  String payload = payload_base + "\"" +pay + "\"}";

  // Publish data to Google Sheets
  Serial.println("Publishing data...");
  Serial.println(payload);
  if(client->POST(url, host, payload)){ 
    // do stuff here if publish was successful
    
  }
  else{
    // do stuff here if publish was not successful
    Serial.println("Error while connecting");
  }

  
}

void measure() {
  // read the probes

  digitalWrite(blueLED,HIGH);

  // moisture reading
  int probeReading = analogRead(probe);
  timeClient.update();
  String payload = String(timeClient.getEpochTime()) +", "+deviceID+", moisture, " + String(probeReading);
  sendPayload(payload);

  // temp and humidity reading
  sensors_event_t humidity, temp;
  timeClient.update();
  aht.getEvent(&humidity, &temp);
    payload = String(timeClient.getEpochTime()) +", "+deviceID+", temperature, " + String(temp.temperature);
    sendPayload(payload);
    payload = String(timeClient.getEpochTime()) +", "+deviceID+", humidity, " + String(humidity.relative_humidity);
    sendPayload(payload);

}

void onButton() {

  digitalWrite(blueLED,HIGH);

  timeClient.update();
  
  String payload = String(timeClient.getEpochTime()) +", "+deviceID+", button, -1";
  sendPayload(payload);
  measure();
  
}
