/*
  
   █████╗ ██╗  ██╗ ██████╗ ██╗     
  ██╔══██╗╚██╗██╔╝██╔═══██╗██║     
  ███████║ ╚███╔╝ ██║   ██║██║     
  ██╔══██║ ██╔██╗ ██║   ██║██║     
  ██║  ██║██╔╝ ██╗╚██████╔╝███████╗
  ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝

  Axol sensing system. 

  ╔═╗┬┌┬┐┬ ┬  ╔═╗┌─┐┬┌─┐┌┐┌┌─┐┌─┐       ╔╦╗╦╔╦╗  ╔╦╗┌─┐┌┬┐┬┌─┐  ╦  ┌─┐┌┐
  ║  │ │ └┬┘  ╚═╗│  │├┤ ││││  ├┤   ───  ║║║║ ║   ║║║├┤  │││├─┤  ║  ├─┤├┴┐
  ╚═╝┴ ┴  ┴   ╚═╝└─┘┴└─┘┘└┘└─┘└─┘       ╩ ╩╩ ╩   ╩ ╩└─┘─┴┘┴┴ ┴  ╩═╝┴ ┴└─┘

                                 .|
                                | |
                                |'|            ._____
                        ___    |  |            |.   |' .---"|
                _    .-'   '-. |  |     .--'|  ||   | _|    |
             .-'|  _.|  |    ||   '-__  |   |  |    ||      |
             |' | |.    |    ||       | |   |  |    ||      |
          ___|  '-'     '    ""       '-'   '-.'    '`      |____

   Code for temperature and humidity sensor for the Axol sensing system. 
   The code runs on an ESP32 chip along with a STHx sensor module. 

   Andres Rico - aricom@mit.edu
  
 */

#include <Arduino.h>
#include <SensirionI2CSht4x.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <vl53l4cx_class.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#define DEV_I2C Wire
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */

uint8_t broadcastAddress[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX}; //Replace XX with Paired Homehub Address MAC Address digits. 
constexpr char WIFI_SSID[] = ""; //Input WIFI network name.

int32_t getWiFiChannel(const char *ssid) { //Function for 

  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i = 0; i < n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
      }
    }
  }

  return 0;
}

typedef struct struct_message { //Structure of sensor data packet that is sent to the HomeHub. 
  char id[50];    //Sensor MAC Address
  int type;       //Type of sensor. -> Temperature/Humidity = 3
  int temp;       //Current temperature
  int humidity;   //Current humidity
} struct_message;

struct_message myData;

String address = WiFi.macAddress(); //Fetch sensor MAC Address. 
char mac_add[50];

int attempts = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

}


SensirionI2CSht4x sht4x;

int sensPin = 4; /Pin for powering sensor from ESP32 GPIO. 
float temperature;
float humidity;

void getHumTemp() {
  
  uint16_t error;
  char errorMessage[256];

  delay(1000);

  error = sht4x.measureHighPrecision(temperature, humidity);
  
  if (error) {
    Serial.print("Error trying to execute measureHighPrecision(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("Temperature:");
    Serial.print(temperature);
    Serial.print("\t");
    Serial.print("Humidity:");
    Serial.println(humidity);
  }

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(sensPin, OUTPUT);
  //pinMode(lightPin, INPUT);
  digitalWrite(sensPin, HIGH);

  Wire.begin();

  uint16_t error;
  char errorMessage[256];

  sht4x.begin(Wire);

  getHumTemp();
  address.toCharArray(mac_add, 50);
  WiFi.mode(WIFI_STA);
  int32_t wifi_channel = getWiFiChannel(WIFI_SSID);
  strcpy(myData.id, mac_add);
  myData.type = 3; //Id 2 = Tank Level sensor.
  myData.temp = temperature;
  myData.humidity = humidity;

  Serial.println(myData.id);

  //WiFi.printDiag(Serial); // Uncomment to verify channel number before
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  //WiFi.printDiag(Serial); // Uncomment to verify channel change after

  //Serial.println(WiFi.macAddress());
  // Init ESP-NOW
  esp_now_init(); //Start ESP now communication. 
  
  esp_now_register_send_cb(OnDataSent); // get the status of Trasnmitted packet

  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.encrypt = false;

  // Add peer
  esp_now_add_peer(&peerInfo); //Loof for esp now peer. 

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  esp_sleep_enable_timer_wakeup(43200000000) ; //Time in uS -> Reports Twice a day. 

  esp_wifi_stop(); //Shut down wifi before going to sleep. 

  esp_deep_sleep_start(); //Go into Deep Sleep

}

void loop() {
  // put your main code here, to run repeatedly:
}
