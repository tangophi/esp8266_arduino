
Proof of concept of an ESP8266 module talking to Arduino through UART (both ways) using json strings. Also, shows how two separate ESP8266 modules can work together through MQTT.   It is still a WORK IN PROGRESS !!!!!

Hardware Setup:

1st board:
One ESP8266 (ESP-01) module connected to a ATMEGA1284P through UART.  Also, a 0.96 inch i2c OLED screen is connected to GPIO0 and GPIO2 pins.
ATMEGA1284 is connected to a 2.2 inch TFT LCD screen with SD card, MQ2 and MQ135 air quality sensors, a LDR (light sensor),  a PIR motion detector and an IR receiver.

2nd board:
One ESP8266 (ESP-01) module with GPIO0 connected to a relay controlling a light bulb.  GPIO2 is connected to a DHT22 sensor.


How it works:

ESP8266 on the first board is subcribed to several topics on devicehub and if it receives messages on any of them displays it on OLED screen and formats it as a json strings and sends it to the Arduino through the UART port.
Also, it has a loop() function which will check if there are incoming messages from the Arduino and if it receives a valid json string, it creates a json object and parses the string.  The callback functions of the json object will then publish the received messages to MQTT.

The Arduino displays local sensor data on the TFT screen such as light sensor, MQ2 and MQ3 sensors.  It also displays a image (from SD card) of a man in motion if it detects motion.  
It also waits for power control command on the IR receiver and if it receives a power command, will send a message to the ESP8266 module.  The ESP8266 module will inturn publish a message which will then be received by the ESp8266 module on the second board to turn on/off the relay.

The Arduino also displays temperature and humidity data sent by the ESP8266 module on the second board to MQTT.  It also displays the image of a unlit bulb or a lit bulb depending on the state of the relay on the second board.


The ESP8266 module on the second board periodically sends temperature and humidity data to MQTT and turns on/off relay if it receives power control messages.

Code:
Arduino_ESP8266_Json :         Arduino sketch for ATMEGA1284P on first board
esp_arduino_mqtt_multisensor : ESP8266 code for ESP8266 (ESP-01) module on first board

esp_mqtt_devicehub :           ESP8266 code for ESP8266 (ESP-01) module on second board

Future plans:
Replace TFT LCD with a touch screen.  And make a combo burglar/security alarm.
Add speed control of AC ceiling fan.
Add more ESP8266 (ESP-01 or ESP-12) standalone or Arduino combo boards.
LPG and smoke alarm for kitchen.
Add WS2812 strips to Arduino or ESP8266 directly.
Make enclosures to install these properly.
Make circuits flexible enough so that existing switches (replaced by two way switches) on wall outlets can still be used.
Modify AC circuit to detect if an appliance (bulb or fan) is actually switched on or not.  Both the normally open and normally closed ports of the relays will be used in tandem with two-way AC switches on wall outlet.
Configure local MQTT server.
Display current time on LCD screen.  time sent by local raspberry pi periodically.
Connect everything to openhab.
Connect to VPN so that setup can be accessed from outside home network.
Secure everything.
Lots of things to do...