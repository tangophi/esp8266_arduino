Instructables page:
http://www.instructables.com/id/An-inexpensive-IoT-enabler-using-ESP8266/

ESP8266 IoT enabler
	- Sends temperature/humidity readings from a DHT11 sensor
	  to a MQTT broker and a Thingspeak channel
	- Listens for MQTT messages and turns on or off a relay
	  connected to ESP8266.
	- Checks a Thingspeak channel periodically and turns on/off
	  a relay if there is an update.
	  
Connections:
    - GPIO0   to  relay
    - GPIO2   to  data pin of DHT11	  
    
Build:
* Configure ESP8266 build environment:
  - http://signusx.com/esp8266-windows-compilation-tutorial/
* Update user_config.h and change MQTT host and login details
  as well as WiFi login details.
  	#define MQTT_HOST	"your cloudmqtt instance name" 
	#define MQTT_PORT	your cloudmqtt instance port

	#define MQTT_USER	"your cloudmqtt instance username"
	#define MQTT_PASS	"your cloudmqtt instance password"

	#define STA_SSID        "your WiFi SSID"
	#define STA_PASS        "your WiFi password"    
* Update following two global variables in user_main.c to your
  values:
	char YOUR_THINGSPEAK_API_KEY[]=  "xxxxxxxxxxxxxxxxx";
	char YOUR_THINGSPEAK_CHANNEL[]= "xxxxxxxxx";

* Connect GPIO0 to ground by changing the jumper position.
* Clean, build and flash the firmware to the ESP8266 module.
* Remove jumper and power cycle the ESP8266 module.
* Connect GPIO0 to relay input by inserting the jumper.
* Thats it.  You can now view the readings and control the relay.
    
Operation:
	- Connects to a free online MQTT broker (cloudmqtt.com)
	- Subscribes to three topics - /esp8266/temperature, /esp8266/humidity and
	  /esp8266/relay
	- Starts a timer that calls a function to periodically:
	  	- Send temperature/humidity readings to the MQTT broker
	  	- Send temperature and humidity readings to the following Thingspeak
	  	  Channel:
	  	   	  http://thingspeak.com/channels/21370
	  	- Read the last value of field1 in the Thingspeak channel that
	  	  corresponds to the relay state and if there is a change in the value
	  	  then send appropriate MQTT messages to turn on/off relay.
	  	  
	- Defines a call back routine when MQTT messages are received
		- If a message is received on the /esp8266/relay topic, it will then
		  turn on or off relay by setting GPIO0 high or low.
		- Sends relay state to field3 of Thingspeak channel.
		  
Display:
	- The temperature/humidity readings is available at https://thingspeak.com/channels/21370
	- The relay state is also available in the Thingspeak channel.  A value of -1.0 means
	  the relay is off and 1.0 means it is on.
	- These readings can also be viewed through any MQTT client connected to the broker.
	  
Control:
	- The relay can be turned on/off using any MQTT client connected to the same
	  MQTT broker by sending a message, "on" of "off" to /esp8266/relay topic.
	
	- The relay can also be turned on/off by updating field1 of the Thingspeak
	  channel using the following URLs:

	  on: https://api.thingspeak.com/update?key=YF2DC4HXFSUQUIEE&field1=1.0
	  off: https://api.thingspeak.com/update?key=YF2DC4HXFSUQUIEE&field1=-1.0
	  
	  Copy/paste the URL in a broswer.  If the response is a non zero integer, it means
	  the field was successfully updated.  If a zero is received back, try the URL again.
  	
References:
	This project is based mainly on the following two git repos:
		https://github.com/tuanpmt/esp_mqtt
		https://github.com/Caerbannog/esphttpclient
	