#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Constants
const char* autoconf_ssid = "ESP8266_SWIFITCH";     //AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char* autoconf_pwd  = "PASSWORD";             //AP password so noone else can connect to the ESP in case your router fails
const char* mqtt_server   = "test.mosquitto.org";   //MQTT Server IP, your home MQTT server eg Mosquitto on RPi, or some public MQTT
const int mqtt_port       = 1883;                   //MQTT Server PORT, default is 1883 but can be anything.

// MQTT Constants
const char* mqtt_devicestatus_set_topic    = "home/room/swifitch/devicestatus"; // Change device name, but you can completely change the topics to suit your needs
const char* mqtt_relayone_set_topic        = "home/room/swifitch/status";
const char* mqtt_relayone_get_topic        = "home/room/swifitch";
const char* mqtt_pingall_get_topic         = "home/pingall";
const char* mqtt_pingallresponse_set_topic = "home/pingallresponse";
const char* mqtt_pingall_response_text     = "{\"swifitch\":\"connected\"}";    // Set unique name for each Swifitch you deploy

// Global
byte relayone_state;
long lastReconnectAttempt = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_ota() {

  // Set OTA Password, and change it in platformio.ini
  ArduinoOTA.setPassword("ESP8266_PASSWORD");
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
    if (client.connect(clientId.c_str(),mqtt_devicestatus_set_topic,0,false,"disconnected")) {

      // Once connected, publish an announcement...
      client.publish(mqtt_devicestatus_set_topic, "connected");
      // ... and resubscribe
      client.subscribe(mqtt_relayone_get_topic);
      client.subscribe(mqtt_pingall_get_topic);

    }

    return client.connected();

}

void callback(char* topic, byte* payload, unsigned int length) {

    char c_payload[length];
    memcpy(c_payload, payload, length);
    c_payload[length] = '\0';

    String s_topic = String(topic);         // Topic
    String s_payload = String(c_payload);   // Message content

  // Handling incoming MQTT messages

    if ( s_topic == mqtt_relayone_get_topic ) {

      if (s_payload == "1") {

        if (relayone_state != 1) {

          // Turn relay ON, set HIGH

          digitalWrite(D1,HIGH);
          client.publish(mqtt_relayone_set_topic, "1");
          relayone_state = 1;
          blink();

        }

      } else if (s_payload == "0") {

        if (relayone_state != 0) {

          // Turn relay OFF, set LOW

          digitalWrite(D1,LOW);
          client.publish(mqtt_relayone_set_topic, "0");
          relayone_state = 0;
          blink();

        }

      }

    } else if ( s_topic == mqtt_pingall_get_topic ) {

      client.publish(mqtt_pingallresponse_set_topic, mqtt_pingall_response_text);

    }


}

void blink() {

  //Blink on received message
  digitalWrite(D6, HIGH);
  delay(25);
  digitalWrite(D6, LOW);

}

void setup() {

  //Relay setup
  pinMode(D1,OUTPUT);       //Initialize relay GPIO05 > NODEMCU pin D1
  digitalWrite(D1, HIGH);   //Set relay ON by default (so when you flip the switch it will be on) GPIO05 > NODEMCU pin D1
  pinMode(D6, OUTPUT);      //Initialize the SWIFITCH built-in LED (green) - GPIO12 > NODEMCU pin D6
  Serial.begin(115200);
  WiFiManager wifiManager;
  wifiManager.autoConnect(autoconf_ssid,autoconf_pwd);
  setup_ota();
  MDNS.begin("esp-swifitch");
  client.setServer(mqtt_server, mqtt_port);
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