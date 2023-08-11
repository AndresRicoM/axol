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
   The code is based on the example code provided by DFrobot for the TDS meter.

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

 #define DEV_I2C Wire
 #define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
 #define TIME_TO_SLEEP  1        /* Time ESP32 will go to sleep (in seconds) */

 #define TdsSensorPin 4 //Output pin for giving power to the sensor. 
 #define VREF 3.3      // analog reference voltage(Volt) of the ADC
 #define SCOUNT  30           // sum of sample point

 SensirionI2CSht4x sht4x;

 int analogBuffer[SCOUNT];    // store the analog value in the array, read from ADC
 int analogBufferTemp[SCOUNT];
 int analogBufferIndex = 0,copyIndex = 0;
 float averageVoltage = 0,tdsValue = 0;
 int sensPower = 17;
 float temperature;
 float humidity;

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

   getHumTemp(); //Get temperature and humidity for temperature compensation.

   float analogSum = 0;
     for (int i = 0 ; i < 50 ; i++) {
       analogSum = analogSum + analogRead(TdsSensorPin);

     }

   float analogVal = analogSum / 50;

   averageVoltage = analogVal * (float)VREF / 4096.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
   float compensationCoefficient=1.0+0.02*(temperature-25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
   float compensationVolatge=averageVoltage/compensationCoefficient;  //temperature compensation
   tdsValue=(133.42*compensationVolatge*compensationVolatge*compensationVolatge - 255.86*compensationVolatge*compensationVolatge + 857.39*compensationVolatge)*0.5; //convert voltage value to tds value

   Serial.print("TDS Value:");
   Serial.print(tdsValue,0);
   Serial.println("ppm");


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

   /////////////////Change value for higher or lower frequency of data collection. This is the time the ESP32 will sleep for.
   esp_sleep_enable_timer_wakeup(43200000000); //TIME_TO_SLEEP * uS_TO_S_FACTOR); //Twice per day. Value is in microseconds.

   esp_wifi_stop();

   esp_deep_sleep_start();

 }

 void loop() {
   // put your main code here, to run repeatedly:
 }
