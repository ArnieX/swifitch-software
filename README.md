![](https://github.com/ArnieX/swifitch/blob/master/Images/swifitch_looong_header.png?raw=true)

Welcome to SWIFITCH SW repository. If you didn't come here through HW repository, and want to know more about SWIFITCH go have a [look](http://swifitch.cz).

<img src="https://github.com/ArnieX/swifitch/blob/master/Images/3D_Vector_Swifitch2.png?raw=true" width="500">

## Dependencies
- Some MQTT server (If you have Raspberry Pi use Mosquitto)
- Optional is [Homebridge](https://github.com/nfarina/homebridge) with [MQTT Plugin](https://github.com/cflurin/homebridge-mqtt) to control the relay from iDevices
- Optional that will make your life easier with IoT is Node-RED, plus you can get decent dashboard with Node-RED Dashboard plugin
- [PlatformIO](https://github.com/platformio/platformio) best Arduino IDE available, hacked from ATOM text editor

- [WiFi Manager](https://github.com/tzapu/WiFiManager) this library is included and quite a bit altered for Swifitch graphics etc.
- [PubSubClient (MQTT)](https://github.com/knolleary/pubsubclient)
- ESP8266 WiFi library

Install using PlatformIO Library Manager

```
pio lib -g install PubSubClient
pio lib -g install ESP8266wifi
pio lib -g install ESPAsyncTCP
```

## Getting started

- Flash the software to your freshly soldered Swifitch
- Connect to the SWIFITCH_XXXXXXX WiFi
- Click Configure Swifitch
- Select your WiFi or enter SSID manually
- Enter settings for your MQTT server - if you do not have one yet, use our public mqtt.swifitch.cz (it isn't guaranteed to be always online, but it should be)
- Enter you home name, room and device name for MQTT control, see below, omit whitespace between words

## Flashing
### How to flash with Arduino UNO

- Remove the chip
- Connect your Arduino with Swifitch with dupont wires according to this mapping:

| Swifitch     | Arduino |
|:------------ |:------- |
| RX           | RX      |
| TX           | TX      |
| 3v3          | 3v3     |
| D3           | GND     |
| GND          | GND     |

Arduino RESET -> Arduino GND

- Flash with PlatformIO

## Configuration screenshots

![](https://github.com/ArnieX/swifitch/blob/master/Images/Software/swifitch_sw_config_root.png?raw=true)
![](https://github.com/ArnieX/swifitch/blob/master/Images/Software/swifitch_sw_config_1.png?raw=true)
![](https://github.com/ArnieX/swifitch/blob/master/Images/Software/swifitch_sw_config_2.png?raw=true)

## Usage

Send command through your MQTT server as such:

|TOPIC|DESCRIPTION|
|---|---|
|home/room/swifitch|Send 1/0 to turn ON/OFF|
|home/room/swifitch/getstatus|Send anything to get current relay pin status (digitalRead) on topic home/room/swifitch/status|
|home/pingall|Send whatever and you get response at topic home/pingallresponse|

Receive back from your device:

|TOPIC|DESCRIPTION|
|---|---|
|home/room/swifitch/devicestatus|Will contain device status eg. connected|
|home/pingallresponse|This will contain status after you send pingall request and all devices should respond|
|home/room/swifitch/status|This listens for status change to set correct status eg. in Homebridge|

## Using with Amazon Echo

Swifitch will be automatically discovered by Amazon Echo since it fakes being a WeMo device and it doesn't require any server/homebridge setup.

## HTTP API

|URL|DESCRIPTION|
|---|---|
|/|Check webserver status|
|/toggle|Change relay state, return JSON response|
|/info|JSON response with relay status and other device info|
|/mqtt|JSON response with MQTT status and settings|
|/reset|Finally reset your Swifitch for returning into WiFi manager, you may need to powercycle after reset.|

## Credits

Thanks goes to [Alexander Luberg](https://github.com/LubergAlexander) for pointing out that there is no need for external pulldown or pullup resistor for analog switch.
