
extern "C" {
  #include <user_interface.h>
}

#define DATA_LENGTH           112
#define TYPE_MANAGEMENT       0x00
#define TYPE_CONTROL          0x01
#define TYPE_DATA             0x02
#define SUBTYPE_PROBE_REQUEST 0x04

#define QTD_MACS 30

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/////////////////BEACON///////////////////////

byte channel;
byte rnd;
byte i;
byte count;
int maxssids = 1;  /*Quantidade de SSIDS*/
char *ssids[] = {  "Macaco" };

byte wifipkt[128] = {   0x80, 0x00, 0x00, 0x00, 
                /*4*/   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                /*10*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                /*16*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 
                /*22*/  0xc0, 0x6c, 
                /*24*/  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, 
                /*32*/  0x64, 0x00, 
                /*34*/  0x01, 0x04, 
                /* SSID */
                /*36*/  0x00};

byte pktsuffix[] = {    0x01, 0x08, 0x82, 0x84,
                        0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, 0x03, 0x01, 
                        0x04 };   
/////////////////BEACON///////////////////////

WiFiClient espClient;

const char* ssid = "Uai-fai"; //Nome da rede
const char* password = "123457890"; //Senha da Rede

PubSubClient client(espClient);

const char* mqtt_server = "m13.cloudmqtt.com"; //server MQTT
const int mqtt_port = 13988; //Porta MQTT

int contador;

struct RxControl {
 signed rssi:8; // signal intensity of packet
 unsigned rate:4;
 unsigned is_group:1;
 unsigned:1;
 unsigned sig_mode:2; // 0:is 11n packet; 1:is not 11n packet;
 unsigned legacy_length:12; // if not 11n packet, shows length of packet.
 unsigned damatch0:1;
 unsigned damatch1:1;
 unsigned bssidmatch0:1;
 unsigned bssidmatch1:1;
 unsigned MCS:7; // if is 11n packet, shows the modulation and code used (range from 0 to 76)
 unsigned CWB:1; // if is 11n packet, shows if is HT40 packet or not
 unsigned HT_length:16;// if is 11n packet, shows length of packet.
 unsigned Smoothing:1;
 unsigned Not_Sounding:1;
 unsigned:1;
 unsigned Aggregation:1;
 unsigned STBC:2;
 unsigned FEC_CODING:1; // if is 11n packet, shows if is LDPC packet or not.
 unsigned SGI:1;
 unsigned rxend_state:8;
 unsigned ampdu_cnt:8;
 unsigned channel:4; //which channel this packet in.
 unsigned:12;
};

typedef struct SnifferPacket{
    struct RxControl rx_ctrl;
    uint8_t data[DATA_LENGTH];
    uint16_t cnt;
    uint16_t len;
} SnifferPacketT;

SnifferPacketT* snifferP = NULL; ///GLOBAL     ///MODIFIED
char addr[] = "00:00:00:00:00:00";

String rssids[QTD_MACS];                              ////rssids

int contadorMac = 0;
char macs[QTD_MACS][18];                              /////MACS


static void showMetadata(SnifferPacketT *snifferPacket) {

  unsigned int frameControl = ((unsigned int)snifferPacket->data[1] << 8) + snifferPacket->data[0];

  uint8_t version      = (frameControl & 0b0000000000000011) >> 0;
  uint8_t frameType    = (frameControl & 0b0000000000001100) >> 2;
  uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;
  uint8_t toDS         = (frameControl & 0b0000000100000000) >> 8;
  uint8_t fromDS       = (frameControl & 0b0000001000000000) >> 9;

  // Only look for probe request packets
  if (frameType != TYPE_MANAGEMENT ||
      frameSubType != SUBTYPE_PROBE_REQUEST)
        return;

  Serial.print("RSSI: ");
  Serial.print(snifferPacket->rx_ctrl.rssi, DEC);

  rssids[contadorMac] = String(snifferPacket->rx_ctrl.rssi, DEC);

  Serial.print(" Ch: ");
  Serial.print(wifi_get_channel());


  getMAC(addr, snifferPacket->data, 10);
  Serial.print(" Peer MAC: ");
  Serial.print(addr);
  for(int i = 0; i < 18; i++){
      macs[contadorMac][i] = addr[i];
  }
  contadorMac++;
  if(contadorMac == QTD_MACS){
      contadorMac = 0;
      Serial.print("Contador de Macs ZERADO!!!");
  }

  uint8_t SSID_length = snifferPacket->data[25];
  Serial.print(" SSID: ");
  printDataSpan(26, SSID_length, snifferPacket->data);

  Serial.println();
}

/**
 * Callback for promiscuous mode
 */
static void ICACHE_FLASH_ATTR sniffer_callback(uint8_t *buffer, uint16_t length) {
  snifferP = (SnifferPacketT*) buffer;
  showMetadata(snifferP);
}

static void printDataSpan(uint16_t start, uint16_t size, uint8_t* data) {
  for(uint16_t i = start; i < DATA_LENGTH && i < start+size; i++) {
    Serial.write(data[i]);
  }
}

static void getMAC(char *addr, uint8_t* data, uint16_t offset) {
  sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", data[offset+0], data[offset+1], data[offset+2], data[offset+3], data[offset+4], data[offset+5]);
}

#define CHANNEL_HOP_INTERVAL_MS   1000
static os_timer_t channelHop_timer;

/**
 * Callback for channel hoping
 */


#define DISABLE 0
#define ENABLE  1

 void timer2(){
  if(contador > 2){
    Serial.println("Disarmando...");
    os_timer_disarm(&channelHop_timer);
    Serial.println("Modo Promiscuo...");
    wifi_promiscuous_enable(DISABLE);
    Serial.println("Disarmado e Sem promiscuo.");
    setup_wifi();
    
    if (!client.connected()) {
        conectMqtt();
        for(int i = 0; i < QTD_MACS; i++){
            if(macs[i][0] == '*'){
              break;
            }
            
            sendMessage("macaco_00", macs[i], rssids[i]);
        }
        contadorMac = 0;
        for(int i = 0; i < QTD_MACS; i++){
            macs[i][0] = '*';
        }
    }
        
    wifi_promiscuous_enable(ENABLE);
    delay(10);
    os_timer_arm(&channelHop_timer, CHANNEL_HOP_INTERVAL_MS, 1);
    contador = 0;  
  } 
}

void channelHop()
{
  contador++; 
  // hoping channels 1-14
  uint8 new_channel = wifi_get_channel() + 1;
  if (new_channel > 14)
    new_channel = 1;
  wifi_set_channel(new_channel);
}


/////////////////PubFunctions///////////////////////
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereco IP : ");
  Serial.println(WiFi.localIP());
}


  
void conectMqtt() {
  while (!client.connected()) {    
    Serial.print("ConectandoMQTT ...");    
    
    if (client.connect("teste","romero","123")) {
      Serial.println("Conectado");
      
    } else {
      Serial.print("Falha ");      
      Serial.print(client.state());      
      Serial.println(" Tentando novamente em 5 segundos");
      Serial.println(WiFi.localIP());      
      
      delay(5000);
    }
  }
}


void sendMessage(String idRecinto, String mac, String potencia){

  String mensagem = "{'idRecinto': " + idRecinto + ", 'mac': " + mac +", 'potencia': " + potencia + "}";

  // Transformando a String em char para poder publicar no mqtt
  char charpub[mensagem.length() + 1];
  mensagem.toCharArray(charpub, mensagem.length()+1);
  Serial.print(mensagem);
  client.publish("acessoNode", charpub);  
  
  Serial.println();
}

/////////////////////////////////////////////

void setup() {

  for(int i = 0; i < QTD_MACS; i++)
    macs[i][0] = '*';

  
  // set the WiFi chip to "promiscuous" mode aka monitor mode
  Serial.begin(115200);
  delay(30);
  /////////////////BEACON///////////////////////
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(1);
  wifipkt[10] = wifipkt[16] = 0x12;
  wifipkt[11] = wifipkt[17] = 0x13;
  wifipkt[12] = wifipkt[18] = 0x14;
  wifipkt[13] = wifipkt[19] = 0x15;
  wifipkt[14] = wifipkt[20] = 0x09;
  wifipkt[15] = wifipkt[21] = 0x10;
  /////////////////BEACON///////////////////////

  wifi_promiscuous_enable(DISABLE);
  delay(10);
  wifi_set_promiscuous_rx_cb(sniffer_callback);
  delay(10);
  wifi_promiscuous_enable(ENABLE);

  client.setServer(mqtt_server, mqtt_port);

  // setup the channel hoping callback timer
  os_timer_disarm(&channelHop_timer);
  os_timer_setfn(&channelHop_timer, (os_timer_func_t *) channelHop, NULL);
  os_timer_arm(&channelHop_timer, CHANNEL_HOP_INTERVAL_MS, 1);
}

void loop() {
  /////////////////BEACON///////////////////////
    count=37;

    rnd=random(maxssids);
    
    wifipkt[count++]=strlen(ssids[rnd]);
    for (i=0; i<strlen(ssids[rnd]); i++) {
      wifipkt[count++]=ssids[rnd][i];
    }
    
    for (i=0; i<sizeof(pktsuffix); i++) {
       wifipkt[count++]=pktsuffix[i];
    }

    wifi_set_channel(1);
    wifipkt[count-1] = 1;
    wifi_send_pkt_freedom(wifipkt, count, 0);
  /////////////////BEACON///////////////////////
    
    delay(10);
    timer2();
}
