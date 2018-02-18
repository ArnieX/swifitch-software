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
#include <fauxmoESP.h>

// Constants
String autoconf_ssid          = "SWIFITCH_"+String(ESP.getChipId());   // AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char* autoconf_pwd      = "PASSWORD";                            // AP password so noone else can connect to the ESP in case your router fails
std::unique_ptr<ESP8266WebServer> server;

// WiFi Manager defaults
char mqtt_server[40]       = "mqtt.swifitch.cz";   // Public broker
char mqtt_port[6]          = "1883";
char mqtt_username[20]     = "";
char mqtt_password[40]     = "";
char home_name[40]         = "myhome";
char room[40]              = "room";
char device_name[40]       = "swifitch-one";

//#define FAUXMO //Uncomment for enabling Fake WeMo Switch for Alexa to discover and control - may not work with Gen2 devices
#ifdef FAUXMO
// WeMo Emulation
fauxmoESP fauxmo;
#endif

// MQTT Constants
String mqtt_devicestatus_set_topic    = "";
String mqtt_relay_set_topic           = "";
String mqtt_relay_get_status_topic    = "";
String mqtt_relay_get_topic           = "";
String mqtt_pingall_get_topic         = "";
String mqtt_pingallresponse_set_topic = "";
String mqtt_pingall_response_text     = "";

// Global
byte relay_state = 1;
long lastReconnectAttempt = 0;
bool shouldSaveConfig = false;

//#define PHYSwitch //Uncomment for enabling physical switch connected between D2 and GND
#ifdef PHYSwitch
// Physical switch
long lastPHYSwitchCheck = 0;
int debounce_ms = 100; // Check for physical switch state change each ... ms
int last_switch_state = 1;
// END PHYSwitch
#endif

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
  client.publish(mqtt_relay_set_topic.c_str(), "1");
  // ... and resubscribe
  client.subscribe(mqtt_relay_get_topic.c_str());
  client.subscribe(mqtt_relay_get_status_topic.c_str());
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

    if ( s_topic == mqtt_relay_get_topic.c_str() ) {

      if (s_payload == "1") {

        if (relay_state != 1) {

          digitalWrite(D1,LOW);   // LOW when output is NC, HIGH if output is NO
          client.publish(mqtt_relay_set_topic.c_str(), "1");
          relay_state = 1;

        }

      } else if (s_payload == "0") {

        if (relay_state != 0) {

          digitalWrite(D1,HIGH);  // HIGH when output is NC, LOW if output is NO
          client.publish(mqtt_relay_set_topic.c_str(), "0");
          relay_state = 0;

        }

      }

    } else if ( s_topic == mqtt_relay_get_status_topic.c_str() ) {

      int current_relay_status = digitalRead(D1);

      if ( current_relay_status == 1 ) {

        client.publish(mqtt_relay_set_topic.c_str(), "0");  // 0 if output is NC, 1 if output is NO

      } else if ( current_relay_status == 0 ) {

        client.publish(mqtt_relay_set_topic.c_str(), "1");  // 1 if output is NC, 0 if output is NO

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

void toggle() {

  if (relay_state != 1) {

    digitalWrite(D1, LOW); // LOW when output is NC, HIGH if output is NO
    client.publish(mqtt_relay_set_topic.c_str(), "1");
    relay_state = 1;

    blink();

  } else if (relay_state != 0) {

    digitalWrite(D1, HIGH); // HIGH when output is NC, LOW if output is NO
    client.publish(mqtt_relay_set_topic.c_str(), "0");
    relay_state = 0;

    blink();

  }
}

void setup() {

  //Relay setup
  pinMode(D1,OUTPUT);         //Initialize relay GPIO05 > NODEMCU pin D1
  digitalWrite(D1,LOW);       //Set relay OFF by default - relay NC ON/ NO OFF (so when you flip the switch it will be on) GPIO05 > NODEMCU pin D1
  pinMode(D6,OUTPUT);         //Initialize the SWIFITCH built-in LED - GPIO12 > NODEMCU pin D6
  digitalWrite(D6,HIGH);      //Turn on SWIFITCH built-in LED

  #ifdef PHYSwitch
  pinMode(D2,INPUT_PULLUP);   //Physical switch ON/OFF
  #endif

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
  WiFiManagerParameter custom_text_home("<p>Home Settings</p>");
  WiFiManagerParameter custom_text_amazon("<p>Amazon Echo Settings</p>");
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

  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

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
    json["mqtt_server"]        = mqtt_server;
    json["mqtt_port"]          = mqtt_port;
    json["mqtt_username"]      = mqtt_username;
    json["mqtt_password"]      = mqtt_password;
    json["home"]               = home_name;
    json["room"]               = room;
    json["device"]             = device_name;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {}

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    // End save
  }

  mqtt_devicestatus_set_topic     = String(home_name)+"/"+String(room)+"/"+String(device_name)+"/devicestatus";
  mqtt_relay_set_topic            = String(home_name)+"/"+String(room)+"/"+String(device_name)+"/status";
  mqtt_relay_get_status_topic     = String(home_name)+"/"+String(room)+"/"+String(device_name)+"/getstatus";
  mqtt_relay_get_topic            = String(home_name)+"/"+String(room)+"/"+String(device_name);
  mqtt_pingall_get_topic          = String(home_name)+"/pingall";
  mqtt_pingallresponse_set_topic  = String(home_name)+"/pingallresponse";
  mqtt_pingall_response_text      = "{\""+String(room)+"/"+String(device_name)+"\":\"connected\"}";

  // After WiFi Configuration
  wifi_station_set_hostname((char*)device_name);
  setup_ota();
  MDNS.begin((char*)device_name);
  client.setServer((char*)mqtt_server, atoi(&mqtt_port[0]));
  client.setCallback(callback);
  lastReconnectAttempt = 0;

  server->on("/", handleRootPath);
  server->on("/toggle", handleTogglePath);
  server->on("/info", handleInfoPath);
  server->on("/mqtt", handleMQTTPath);
  server->on("/reset", handleResetPath);
  server->onNotFound(handleNotFound);

  server->begin();
  Serial.println("HTTP server started");
  Serial.println(WiFi.localIP());


  #ifdef FAUXMO
    // WeMo Emulation
    fauxmo.addDevice(device_name);
    fauxmo.enable(true);
    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state) {
      toggle();
    });
    fauxmo.onGetState([](unsigned char device_id, const char * device_name) {

      int current_relay_status = digitalRead(D1);

      if ( current_relay_status == 1 ) {

        return true;

      } else if ( current_relay_status == 0 ) {

        return false;

      }

    });
  #endif

digitalWrite(D6, LOW);   //Turn off LED as default, also signal that setup is over

}

void handleRootPath() {

   Serial.println("Root loaded");
   server->send(200, "text/plain", "Webserver is running");

}

void handleTogglePath() {

   toggle();

   StaticJsonBuffer<200> jsonBuffer;
   JsonObject& root = jsonBuffer.createObject();
   root["response"] = "success";
   root["relay_state"] = digitalRead(D1);

   String json;
   root.prettyPrintTo(json);

   server->send(200, "application/json", json);

}

String ipToString(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}

void handleInfoPath() {

   StaticJsonBuffer<1000> jsonBuffer;
   JsonObject& root = jsonBuffer.createObject();
   root["response"] = "success";
   root["relay_state"] = digitalRead(D1);
   root["ip_address"] = ipToString(WiFi.localIP());
   root["mac_address"] = WiFi.macAddress();
   root["device_name"] = device_name;

   root["chip_id"] = ESP.getChipId();
   root["flash_chip_id"] = ESP.getFlashChipId();
   root["flash_chip_size"] = ESP.getFlashChipSize();

   String json;
   root.prettyPrintTo(json);

   server->send(200, "application/json", json);

}

void handleMQTTPath() {

   StaticJsonBuffer<1000> jsonBuffer;
   JsonObject& root = jsonBuffer.createObject();
   root["response"] = "success";
   root["mqtt_status"] = client.connected();
   root["mqtt_broker"] = mqtt_server;
   root["mqtt_port"] = mqtt_port;
   root["mqtt_username"] = mqtt_username;
   root["mqtt_password"] = mqtt_password;
   root["mqtt_devicestatus_set_topic"] = mqtt_devicestatus_set_topic;
   root["mqtt_relay_set_topic"] = mqtt_relay_set_topic;
   root["mqtt_relay_get_status_topic"] = mqtt_relay_get_status_topic;
   root["mqtt_relay_get_topic"] = mqtt_relay_get_topic;
   root["mqtt_pingall_get_topic"] = mqtt_pingall_get_topic;
   root["mqtt_pingallresponse_set_topic"] = mqtt_pingallresponse_set_topic;
   root["mqtt_pingall_response_text"] = mqtt_pingall_response_text;

   String json;
   root.prettyPrintTo(json);

   server->send(200, "application/json", json);

}

void handleResetPath() {

   server->send(200, "text/plain", "Reset successful, powercycle your device.");
   delay(500);
   WiFiManager wifiManager;
   wifiManager.resetSettings();
   delay(500);
   ESP.reset();

}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}

#ifdef PHYSwitch
void physicalSwitch() {

  long now = millis();

  if (now - lastPHYSwitchCheck > debounce_ms) {

    lastPHYSwitchCheck = now;

    int switch_state = digitalRead(D2);

    if (last_switch_state != switch_state) {

      toggle();

    }

    last_switch_state = switch_state;

  }

}
#endif

void loop() {

  long now = millis();

  if (!client.connected()) {

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
  server->handleClient();

#ifdef FAUXMO
  fauxmo.handle();
#endif

#ifdef PHYSwitch
  physicalSwitch();
#endif

}
