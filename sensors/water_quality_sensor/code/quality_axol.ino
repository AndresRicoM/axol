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

   Code for water quality sensor. The sensor uses a TDS meter to measure water conductivity.   

   Andres Rico - aricom@mit.edu
  
 */


#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <SensirionI2CSht4x.h>
#include "GravityTDS.h"

#define DEV_I2C Wire
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */
#define TdsSensorPin 2
GravityTDS gravityTds;
//Definitions for thermistor temperature option. 
SensirionI2CSht4x sht4x;

float temperature;
float humidity;

float tdsValue = 0;

uint8_t broadcastAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //MAC Address for receiving homehub device. 
constexpr char WIFI_SSID[] = ""; //Network name, no password needed. 

int32_t getWiFiChannel(const char *ssid) {

    if (int32_t n = WiFi.scanNetworks()) {
        for (uint8_t i=0; i<n; i++) {
            if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
                return WiFi.channel(i);
            }
        }
    }

    return 0;
}

typedef struct struct_message {
  char id[50];
  int type;
  int tds;
  int temp;
} struct_message;

struct_message myData;

String address = WiFi.macAddress();
char mac_add[50];

int attempts = 0; 

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

}

int sensPower = 17;

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
  }

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  pinMode(sensPower, OUTPUT);
  pinMode(TdsSensorPin, INPUT);
  
  digitalWrite(sensPower, HIGH);
  delay(1000);
  Wire.begin();

  uint16_t error;
  char errorMessage[256];

  sht4x.begin(Wire);

  getHumTemp();

  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(3.3);  //reference voltage on ADC, default 5.0V on Arduino UNO
  gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
  gravityTds.begin();  //initialization
  gravityTds.setTemperature(temperature);
  gravityTds.update();  //sample and calculate
  tdsValue = gravityTds.getTdsValue();  // then get the value 
  
  Serial.print("TDS value: ");
  Serial.print(tdsValue); //TDS Value
  Serial.println(" ppm");

  //Serial.println(read_efuse_vref(void));
  address.toCharArray(mac_add, 50);
  WiFi.mode(WIFI_STA);
  int32_t wifi_channel = getWiFiChannel(WIFI_SSID);
  strcpy(myData.id, mac_add);
  myData.type = 4; //Id 4 = Water Quality. 
  myData.temp = temperature;
  myData.tds = tdsValue;

  Serial.println(myData.id);

  //WiFi.printDiag(Serial); // Uncomment to verify channel number before
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  //WiFi.printDiag(Serial); // Uncomment to verify channel change after

  // Init ESP-NOW
  esp_now_init();

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.encrypt = false;
  
  // Add peer
  esp_now_add_peer(&peerInfo);        

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
 
  esp_sleep_enable_timer_wakeup(43200000000);//TIME_TO_SLEEP * uS_TO_S_FACTOR); //Twice per day. 86400000000); 43200000000

  esp_wifi_stop();

  esp_deep_sleep_start();

}

void loop() {
  // put your main code here, to run repeatedly:
}
