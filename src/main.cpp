#define TINY_GSM_MODEM_BG96 // EC25
#define MODEM_TX 27
#define MODEM_RX 14
#define UART_BAUD 115200
#define TINY_GSM_RX_BUFFER 1030
#define TINY_GSM_TEST_GPRS true
#define TINY_GSM_TEST_TCP true
#define pingCount 5 // Error time count 5 to reset
#define ID        1 

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <WiFi.h>
#include <HardwareSerial.h>
#include <ModbusMaster.h>
#include <TinyGSM.h>
#include <HTTPClientESP32.h>
#include <SSLClient.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <HttpClient.h>

#include "strcData.h"
// #include "gsmOTA.h"

BluetoothSerial BT;
HardwareSerial modbus(2);
HardwareSerial SerialAT(1);
ModbusMaster node;
// GSM Object
TinyGsm modem(SerialAT);
TinyGsmClient gsm_mqtt_client(modem, 0);
PubSubClient mqttGSM(gsm_mqtt_client);
// HTTPS Transport OTA
const char serverOTA[] = "raw.githubusercontent.com";
const int port = 443;
TinyGsmClient base_client(modem, 1);
SSLClient secure_layer(&base_client);
HttpClient GSMclient = HttpClient(secure_layer, serverOTA, port);


String hostBT = "CRA-WaterTreat-";
String current_version = "0.0.2";
String deviceToken, json, token;
boolean GSMnetwork, GSMgprs = false;
// Your GPRS credentials
const char apn[] = "internet";
const char user[] = "", pass[] = "";
unsigned long previousMillis, periodOTA;
String serverName ="https://tb.thingcontrol.io/api/v1/";
const char *thingsboardServer = "tb.thingcontrol.io";
uint16_t PORT = 1883;
uint32_t lastReconnectAttempt = 0;
unsigned int CountPing = 0;
String new_version;
const char version_url[] = "/Vichayasan/CRA_WaterTreatment/main/bin_version.txt"; //  "/Vichayasan/BMA/refs/heads/main/TX/bin_version.txt"; // "/IndustrialArduino/OTA-on-ESP/release/version.txt";  https://raw.githubusercontent.com/:owner/:repo/master/:path
String firmware_url;
boolean isValid = true;
uint16_t data[64];
uint8_t j, result;

void getMac()
{
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("Get Mac");
  Serial.println("OK");
  Serial.print("+deviceToken: ");
  Serial.println(WiFi.macAddress());
  BT.print("Get Mac");
  BT.println("OK");
  BT.print("+deviceToken: ");
  BT.println(WiFi.macAddress());

  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) {
      deviceToken += "0"; // Add leading zero if needed
    }
    deviceToken += String(mac[i], HEX); // Convert byte to hex
  }
  deviceToken.toUpperCase();

  hostBT.concat(deviceToken);
  // ESPUI.updateLabel(GUI.idLabel, String(deviceToken));

  // --- NEW CODE TO EXTRACT DIGITS ---
  // for (int i = 0; i < deviceToken.length(); i++) {
  //   // Check if the character at the current position is a digit
  //   if (isDigit(deviceToken.charAt(i))) {
  //     // If it is a digit, add it to our new string
  //     digitsOnlyToken += deviceToken.charAt(i);
  //   }
  // }

}


void _initModemGSM(){

  SerialAT.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  modem.sendAT("+QCFG=\"roamservice\",2");
  // modem.sendAT("+SGPIO=0,4,1,1");
  // modem.sendAT("+QGPS=1");
  // modem.testAT();

  Serial.println();
  BT.println();
  
  // Serial.println("debug 01");

  modem.init();
  // modem.enableGPS();
  // delay(30000);

  Serial.println("Initializing modem...");
  BT.println("Initializing modem...");

  String modemInfo = modem.getModemInfo();
  Serial.print("Modem: ");
  Serial.println(modemInfo);
  BT.print("Modem: ");
  BT.println(modemInfo);

  Serial.print("Waiting for network...");
  BT.print("Waiting for network...");

  if (!modem.waitForNetwork())
  {
    GSMnetwork = false;
    Serial.println(" fail");
    BT.println(" fail");
    delay(10000);
    // ESP.restart();
    // debug.initModem = "Fail";
    
  }
  else
  {
    GSMnetwork = true;
    // debug.initModem= "Success";
    Serial.println(" OK");
    Serial.println();
    Serial.print("GSM RSSI: ");
    Serial.println(map(modem.getSignalQuality(), 0, 31, 0, 100));
    Serial.print("SimCCID: ");
    Serial.println(modem.getSimCCID());
    BT.println(" OK");
    BT.println();
    BT.print("GSM RSSI: ");
    BT.println(map(modem.getSignalQuality(), 0, 31, 0, 100));
    BT.print("SimCCID: ");
    BT.println(modem.getSimCCID());
    token = modem.getSimCCID();
    
    // debug..getSimCCID());
    // Serial.print("SimStatus: ");
    // debug.simInfo = modem.getSimStatus();
    // Serial.print("RegistrationStatus: ");
    // Serial.println(modem.getRegistrationStatus());
    // Serial.println();

  }
  delay(2000);

  if (GSMnetwork)
      {
        String showText = "Connecting to ";
        showText += apn;
        showText += " ...";
        
        Serial.print("Connecting to ");
        Serial.print(apn);
        BT.print("Connecting to ");
        BT.print(apn);
        
        if (!modem.gprsConnect(apn, user, pass))
        {
          GSMgprs = false;
          Serial.println(" fail");
          BT.println(" fail");
          
          // debug.simCon = "Fail";
          delay(10000);
        }
        else
        {
          GSMgprs = true;
          Serial.println(" OK");
          BT.println(" OK");
          // debug.simCon = "Success";
        }
        delay(3000);
      }

}

boolean reconnectGSMMqtt()
{
  Serial.print("Connecting to ");
  Serial.print(thingsboardServer);
  BT.print("Connecting to ");
  BT.print(thingsboardServer);


  boolean status = mqttGSM.connect(deviceToken.c_str(), deviceToken.c_str(), NULL);

  if (status == false)
  {
    Serial.println(" fail");
    BT.println(" fail");
    // mqttStatus = "Failed to Connect Server with GSM!";
    // ESPUI.updateLabel(teleLog, String(mqttStatus));
    return false;
  }
  Serial.println(" success");
  BT.println(" success");
  // mqttStatus = "Succeed to Connect Server with GSM!";
  // ESPUI.updateLabel(teleLog, String(mqttStatus));
  Serial.println(F("Connect MQTT Success."));
  BT.println(F("Connect MQTT Success."));
  mqttGSM.subscribe("v1/devices/me/rpc/request/+");
  return mqttGSM.connected();
}

void _initHttp(){

  // serverName.concat(token);
  // serverName.concat("/telemetry");
  // Serial.println("Server Name: " + serverName);
  // BT.println("Server Name: " + serverName);

   mqttGSM.setServer(thingsboardServer, PORT);

  reconnectGSMMqtt();

}

void HttpSend(String msg){

  // String testURI = 

  // http.begin("tb.thingcontrol.io", 80, "/api/v1/"+ token + "/telemetry");

  // http.addHeader("Content-Type", "application/json");

  // int httpResponseCode = http.POST(msg);

  // Serial.print("HTTP Response code: ");
  // Serial.println(httpResponseCode);
  // BT.print("HTTP Response code: ");
  // BT.println(httpResponseCode);
    
  // // Free resources
  // http.end();

  // Length (with one extra character for the null terminator)
  int str_len = msg.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  msg.toCharArray(char_array, str_len);

  mqttGSM.publish("v1/devices/me/telemetry", char_array);


}

void testComposed(){
  json = "";

  json.concat(" {\"Tn\":\"");
  json.concat(deviceToken);
  json.concat("\"");
  json.concat(",\"DO\":");
  json.concat(rand() % 10);
  json.concat(",\"TempDO\":");
  json.concat(rand() % 10);
  json.concat(",\"pH1\":");
  json.concat(rand() % 10);
  json.concat(",\"pH2\":");
  json.concat(rand() % 10);
  json.concat(",\"TempH\":");
  json.concat(rand() % 10);
  json.concat(",\"TSS1\":");
  json.concat(rand() % 10);
  json.concat(",\"Cond\":");
  json.concat(rand() % 10);
  json.concat(",\"TempCond\":");
  json.concat(rand() % 10);
  json.concat(",\"TSS2\":");
  json.concat(rand() % 10);
  json.concat(",\"COD\":");
  json.concat(rand() % 10);
  json.concat(",\"BOD\":");
  json.concat(rand() % 10);
  json.concat(",\"rssi\":");
  json.concat(map(modem.getSignalQuality(), 0, 31, 0, 100));
  json.concat(",\"Version\":\"");
  json.concat(current_version);
  json.concat("\",\"NCCID\":\"");
  json.concat(token);
  json.concat("\"}");

  Serial.println(json);
  BT.println(json);

  HttpSend(json);

}

void reconGSM(){

  if (!modem.isNetworkConnected())
    {
      Serial.println("Network disconnected");
      BT.println("Network disconnected");
      if (!modem.waitForNetwork(180000L, true))
      {
        Serial.println(" fail");
        BT.println(" fail");
        delay(10000);
        return;
      }
      if (modem.isNetworkConnected())
      {
        Serial.println("Network re-connected");
        BT.println("Network re-connected");
      }

      // and make sure GPRS/EPS is still connected
      if (!modem.isGprsConnected())
      {
        Serial.println("GPRS disconnected!");
        Serial.print(F("Connecting to "));
        Serial.print(apn);
        BT.println("GPRS disconnected!");
        BT.print(F("Connecting to "));
        BT.print(apn);
        if (!modem.gprsConnect(apn, user, pass))
        {
          Serial.println(" fail");
          BT.println(" fail");
          delay(10000);
          return;
        }
        if (modem.isGprsConnected())
        {
          Serial.println("GPRS reconnected");
          BT.println("GPRS reconnected");
        }
      }
    }

    if (!mqttGSM.connected())
    {
      Serial.println("=== GSM MQTT NOT CONNECTED ===");
      BT.println("=== GSM MQTT NOT CONNECTED ===");
      // Reconnect every 10 seconds
      uint32_t t = millis() / 1000;
      if (t - lastReconnectAttempt >= 30)
      {
        lastReconnectAttempt = t;

        if (CountPing >= pingCount)
        {
          CountPing = 0;
          ESP.restart();
        }
        CountPing++;

        if (reconnectGSMMqtt())
        {
          CountPing = 0;
          lastReconnectAttempt = 0;
        }
      }
      delay(100);
      return;
    }

    mqttGSM.loop();

}





bool checkForUpdate(String &firmware_url)
{
  // HeartBeat();

  Serial.println("Making GSM GET request securely...");
  BT.println("Making GSM GET request securely...");
  GSMclient.get(version_url);
  int status_code = GSMclient.responseStatusCode();
  delay(1000);
  String response_body = GSMclient.responseBody();
  delay(1000);

  Serial.print("Status code: ");
  Serial.println(status_code);
  Serial.print("Response: ");
  Serial.println(response_body);

  BT.print("Status code: ");
  BT.println(status_code);
  BT.print("Response: ");
  BT.println(response_body);

  response_body.trim();
  response_body.replace("\r", ""); // Remove carriage returns
  response_body.replace("\n", ""); // Remove newlines

  // Extract the version number from the response
  new_version = response_body;

  Serial.println("Current version: " + current_version);
  Serial.println("Available version: " + new_version);

  BT.println("Current version: " + current_version);
  BT.println("Available version: " + new_version);

  GSMclient.stop();

  if (new_version != current_version)
  {
    Serial.println("New version available. Updating...");
    BT.println("New version available. Updating...");
    firmware_url = String("/Vichayasan/CRA_WaterTreatment/main/firmware") + new_version + ".bin";// ***WITHOUT "/raw"***
    Serial.println("Firmware URL: " + firmware_url);
    BT.println("Firmware URL: " + firmware_url);
    return true;
  }
  else
  {
    Serial.println("Already on the latest version");
    BT.println("Already on the latest version");
  }

  return false;
}

// Update the latest firmware which has uploaded to Github
void performOTA(const char *firmware_url)
{
  // HeartBeat();

  // Initialize HTTP
  Serial.println("Making GSM GET firmware OTA request securely...");
  BT.println("Making GSM GET firmware OTA request securely...");
  GSMclient.get(firmware_url);
  int status_code = GSMclient.responseStatusCode();
  delay(1000);
  long contentlength = GSMclient.contentLength();
  delay(1000);

  Serial.print("Contentlength: ");
  Serial.println(contentlength);

  BT.print("Contentlength: ");
  BT.println(contentlength);

  if (status_code == 200)
  {

    if (contentlength <= 0)
    {
      Serial.println("Failed to get content length");
      BT.println("Failed to get content length");
      GSMclient.stop();
      return;
    }

    // Begin OTA update
    bool canBegin = Update.begin(contentlength);
    size_t written;
    long totalBytesWritten = 0;
    uint8_t buffer[1024];
    int bytesRead;
    long contentlength_real = contentlength;

    if (canBegin)
    {
      // HeartBeat();

      while (contentlength > 0)
      {
        // HeartBeat();

        bytesRead = GSMclient.readBytes(buffer, sizeof(buffer));
        if (bytesRead > 0)
        {
          written = Update.write(buffer, bytesRead);
          if (written != bytesRead)
          {
            Serial.println("Error: written bytes do not match read bytes");
            BT.println("Error: written bytes do not match read bytes");
            Update.abort();
            return;
          }
          totalBytesWritten += written; // Track total bytes written

          Serial.printf("Write %.02f%% (%ld/%ld)\n", (float)totalBytesWritten / (float)contentlength_real * 100.0, totalBytesWritten, contentlength_real);
          BT.printf("Write %.02f%% (%ld/%ld)\n", (float)totalBytesWritten / (float)contentlength_real * 100.0, totalBytesWritten, contentlength_real);
          
          String OtaStat = "OTA Updating: " + String((float)totalBytesWritten / (float)contentlength_real * 100.0) + " % ";
          

          contentlength -= bytesRead; // Reduce remaining content length
        }
        else
        {
          Serial.println("Error: Timeout or no data received");
          BT.println("Error: Timeout or no data received");
          break;
        }
      }

      if (totalBytesWritten == contentlength_real)
      {
        Serial.println("Written : " + String(totalBytesWritten) + " successfully");
        BT.println("Written : " + String(totalBytesWritten) + " successfully");
      }
      else
      {
        Serial.println("Written only : " + String(written) + "/" + String(contentlength_real) + ". Retry?");
        BT.println("Written only : " + String(written) + "/" + String(contentlength_real) + ". Retry?");
      }

      if (Update.end())
      {
        Serial.println("OTA done!");
        BT.println("OTA done!");
        if (Update.isFinished())
        {
          Serial.println("Update successfully completed. Rebooting.");
          BT.println("Update successfully completed. Rebooting.");
          delay(300);
          ESP.restart();
        }
        else
        {
          Serial.println("Update not finished? Something went wrong!");
          BT.println("Update not finished? Something went wrong!");
        }
      }
      else
      {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
        BT.println("Error Occurred. Error #: " + String(Update.getError()));
      }
    }
    else
    {
      Serial.println("Not enough space to begin OTA");
      BT.println("Not enough space to begin OTA");
    }
  }
  else
  {
    Serial.println("Cannot download firmware. HTTP code: " + String(status_code));
    BT.println("Cannot download firmware. HTTP code: " + String(status_code));
  }

  GSMclient.stop();
}

void GSM_OTA()
{
  Serial.println("---- GSM OTA Check version before update ----");
  BT.println("---- GSM OTA Check version before update ----");

  if (checkForUpdate(firmware_url))
  {
    performOTA(firmware_url.c_str());
  }
}

float HexTofloat(uint32_t x)
{
  return (*(float*)&x);
}

void getMODBUS()
{
  float i = 0;

  modbus.begin(9600, SERIAL_8N2, 16, 17);

  result = node.readHoldingRegisters(0x0000, 64);
  delay(1000);
  // do something with data if read is successful
  if (result == node.ku8MBSuccess)
  {
    for (j = 0; j < 64; j++)
    {
      data[j] = node.getResponseBuffer(j);
      Serial.print(j); Serial.print(":");      Serial.print(data[j]); Serial.print(",");
      BT.print(j); BT.print(":");      BT.print(data[j]); BT.print(",");

    }
    BT.println();
    Serial.println();
  }

}


float getDO()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[4];
  value = value << 16;
  value = value + data[5];
  param = HexTofloat(value);
  if (param <= 0)
    isValid = false;
  else
    isValid = true;
  return param;
}

float getTempDO()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[6];
  value = value << 16;
  value = value + data[7];
  param = HexTofloat(value);
  if (param <= 0)
    isValid = false;
  else
    isValid = true;
  return param;
}

float getPh1()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[12];
  value = value << 16;
  value = value + data[13];
  param = HexTofloat(value);
  if (param <= 0)
    isValid = false;
  else
    isValid = true;
  return param;
}

float getPh2()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[36];
  value = value << 16;
  value = value + data[37];
  param = HexTofloat(value);
  if (param <= 0)
    isValid = false;
  else
    isValid = true;
  return param;
}
float getTempH()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[14];
  value = value << 16;
  value = value + data[15];
  param = HexTofloat(value);
  if (param <= 0)
    isValid = false;
  else
    isValid = true;
  return param;
}

float getTSS1()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[20];
  value = value << 16;
  value = value + data[21];
  param = HexTofloat(value);

  return param;
}

float getCond()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[28];
  value = value << 16;
  value = value + data[29];
  param = HexTofloat(value);

  return param;
}


float getTempCond()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[30];
  value = value << 16;
  value = value + data[31];
  param = HexTofloat(value);

  return param;
}

float getTSS2()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[52];
  value = value << 16;
  value = value + data[53];
  param = HexTofloat(value);

  return param;
}
float getCOD()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[46];
  value = value << 16;
  value = value + data[47];
  param = HexTofloat(value);

  return param;
}
float getBOD()
{
  float param = 0.0;
  uint32_t value = 0;

  value = data[60];
  value = value << 16;
  value = value + data[61];
  BT.println(value);
  param = HexTofloat(value);


  return param;
}

void readMeter()
{
  node.begin(ID, modbus);
  Serial.print("Start");
  getMODBUS();

  device.DO = getDO();//
  device.TempDO = getTempDO(); //

  device.pH1 = getPh1(); //

  device.TempH = getTempH(); //

  device.TSS1 = getTSS1(); //
  device.Cond = getCond();
  device.TempCond = getTempCond();
  device.TSS2 = getTSS2(); //
  device.COD = getCOD();

  device.BOD = getBOD();
  device.pH2 = getPh2();




  Serial.print("DO: ");  Serial.println(device.DO);
  Serial.print("TempDo: ");  Serial.println(device.TempDO);
  Serial.print("pH1: ");  Serial.println(device.pH1);
  Serial.print("TempH: ");  Serial.println(device.TempH);

  Serial.print("Tss1: ");  Serial.println(device.TSS1);
  Serial.print("COD: ");  Serial.println(device.COD);
  Serial.print("BOD: ");  Serial.println(device.BOD);
  Serial.print("pH2: ");  Serial.println(device.pH2);

  BT.print("DO: ");  BT.println(device.DO);
  BT.print("TempDo: ");  BT.println(device.TempDO);
  BT.print("TempH: ");  BT.println(device.TempH);
  BT.print("pH1: ");  BT.println(device.pH1);
  BT.print("pH2: ");  BT.println(device.pH2);
  BT.print("COD: ");  BT.println(device.COD);
  BT.print("BOD: ");  BT.println(device.BOD);
  BT.print("Tss1: ");  BT.println(device.TSS1);
  BT.print("Cond: ");  BT.println(device.Cond);
  BT.print("TempCond: ");  BT.println(device.TempCond);
  BT.print("Tss2: ");  BT.println(device.TSS2);
  BT.println("");
  Serial.println("");
  
}

void t2CallsendViaGSM ()
{
  readMeter();

  Serial.println("#####################t2CallsendViaGSM###################");
  BT.println("#####################t2CallsendViaGSM###################");

  // HeartBeat();

  json = "";

  json.concat(" {\"Tn\":\"");
  json.concat(deviceToken);
  json.concat("\"");

  json.concat(",\"DO\":");
  json.concat(device.DO);
  json.concat(",\"TempDO\":");
  json.concat(device.TempDO);
  json.concat(",\"pH1\":");
  json.concat(device.pH1);
  json.concat(",\"pH2\":");
  json.concat(device.pH2);
  json.concat(",\"TempH\":");
  json.concat(device.TempH);
  json.concat(",\"TSS1\":");
  json.concat(device.TSS1);
  json.concat(",\"Cond\":");
  json.concat(device.Cond);
  json.concat(",\"TempCond\":");
  json.concat(device.TempCond);
  json.concat(",\"TSS2\":");
  json.concat(device.TSS2);
  json.concat(",\"COD\":");
  json.concat(device.COD);
  json.concat(",\"BOD\":");
  json.concat(device.BOD);
  json.concat(",\"rssi\":");
  json.concat(map(modem.getSignalQuality(), 0, 31, 0, 100));
  json.concat(",\"Version\":\"");
  json.concat(current_version);
  json.concat("\",\"NCCID\":\"");
  json.concat(token);
  json.concat("\"}");

  Serial.println(json);
  BT.println(json);

  HttpSend(json);

}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  modbus.begin(9600, SERIAL_8N2, 16, 17);

  getMac();

  BT.begin(hostBT);

  Serial.println("version: " + current_version);
  BT.println("version: " + current_version);

  // secure_layer_mqtt.setInsecure();
  secure_layer.setInsecure();

  _initModemGSM();

  _initHttp();

  GSM_OTA();

}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long ms = millis();

  reconGSM();

  if (ms - previousMillis >= 60000){
    previousMillis += 60000;

    // testComposed();
    t2CallsendViaGSM();


  }

  if (ms - periodOTA >= 120000){
    periodOTA += 120000;

    GSM_OTA();

  }

}
