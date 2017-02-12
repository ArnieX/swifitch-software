#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Constants
String autoconf_ssid          = "SWIFITCH_"+String(ESP.getChipId());   // AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char* autoconf_pwd      = "PASSWORD";                            // AP password so noone else can connect to the ESP in case your router fails

// WiFi Manager defaults
char mqtt_server[40]    = "mqtt.swifitch.cz";   // Public broker
char mqtt_port[6]       = "1883";
char mqtt_username[20]  = "";
char mqtt_password[40]  = "";
char home_name[40]      = "myhome";
char room[40]           = "room";
char device_name[40]    = "swifitch-one";

// MQTT Constants
String mqtt_devicestatus_set_topic    = "";
String mqtt_relayone_set_topic        = "";
String mqtt_relayone_get_topic        = "";
String mqtt_pingall_get_topic         = "";
String mqtt_pingallresponse_set_topic = "";
String mqtt_pingall_response_text     = "";

// Global
byte relayone_state = 1;
long lastReconnectAttempt = 0;
bool shouldSaveConfig = false;

// Callback to save config
void saveConfigCallback () {
  shouldSaveConfig = true;
}

WiFiClient espClient;
PubSubClient client(espClient);

void setup_ota() {

  // Set OTA Password, and change it in platformio.ini
  ArduinoOTA.setPassword("SWIFITCH_PASSWORD");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR);          // Auth failed
    else if (error == OTA_BEGIN_ERROR);    // Begin failed
    else if (error == OTA_CONNECT_ERROR);  // Connect failed
    else if (error == OTA_RECEIVE_ERROR);  // Receive failed
    else if (error == OTA_END_ERROR);      // End failed
  });
  ArduinoOTA.begin();

}

boolean reconnect() {

  // MQTT reconnection function

    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect
    if ( strcmp(mqtt_username,"") == 0 || strcmp(mqtt_password,"") == 0 ) {

      if ( client.connect(clientId.c_str(), mqtt_devicestatus_set_topic.c_str(), 0, false, "disconnected") ) {

        mqttConnected();

      }

    } else {

      if ( client.connect(clientId.c_str(), (char*)mqtt_username, (char*)mqtt_password, mqtt_devicestatus_set_topic.c_str(), 0, false, "disconnected") ) {

        mqttConnected();

      }

    }

    return client.connected();

}

void mqttConnected() {

  // Once connected, publish an announcement...
  client.publish(mqtt_devicestatus_set_topic.c_str(), "connected");
  client.publish(mqtt_relayone_set_topic.c_str(), "1");
  // ... and resubscribe
  client.subscribe(mqtt_relayone_get_topic.c_str());
  client.subscribe(mqtt_pingall_get_topic.c_str());

}

void callback(char* topic, byte* payload, unsigned int length) {

    char c_payload[length];
    memcpy(c_payload, payload, length);
    c_payload[length] = '\0';

    String s_topic = String(topic);         // Topic
    String s_payload = String(c_payload);   // Message content

  // Handling incoming MQTT messages

  blink();

    if ( s_topic == mqtt_relayone_get_topic.c_str() ) {

      if (s_payload == "1") {

        if (relayone_state != 1) {

          digitalWrite(D1,LOW);
          client.publish(mqtt_relayone_set_topic.c_str(), "1");
          relayone_state = 1;

        }

      } else if (s_payload == "0") {

        if (relayone_state != 0) {

          digitalWrite(D1,HIGH);
          client.publish(mqtt_relayone_set_topic.c_str(), "0");
          relayone_state = 0;

        }

      }

    } else if ( s_topic == mqtt_pingall_get_topic.c_str() ) {

      client.publish(mqtt_pingallresponse_set_topic.c_str(), mqtt_pingall_response_text.c_str());
      client.publish(mqtt_devicestatus_set_topic.c_str(), "connected");

    }

}

void blink() {

  //Blink built-in LED
  digitalWrite(D6, HIGH);
  delay(25);
  digitalWrite(D6, LOW);

}

void setup() {

  //Relay setup
  pinMode(D1,OUTPUT);       //Initialize relay GPIO05 > NODEMCU pin D1
  digitalWrite(D1, LOW);    //Set relay OFF by default - relay NC ON/ NO OFF (so when you flip the switch it will be on) GPIO05 > NODEMCU pin D1
  pinMode(D6, OUTPUT);      //Initialize the SWIFITCH built-in LED - GPIO12 > NODEMCU pin D6
  digitalWrite(D6, HIGH);   //Turn on SWIFITCH built-in LED
  Serial.begin(115200);

  // WiFi Configuration
  // Read settings from config.json

  if (SPIFFS.begin()) {
    if (SPIFFS.exists("/config.json")) {
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(home_name, json["home"]);
          strcpy(room, json["room"]);
          strcpy(device_name, json["device"]);

        }
      }
    }
  }
  // End read

  // The extra parameters to be configured
  WiFiManagerParameter custom_text_mqtt("<p>MQTT Settings</p>");
  WiFiManagerParameter custom_text_home("<p>Home settings</p>");
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_username("username", "MQTT Username", mqtt_username, 20);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 40);
  WiFiManagerParameter custom_home_name("home", "Home_name", home_name, 40);
  WiFiManagerParameter custom_room("room", "Room", room, 40);
  WiFiManagerParameter custom_device_name("device", "Device_name", device_name, 40);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Callback for saving configuration
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // WiFi Manager custom parameters
  wifiManager.addParameter(&custom_text_mqtt);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_text_home);
  wifiManager.addParameter(&custom_home_name);
  wifiManager.addParameter(&custom_room);
  wifiManager.addParameter(&custom_device_name);

  // Will reset settings (for testing purposes) comment after single reset!!!
  //wifiManager.resetSettings();

  // Blocking loop awaiting configuration
  if (!wifiManager.autoConnect(autoconf_ssid.c_str(),autoconf_pwd)) {
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  // Read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(home_name, custom_home_name.getValue());
  strcpy(room, custom_room.getValue());
  strcpy(device_name, custom_device_name.getValue());

  // Save custom parameters to FS
  if (shouldSaveConfig) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"]   = mqtt_server;
    json["mqtt_port"]     = mqtt_port;
    json["mqtt_username"] = mqtt_username;
    json["mqtt_password"] = mqtt_password;
    json["home"]          = home_name;
    json["room"]          = room;
    json["device"]        = device_name;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {}

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    // End save
  }

  mqtt_devicestatus_set_topic    = String(home_name)+"/"+String(room)+"/"+String(device_name)+"/devicestatus";
  mqtt_relayone_set_topic        = String(home_name)+"/"+String(room)+"/"+String(device_name)+"/status";
  mqtt_relayone_get_topic        = String(home_name)+"/"+String(room)+"/"+String(device_name);
  mqtt_pingall_get_topic         = String(home_name)+"/pingall";
  mqtt_pingallresponse_set_topic = String(home_name)+"/pingallresponse";
  mqtt_pingall_response_text     = "{\""+String(room)+"/"+String(device_name)+"\":\"connected\"}";

  // After WiFi Configuration
  wifi_station_set_hostname((char*)device_name);
  setup_ota();
  MDNS.begin((char*)device_name);
  client.setServer((char*)mqtt_server, atoi(&mqtt_port[0]));
  client.setCallback(callback);
  lastReconnectAttempt = 0;
  digitalWrite(D6, LOW);   //Turn off LED as default, also signal that setup is over

}

void loop() {

  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }
  ArduinoOTA.handle();

}