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

   Code for bucket sensor. The sensor uses a tilt sensor to detect when a bucket is used.   

   Andres Rico - aricom@mit.edu
  
 */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

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
  
} struct_message;

struct_message myData;

String address = WiFi.macAddress();
char mac_add[50];

int attempts = 0; 

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  pinMode(15, INPUT_PULLUP);

  send_espnow();

  Serial.println("Going to bed...");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0); //Set wake up pin to GPIO_NUM_15
  esp_wifi_stop();
  esp_deep_sleep_start();  
  
}

void send_espnow() {
  address.toCharArray(mac_add, 50);
  Serial.println(mac_add);
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  int32_t wifi_channel = getWiFiChannel(WIFI_SSID);
  strcpy(myData.id, mac_add);
  myData.type = 1;

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
  delay(4000); // delay to allow ESP-NOW to work.
  
}

void loop() {

}
