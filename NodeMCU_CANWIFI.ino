// mcp_can - Version: Latest 
#include <mcp_can.h>
#include <mcp_can_dfs.h>

// ArduinoWebsockets - Version: Latest 
#include <ArduinoWebsockets.h>

  /* CAN Bridge Loopbak
 * This program bridges the CAN messages from MCP2515 module to 
 *  a websocket server and viceversa.
 *   
 *   Written By: Renato Gonzalez
 *   Project: TEC 
 */
#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <SPI.h>

byte RxData[8];

/* Websocket definitions */
const char* ssid = "DESKTOP-9DR6NJP 8238";                               // Enter SSID
const char* password = "5(C7008k";                                      // Enter Password
const char* serverAddress = "wss://vast-reef-61525.herokuapp.com/:80";

using namespace websockets;
WebsocketsClient client;

/* MCP2515 definitions */
#define WS_BUFFER_SIZE  13
#ifdef ARDUINO_ARCH_ESP32
#define TEST_PGN  0x18F00401
#else
#define TEST_PGN  0x18F00422
#endif
// CAN TX Variables
unsigned long txId = 0;
unsigned char txLen = 0;
unsigned char txBuf[8];
char msgString[128];
unsigned long prevTX = 0;                                        // Variable to store last execution time
const unsigned int invlTX = 1000;                                // One second interval constant
byte data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // Generic CAN data to send
// CAN RX Variables
unsigned long rxId;
unsigned char len;
unsigned char rxBuf[8];
unsigned char rxMsgBuffer[13];
// CAN0 INT and CS
#ifdef ARDUINO_ARCH_ESP32
#define CAN0_INT  21                            // Set INT to pin 2 on ESP32
MCP_CAN CAN0(5);                                // Set CS to pin 5 on ESP32
#else
#define CAN0_INT  4                             // Set INT to pin 2 on ESP8266
MCP_CAN CAN0(15);                               // Set CS to pin 15 on ESP8266
#endif

#include "thingProperties.h"
int pinLED = D4;

void setup() {
  Serial.begin(115200);
  pinMode(pinLED, OUTPUT);
  // This delay gives the chance to wait for a Serial Monitor without blocking if none is found
  delay(1500); 

  // Defined in thingProperties.h
  initProperties();

  // Connect to Arduino IoT Cloud
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("*");
  }

  Serial.println("");
  Serial.println("WiFi connection Successful");
  Serial.print("The IP Address of Module is: ");
  Serial.print(WiFi.localIP());// Print the IP address
  
    // Connect to the websocket server
  if (client.connect(serverAddress)) {
    Serial.println("Connected");
  } else {
    Serial.println("Connection failed.");
    while(1) {
      // Hang on failure
    }
  }

  // run callback when messages are received
  client.onMessage([&](WebsocketsMessage message) {
      Serial.print("Got Message from server. ");
      // Parse ws received string data
      const char *wsDataStr = message.c_str();
      // Split data to diff variables
      for (int dataIndex = 3; dataIndex >= 0; dataIndex--)
      {
        txId = (txId << 8) + wsDataStr[dataIndex];
      }
      txLen = wsDataStr[4];
      memcpy(&txBuf, &wsDataStr[5], sizeof(txBuf));

      // Format and print ws received message
      sprintf(msgString, "Id: 0x%.8lX  Len: %1d  Data:", txId, txLen);
      Serial.print(msgString);
      for(byte i = 0; i<txLen; i++){
        sprintf(msgString, " 0x%.2X", txBuf[i]);
        Serial.print(msgString);
      }
      Serial.println();
  });

  /* Initialize MCP CAN */
  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");
  
  // Since we do not set NORMAL mode, we are in loopback mode by default.
  CAN0.setMode(MCP_NORMAL);

  pinMode(CAN0_INT, INPUT);                           // Configuring pin for /INT input
  
  Serial.println("MCP2515 Loopback ...");
  
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  if(!digitalRead(CAN0_INT))                          // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len, rxBuf);              // Read data: len = data length, buf = data byte(s)
    sprintf(msgString, "Id: 0x%.8lX  Len: %1d  Data:", rxId, len);
    Serial.print(msgString);
    for(byte i = 0; i<len; i++){
       sprintf(msgString, " 0x%.2X", rxBuf[i]);
       Serial.print(msgString);
      }
    Serial.println();
    Serial.println(" Mensaje recibido correctamente");
    // Send received message to websocket server
    pressureValue = rxBuf[7];
    Serial.println(rxBuf[7]);
    if(client.available()) {
        // Copy data to buffer 
        rxId = rxId & 0x1FFFFFFF;
        memcpy(rxMsgBuffer, (unsigned char *)&(rxId), sizeof(unsigned long));
        rxMsgBuffer[4] = len;
        memcpy(&rxMsgBuffer[5], &rxBuf, sizeof(rxBuf));
        // Send buffer - fixed for CAN extended message type
        client.sendBinary((const char *)rxMsgBuffer, 13);
        Serial.println("WS Message Sent Successfully!");
    }
  }
  
  if(millis() - prevTX >= invlTX){                    // Send this at a one second interval. 
    prevTX = millis();
    byte sndStat = CAN0.sendMsgBuf(TEST_PGN,1, 8, data);
    
    if(sndStat == CAN_OK){
      Serial.println("CAN Message Sent Successfully!");
    }
    else
      Serial.println("Error Sending CAN Message...");

  }
  
  static int number = 0;
  
  // let the websockets client check for incoming messages
  if(client.available()) {
      client.poll();
  }
  
  ArduinoCloud.update();
}
/*
  Since SetPressureValue is READ_WRITE variable, onSetPressureValueChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onSetPressureValueChange()  {
  // Add your code here to act upon SetPressureValue change
  data[7] = setPressureValue;
}
/*
  Since EspLED is READ_WRITE variable, onEspLEDChange() is
  executed every time a new value is received from IoT Cloud.
*/
void onEspLEDChange()  {
  // Add your code here to act upon EspLED change
   if(espLED){
    digitalWrite(pinLED, HIGH);
  }else{
    digitalWrite(pinLED, LOW);
  }
}
