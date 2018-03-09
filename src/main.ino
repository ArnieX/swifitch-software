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
//#include <fauxmoESP.h>

// Constants
String autoconf_ssid          = "SWIFITCH_"+String(ESP.getChipId());   // AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char* autoconf_pwd      = "PASSWORD";                            // AP password so noone else can connect to the ESP in case your router fails
const char* www_username      = "admin";                               // HTTP Basic Auth for config page
const char* www_password      = "swifitch";                            // HTTP Basic Auth for config page

std::unique_ptr<ESP8266WebServer> server;

//HTML Holder
String config_page;

// WiFi Manager defaults
char hostname[40]          = "";
char mqtt_status[5]        = "0";
char mqtt_server[40]       = "";   // Public broker
char mqtt_port[6]          = "";
char mqtt_username[20]     = "";
char mqtt_password[40]     = "";
char home_name[40]         = "";
char room[40]              = "";
char device_name[40]       = "";

char physwitch[5]          = "0";
char alexa_status[5]       = "0";
char alexa_name[40]        = "";

// WeMo Emulation
//fauxmoESP fauxmo;


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

// Physical switch
long lastPHYSwitchCheck = 0;
int debounce_ms = 100; // Check for physical switch state change each ... ms
int last_switch_state = 1;
// END PHYSwitch

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

  if ( strcmp(physwitch,"1") != 0 ) {
  pinMode(D2,INPUT_PULLUP);   //Physical switch ON/OFF
  }

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

          strcpy(hostname, json["hostname"]);
          strcpy(mqtt_status, json["mqtt_status"]);
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(home_name, json["home"]);
          strcpy(room, json["room"]);
          strcpy(device_name, json["device"]);
          strcpy(physwitch, json["physwitch"]);
          strcpy(alexa_status, json["alexa_status"]);
          strcpy(alexa_name, json["alexa_name"]);

        }
      }
    }
  }
  // End read

  // The extra parameters to be configured
  /*WiFiManagerParameter custom_text_mqtt("<p>MQTT Settings</p>");
  WiFiManagerParameter custom_text_home("<p>Home Settings</p>");
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_username("username", "MQTT Username", mqtt_username, 20);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 40);
  WiFiManagerParameter custom_home_name("home", "Home_name", home_name, 40);
  WiFiManagerParameter custom_room("room", "Room", room, 40);
  WiFiManagerParameter custom_device_name("device", "Device_name", device_name, 40);*/

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // Callback for saving configuration
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // WiFi Manager custom parameters
  /*wifiManager.addParameter(&custom_text_mqtt);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_text_home);
  wifiManager.addParameter(&custom_home_name);
  wifiManager.addParameter(&custom_room);
  wifiManager.addParameter(&custom_device_name);*/

  // Will reset settings (for testing purposes) comment after single reset!!!
  //wifiManager.resetSettings();

  // Blocking loop awaiting configuration
  if (!wifiManager.autoConnect(autoconf_ssid.c_str(),autoconf_pwd)) {
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  // Read updated parameters
  /*strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_username, custom_mqtt_username.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  strcpy(home_name, custom_home_name.getValue());
  strcpy(room, custom_room.getValue());
  strcpy(device_name, custom_device_name.getValue());*/

  // Save custom parameters to FS
  if (shouldSaveConfig) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["hostname"]           = hostname;
    json["mqtt_status"]        = mqtt_status;
    json["mqtt_server"]        = mqtt_server;
    json["mqtt_port"]          = mqtt_port;
    json["mqtt_username"]      = mqtt_username;
    json["mqtt_password"]      = mqtt_password;
    json["home"]               = home_name;
    json["room"]               = room;
    json["device"]             = device_name;
    json["physwitch"]          = physwitch;
    json["alexa_status"]       = alexa_status;
    json["alexa_name"]         = alexa_name;


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
  if ( strcmp(hostname,"") != 0 ) {
    wifi_station_set_hostname((char*)hostname);
    MDNS.begin((char*)hostname);
  } else {
    wifi_station_set_hostname((char*)"SWIFITCH_UNCONFIGURED");
    MDNS.begin((char*)"SWIFITCH_UNCONFIGURED");
  }
  setup_ota();

  // MQTT Could be disabled in web config
  if ( strcmp(mqtt_status,"1") == 0 ) {
    client.setServer((char*)mqtt_server, atoi(&mqtt_port[0]));
    client.setCallback(callback);
  }

  lastReconnectAttempt = 0;

  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

  server->on("/", handleRootPath);
  server->on("/toggle", handleTogglePath);
  server->on("/info", handleInfoPath);
  server->on("/mqtt", handleMQTTPath);
  server->on("/config", handleConfigPath);
  server->onNotFound(handleNotFound);

  server->begin();
  Serial.println("HTTP server started");
  Serial.println(WiFi.localIP());

/*
  if ( strcmp(alexa_status,"1") == 0 ) {
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
  }
*/

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
   root["response"]         = "success";
   root["relay_state"]      = digitalRead(D1);
   root["ip_address"]       = ipToString(WiFi.localIP());
   root["mac_address"]      = WiFi.macAddress();
   root["hostname"]         = hostname;

   root["chip_id"]          = ESP.getChipId();
   root["flash_chip_id"]    = ESP.getFlashChipId();
   root["flash_chip_size"]  = ESP.getFlashChipSize();

   String json;
   root.prettyPrintTo(json);

   server->send(200, "application/json", json);

}

void handleMQTTPath() {

   StaticJsonBuffer<1000> jsonBuffer;
   JsonObject& root = jsonBuffer.createObject();
   root["response"] = "success";
   root["mqtt_status"] = mqtt_status;
   root["mqtt_active"] = client.connected();
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

void handleConfigPath() {

  if(!server->authenticate(www_username, www_password))
  return server->requestAuthentication();

  Serial.println("Auth");

  String hostname_str       = server->arg("hostname");
  String mqtt_status_str    = server->arg("mqtt_status");
  String mqtt_server_str    = server->arg("mqtt_server");
  String mqtt_port_str      = server->arg("mqtt_port");
  String mqtt_username_str  = server->arg("mqtt_username");
  String mqtt_password_str  = server->arg("mqtt_password");
  String home_name_str      = server->arg("home");
  String room_str           = server->arg("room");
  String device_name_str    = server->arg("device");
  String physwitch_str      = server->arg("physwitch");
  String alexa_status_str   = server->arg("alexa_status");
  String alexa_name_str     = server->arg("alexa_name");

  String submit_str         = server->arg("submit");
  String reset_str         = server->arg("reset");

  Serial.println("Loaded args");

  if ( submit_str == "true" ) {

    Serial.println(hostname_str);
    hostname_str.toCharArray(hostname,40);

    Serial.println(mqtt_server_str);
    mqtt_status_str.toCharArray(mqtt_status,5);

    Serial.println(mqtt_server_str);
    mqtt_server_str.toCharArray(mqtt_server,40);

    Serial.println(mqtt_port_str);
    mqtt_port_str.toCharArray(mqtt_port,6);

    Serial.println(mqtt_username_str);
    mqtt_username_str.toCharArray(mqtt_username,20);

    Serial.println(mqtt_password_str);
    mqtt_password_str.toCharArray(mqtt_password,40);

    Serial.println(home_name_str);
    home_name_str.toCharArray(home_name,40);

    Serial.println(room_str);
    room_str.toCharArray(room,40);

    Serial.println(device_name_str);
    device_name_str.toCharArray(device_name,40);

    Serial.println(physwitch_str);
    physwitch_str.toCharArray(physwitch,5);

    Serial.println(alexa_status_str);
    alexa_status_str.toCharArray(alexa_status,5);

    Serial.println(alexa_name_str);
    alexa_name_str.toCharArray(alexa_name,40);

    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["hostname"]           = hostname;
    json["mqtt_status"]        = mqtt_status;
    json["mqtt_server"]        = mqtt_server;
    json["mqtt_port"]          = mqtt_port;
    json["mqtt_username"]      = mqtt_username;
    json["mqtt_password"]      = mqtt_password;
    json["home"]               = home_name;
    json["room"]               = room;
    json["device"]             = device_name;
    json["physwitch"]          = physwitch;
    json["alexa_status"]       = alexa_status;
    json["alexa_name"]         = alexa_name;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {}

    //json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();

  }

  Serial.println("Saved");

  if (1) {

    Serial.println("Begin read");

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

            String hostname_local       = json["hostname"];
            String mqtt_status_local    = json["mqtt_status"];
            String mqtt_server_local    = json["mqtt_server"];
            String mqtt_port_local      = json["mqtt_port"];
            String mqtt_username_local  = json["mqtt_username"];
            String mqtt_password_local  = json["mqtt_password"];
            String home_name_local      = json["home"];
            String room_local           = json["room"];
            String device_name_local    = json["device"];
            String physwitch_local      = json["physwitch"];
            String alexa_status_local   = json["alexa_status"];
            String alexa_name_local     = json["alexa_name"];

            String mqtt_status_enabled;
            String mqtt_status_disabled;

            String physwitch_enabled;
            String physwitch_disabled;

            String alexa_status_enabled;
            String alexa_status_disabled;

            if ( mqtt_status_local == "1" ) {

              mqtt_status_enabled = "selected";

            } else {

              mqtt_status_disabled = "selected";

            }

            if ( physwitch_local == "1" ) {

              physwitch_enabled = "selected";

            } else {

              physwitch_disabled = "selected";

            }

            if ( alexa_status_local == "1" ) {

              alexa_status_enabled = "selected";

            } else {

              alexa_status_disabled = "selected";

            }

            Serial.println("Read done");
            Serial.println("Begin config page compose");

            config_page="";

            config_page+="	<html>	";
            config_page+="	<head>	";
            config_page+="	<link rel=\"stylesheet\" href=\"//netdna.bootstrapcdn.com/bootstrap/3.3.2/css/bootstrap.min.css\">	";
            config_page+="	<meta name=\"viewport\" content=\"initial-scale=1.0\">	";
            config_page+="	</head>	";
            config_page+="	<body>	";
            config_page+="	<div>	";
            config_page+="	<form class=\"form-horizontal\">	";
            config_page+="	<fieldset>	";
            config_page+="	<!-- Form Name -->	";
            config_page+="	<legend style=\"background-color: #d30000;\"><img src=\"https://images.swifitch.cz/swifitch_long.png\" style=\"width:40%;\"></legend>	";
            config_page+="	<div style=\"padding-left: 50px; padding-right: 50px; padding-top: 20px;\">	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"hostname\">Hostname</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"hostname\" name=\"hostname\" type=\"text\" placeholder=\"Hostname\" class=\"form-control input-md\" value=\""+hostname_local+"\">	";
            config_page+="	<span class=\"help-block\">Set hostname and MDNS name for this swifitch, you can use it to access this page using hostname.local/config.</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<hr>	";
            config_page+="	<!-- Select Basic -->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"mqtt_status\">MQTT</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<select id=\"mqtt_status\" name=\"mqtt_status\" class=\"form-control\">	";

            config_page+="	<option value=\"1\" "+mqtt_status_enabled+">Enable</option>	";
            config_page+="	<option value=\"0\" "+mqtt_status_disabled+">Disable</option>	";

            config_page+="	</select>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"mqtt_server\">MQTT Server</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"mqtt_server\" name=\"mqtt_server\" type=\"text\" placeholder=\"mqtt.swifitch.cz or IPv4\" class=\"form-control input-md\" value=\""+mqtt_server_local+"\">	";
            config_page+="	<span class=\"help-block\">Enter domain of public MQTT server or your local server IP.</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"mqtt_port\">MQTT Port</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"mqtt_port\" name=\"mqtt_port\" type=\"text\" placeholder=\"1883 is default MQTT port\" class=\"form-control input-md\" value=\""+mqtt_port_local+"\">	";
            config_page+="	<span class=\"help-block\">Enter port, default is 1883.</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"mqtt_username\">MQTT Username</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"mqtt_username\" name=\"mqtt_username\" type=\"text\" placeholder=\"username\" class=\"form-control input-md\" value=\""+mqtt_username_local+"\">	";
            config_page+="	<span class=\"help-block\">Enter your MQTT server username, by default there is usually none.</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Password input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"mqtt_password\">MQTT Password</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"mqtt_password\" name=\"mqtt_password\" type=\"password\" placeholder=\"password\" class=\"form-control input-md\" value=\""+mqtt_password_local+"\">	";
            config_page+="	<span class=\"help-block\">Enter your MQTT server password, by default there is usually none.</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"home_name\">Home</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"home\" name=\"home\" type=\"text\" placeholder=\"Home name\" class=\"form-control input-md\" value=\""+home_name_local+"\">	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"room\">Room</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"room\" name=\"room\" type=\"text\" placeholder=\"Room name\" class=\"form-control input-md\" value=\""+room_local+"\">	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"device\">Device name</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"device\" name=\"device\" type=\"text\" placeholder=\"Device name\" class=\"form-control input-md\" value=\""+device_name_local+"\">	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<hr>	";
            config_page+="	<!-- Select Basic -->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"physwitch\">Physical switch</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<select id=\"physwitch\" name=\"physwitch\" class=\"form-control\">	";

            config_page+="	<option value=\"0\" "+physwitch_disabled+">Disable</option>	";
            config_page+="	<option value=\"1\" "+physwitch_enabled+">Enable</option>	";

            config_page+="	</select>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<hr>	";
            /*
            config_page+="	<!-- Select Basic -->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"alexa_status\">FauxMO for Amazon Echo</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<select id=\"alexa_status\" name=\"alexa_status\" class=\"form-control\">	";

            config_page+="	<option value=\"0\" "+alexa_status_disabled+">Disable</option>	";
            config_page+="	<option value=\"1\" "+alexa_status_enabled+">Enable</option>	";

            config_page+="	</select>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Text input-->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"alexa_name\">Device name for Amazon Echo</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<input id=\"alexa_name\" name=\"alexa_name\" type=\"text\" placeholder=\"ex: kitchen light\" class=\"form-control input-md\" value=\""+alexa_name_local+"\">	";
            config_page+="	<span class=\"help-block\">This will be used when you tell \"Alexa, turn on kitchen light\".</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            */
            config_page+="	<!-- Button -->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"submit\"></label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<button id=\"submit\" value=\"true\" name=\"submit\" class=\"btn btn-success\">Save</button>	";
            config_page+="	<span class=\"help-block\">If device is unresponsive after clicking save, powercycle it.</span>	";
            config_page+="	</div>	";
            config_page+="	</div>	";
            config_page+="	<!-- Button -->	";
            config_page+="	<div class=\"form-group\">	";
            config_page+="	<label class=\"col-md-4 control-label\" for=\"reset\">Reset to factory settings</label>	";
            config_page+="	<div class=\"col-md-4\">	";
            config_page+="	<button id=\"reset\" value=\"true\" name=\"reset\" class=\"btn btn-danger\">Reset</button>	";
            config_page+="	</div>	";
            config_page+="	</div>	";

            config_page+="	</div>	";
            config_page+="	</fieldset>	";
            config_page+="	</form>	";
            config_page+="	</div>	";
            config_page+="	</body>	";
            config_page+="	</html>	";

          }
        }
      }
    }
  }

  Serial.println("Config page compose end");

  if ( submit_str == "true" ) {

    Serial.println("Submit true");

    server->sendHeader("Location", String("/config"), true);
    server->send ( 302, "text/plain", "");

    delay(2000);
    ESP.restart();

  }

  if ( reset_str == "true" ) {

    Serial.println("Reset true");
    server->sendHeader("Location", String("/config"), true);
    server->send ( 302, "text/plain", "");

    delay(1000);
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    delay(2000);
    ESP.restart();

  }

  server->send(200, "text/html", config_page);

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

void loop() {

  long now = millis();

  if ( strcmp(mqtt_status,"1") == 0 ) {
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
  }
  ArduinoOTA.handle();
  server->handleClient();

/*
if ( strcmp(alexa_status,"1") == 0 ) {
  fauxmo.handle();
}
*/

if ( strcmp(physwitch,"1") == 0 ) {
  physicalSwitch();
}

}
