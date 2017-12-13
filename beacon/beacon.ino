#include <ESP8266WiFi.h>

extern "C" {
  #include "user_interface.h"
}

byte channel;

int maxssids=1; /* how much SSIDs we have */
char *ssids[] = {
      "Macaco", 
      };

byte rnd;
byte i;
byte count;


byte wifipkt[128] = { 0x80, 0x00, 0x00, 0x00, 
                /*4*/   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                /*10*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                /*16*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
                /*22*/  0xc0, 0x6c, 
                /*24*/  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, 
                /*32*/  0x64, 0x00, 
                /*34*/  0x01, 0x04, 
                /* SSID */
                /*36*/  0x00};

byte pktsuffix[] = {
                        0x01, 0x08, 0x82, 0x84,
                        0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, 
                        0x04 };                       

void setup() {
  Serial.begin(9600);
  delay(500);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(1);
  wifipkt[10] = wifipkt[16] = 0x12;
  wifipkt[11] = wifipkt[17] = 0x13;
  wifipkt[12] = wifipkt[18] = 0x14;
  wifipkt[13] = wifipkt[19] = 0x15;
  wifipkt[14] = wifipkt[20] = 0x09;
  wifipkt[15] = wifipkt[21] = 0x10;
 
}

void loop() {

    count=37;

    rnd=random(maxssids);
    
    wifipkt[count++]=strlen(ssids[rnd]);
    for (i=0; i<strlen(ssids[rnd]); i++) {
      wifipkt[count++]=ssids[rnd][i];
    }
    
    for (i=0; i<sizeof(pktsuffix); i++) {
       wifipkt[count++]=pktsuffix[i];
    }

    channel = 1; 
    wifi_set_channel(channel);
    wifipkt[count-1] = channel;
    wifi_send_pkt_freedom(wifipkt, count, 0);
   
    delay(1);
}
