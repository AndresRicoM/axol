/*
  
   █████╗ ██╗  ██╗ ██████╗ ██╗     
  ██╔══██╗╚██╗██╔╝██╔═══██╗██║     
  ███████║ ╚███╔╝ ██║   ██║██║     
  ██╔══██║ ██╔██╗ ██║   ██║██║     
  ██║  ██║██╔╝ ██╗╚██████╔╝███████╗
  ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝

  ᓬ(• - •)ᕒ 

  Axol sensing system. 

   Code for tank quantity sensor. The seansor uses a vl53l4 optical sensor to detemine its distance from the water's surface. 
   The data is then used to calculate the quanity and fill percentage in the database. Volume is calaculated in the database backend.
   You need to measure containers dimensions and capacity to register this device for the backend to calculate volume correctly. 

   Andres Rico - aricom@mit.edu
  
 */

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

////CHANGE THESE VARIABLES FOR SETUP WITH HOMEHUB AND NETWORK////////

//Receiver address
uint8_t broadcastAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //MAC Address for receiving homehub.  

constexpr char WIFI_SSID[] = ""; //Network name, no password required.

/////////////////////////////////////////////////////////////////////

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
  int height;
} struct_message;

struct_message myData;

String address = WiFi.macAddress();
char mac_add[50];

int attempts = 0; 

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

}

// Components.
VL53L4CX sensor_vl53l4cx_sat(&DEV_I2C, 16);

void setup() {
  // put your setup code here, to run once:
  pinMode(12, OUTPUT);
  digitalWrite(12, HIGH);
  
  Serial.begin(115200);
  
  // Initialize I2C bus.
  DEV_I2C.begin();

  // Configure VL53L4CX satellite component.
  sensor_vl53l4cx_sat.begin();

  // Switch off VL53L4CX satellite component.
  sensor_vl53l4cx_sat.VL53L4CX_Off();

  //Initialize VL53L4CX satellite component.
  sensor_vl53l4cx_sat.InitSensor(0x12);

  // Start Measurements
  sensor_vl53l4cx_sat.VL53L4CX_StartMeasurement();

  int i = 0;
  int final_reading;
  float sum = 0;
  
  while (i < 50) {

    VL53L4CX_MultiRangingData_t MultiRangingData;
    VL53L4CX_MultiRangingData_t *pMultiRangingData = &MultiRangingData;
    uint8_t NewDataReady = 0;
    int no_of_object_found = 0, j;
    char report[64];
    int status;
  
    do { //Loop until we get new data. 
      status = sensor_vl53l4cx_sat.VL53L4CX_GetMeasurementDataReady(&NewDataReady);
    } while (!NewDataReady);
    
    if ((!status) && (NewDataReady != 0)) {
      status = sensor_vl53l4cx_sat.VL53L4CX_GetMultiRangingData(pMultiRangingData);
      no_of_object_found = pMultiRangingData->NumberOfObjectsFound;
      if (no_of_object_found == 1) {
        i = i + 1;
        sum = sum + pMultiRangingData->RangeData[0].RangeMilliMeter;
        //Serial.println(pMultiRangingData->RangeData[0].RangeMilliMeter);
      }
      
      if (status == 0) {
        status = sensor_vl53l4cx_sat.VL53L4CX_ClearInterruptAndStartMeasurement();
      }
    }
      
   }
  
   //digitalWrite(18, LOW);
   myData.height = sum / 50 ; //Average 50 from 50 readings. 
   Serial.println(myData.height);

   address.toCharArray(mac_add, 50);
    Serial.println(mac_add);
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    int32_t wifi_channel = getWiFiChannel(WIFI_SSID);
    strcpy(myData.id, mac_add);
    myData.type = 2; //Id 2 = Tank Level sensor. 
  
    //Serial.println(myData.id);
  
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
   
   /////////////////Change value for higher or lower frequency of data collection. This is the time the ESP32 will sleep for.
   esp_sleep_enable_timer_wakeup(43200000000) ; //TIME_TO_SLEEP * uS_TO_S_FACTOR); //Twice per day. Value is in microseconds.

   esp_wifi_stop();

   esp_deep_sleep_start();  
   
}

void loop() {
  
}
