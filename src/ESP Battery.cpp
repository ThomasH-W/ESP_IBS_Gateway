/*
 ESP8266 Sensor Module
 - WiFi Mgr
 - SPIFFS Config
 - MQTT
 - DeepSleep
 - OTA ????

 ToDo:
 - ISB 200 Sensor
 - read MQTT topics: 
    stayawake (if last message is >15min goto sleep)

 Config: wifi, mqtt, PwrSupply (module is on battery), Function: last sensor value, sensor config

 DeepSleep: Sleep will cause a reset: all variables are gone, need EEPROM or SPIFFS to save status
 quick re-connect using last working IP+DNS+MAC ....
 OTA: send OTA request to module ?
 MQTT: read sticky status of supply voltage: KL15/PowerGrid

 JSON Validation:  https://jsonformatter.curiousconcept.com/

Referenzen:
  http://www.rainer-rebhan.de/proj_FeuerM_OTA.html
    Sleep Jumper
  https://www.arduino-hausautomation.de/2017/esp8266-im-tiefschlaf/
  https://www.open-homeautomation.com/de/2016/06/12/battery-powered-esp8266-sensor/
*/

#include <arduino.h>
#include <FS.h> //this needs to be first, or it all crashes and burns...

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#else
#include <WiFi.h>
#endif

//needed for library
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson

// need to enlagre packet size
#define MQTT_MAX_PACKET_SIZE 250
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient

#ifdef PASSWORD_THOMAS
#include "EspBattery_Thomas.h"
#else
#include "EspBattery.h"
#endif

#include <IBS_Hella_200.h>

// const int sleepSeconds = 60 * 15;
const int sleepSeconds = 60 * 1;

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

AsyncWebServer server(80);
DNSServer dns;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
long lastMsg = 0;
char msg[200];
int value = 0;

struct Config
{
  char mqtt_server[40];
  char mqtt_port[6];
  char mqtt_topic_data1[40];
  uint32 mqtt_port_i;
};
Config config; // <- global configuration object

//void mqtt_callback(char *topic, byte *payload, unsigned int length);
//PubSubClient mqttClient(config.mqtt_server, config.mqtt_port_i, mqtt_callback, espClient);

char mqtt_msg[200];

const char *filename = SPIFFS_COMFIG_FILE; // <- SD library uses 8.3 filenames

//----------------------------------------------------------------------------------------------------------------------
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (uint16 i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1')
  {
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  }
  else
  {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
  }
}

//----------------------------------------------------------------------------------------------------------------------
// not being used if module is using deep sleep
void mqtt_reconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = DEFAULT_MQTT_CLIENT;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish(MQTT_TOPIC_DATA1, "hello world");
      // ... and resubscribe
      mqttClient.subscribe("inTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//-----------------------------------------------------------------------------------------------------
// Loads the configuration from a file
void SpiffsLoadConfiguration(const char *filename, Config &config)
{
  Serial.println(F("Loading configuration..."));

  // Open file for reading
  File file = SPIFFS.open(filename, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  strlcpy(config.mqtt_server,                       // <- destination
          doc["mqtt_server"] | DEFAULT_MQTT_SERVER, // <- source
          sizeof(config.mqtt_server));              // <- destination's capacity

  strlcpy(config.mqtt_port,                     // <- destination
          doc["mqtt_port"] | DEFAULT_MQTT_PORT, // <- source
          sizeof(config.mqtt_port));            // <- destination's capacity

  config.mqtt_port_i = doc["mqtt_port_i"] | DEFAULT_MQTT_PORT_I;

  strlcpy(config.mqtt_topic_data1,                    // <- destination
          doc["mqtt_topic_data1"] | MQTT_TOPIC_DATA1, // <- source
          sizeof(config.mqtt_topic_data1));           // <- destination's capacity

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

//-----------------------------------------------------------------------------------------------------
// Saves the configuration to a file
void SpiffsSaveConfiguration(const char *filename, const Config &config)
{
  // Delete existing file, otherwise the configuration is appended to the file
  SPIFFS.remove(filename);

  // Open file for writing
  File file = SPIFFS.open(filename, "w");
  if (!file)
  {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<256> doc;

  // Set the values in the document
  doc["mqtt_server"] = config.mqtt_server;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_port_i"] = config.mqtt_port_i;
  doc["mqtt_topic_data1"] = config.mqtt_topic_data1;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
} // end of function

//-----------------------------------------------------------------------------------------------------
// Prints the content of a file to the Serial
void printFile(const char *filename)
{
  // Open file for reading
  File file = SPIFFS.open(filename, "r");
  if (!file)
  {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available())
  {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
} // end of function

//-----------------------------------------------------------------------------------------------------
void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  delay(1000);
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }

  // Should load default config if run for the first time
  SpiffsLoadConfiguration(filename, config);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  AsyncWiFiManagerParameter custom_mqtt_server("server", "mqtt server", config.mqtt_server, 40);
  AsyncWiFiManagerParameter custom_mqtt_port("port", "mqtt port", config.mqtt_port, 5);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  AsyncWiFiManager wifiManager(&server, &dns);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  // wifiManager.setSTAStaticIPConfig(IPAddress(192, 168, 178, 99), IPAddress(192, 168, 178, 1), IPAddress(255, 255, 255, 0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep // in seconds
  wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("WLAN connected...yeey :)");

  //read updated parameters
  strcpy(config.mqtt_server, custom_mqtt_server.getValue());
  strcpy(config.mqtt_port, custom_mqtt_port.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    // Create configuration file
    Serial.println(F("Saving configuration..."));
    SpiffsSaveConfiguration(filename, config);
    printFile(filename); // dump config
  }

  Serial.print("local ip: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(config.mqtt_server, config.mqtt_port_i);
  mqttClient.setCallback(mqtt_callback);
  mqttClient.connect(DEFAULT_MQTT_CLIENT, DEFAULT_MQTT_USER, DEFAULT_MQTT_PASSWD);

  IBS_LIN_Setup(0);
  IBS_LIN_Read(mqtt_msg);
  Serial.begin(115200);

  // mqttClient.publish(config.mqtt_topic_data1, "pre JSON");
  Serial.print("MQTT pub: ");
  Serial.print(config.mqtt_topic_data1);
  Serial.print(" : ");
  Serial.println(mqtt_msg);
  mqttClient.publish(config.mqtt_topic_data1, mqtt_msg);

  // mqttClient.publish(config.mqtt_topic_data1, "post JSON");
  Serial.println("MQTT flush");
  mqttClient.flush();

  Serial.printf("Sleep deeply for %i seconds...", sleepSeconds);
  Serial.flush();
  delay(200);

  ESP.deepSleep(sleepSeconds * 1000000);

} // end of function

void loop()
{
  // put your main code here, to run repeatedly:
} // end of function
