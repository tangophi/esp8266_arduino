/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "user_json.h"

// *** CHANGE CONFIG HERE ***
LOCAL char YOUR_DEVICEHUB_API_KEY[] = "dd4d17b8-7ec4-4f96-a0b3-50a06426a041";
LOCAL char YOUR_DEVICEHUB_PROJECT[] = "738";

LOCAL int SensorIdTemperature    = 1311;
LOCAL int SensorIdHumidity       = 1312;
LOCAL int SensorIdMotionDetector = 1307;
LOCAL int SensorIdLightSensor    = 1308;
LOCAL int SensorIdMQ2            = 1309;
LOCAL int SensorIdMQ135          = 1310;

LOCAL int ActuatorIdLightBulb    = 417;
LOCAL int ActuatorIdBuzzer       = 456;

LOCAL int intMotionDetector  = 0;
LOCAL int intLightSensor     = 0;
LOCAL int intMQ2             = 0;
LOCAL int intMQ135           = 0;
LOCAL char strTemperature[10] = "";
LOCAL char strHumidity[10]    = "";
LOCAL int intLightBulb = 0;
LOCAL int intBuzzer    = 0;

LOCAL bool bTemperatureTopicSubscribed = false;
LOCAL bool bHumidityTopicSubscribed    = false;
LOCAL bool bLightBulbTopicSubscribed   = false;
LOCAL bool bBuzzerTopoicSubscribed     = false;

MQTT_Client mqttClient;

static volatile bool OLED;
#define user_procTaskPrio        1
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void loop(os_event_t *events);

#define DELAY 60000 /* milliseconds */
LOCAL os_timer_t timer;


/******************************************************************************
 * FunctionName : data_get
 * Description  : Process the data received through MQTT and send it to Arduino
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
data_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

    if (os_strncmp(path, "Temperature", 12) == 0)
    {
        jsontree_write_string(js_ctx, strTemperature);
    }
    else if (os_strncmp(path, "Humidity", 8) == 0)
    {
        jsontree_write_string(js_ctx, strHumidity);
    }
    else if (os_strncmp(path, "LightBulb", 9) == 0)
    {
        jsontree_write_int(js_ctx, intLightBulb);
    }
    else if (os_strncmp(path, "Buzzer", 6) == 0)
    {
        jsontree_write_int(js_ctx, intBuzzer);
    }

    return 0;
}


/******************************************************************************
 * FunctionName : data_set
 * Description  : Receive environmental data from Arduino
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
data_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
    char str_url[100],str_buf[64];

    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            char buffer[64];
            os_bzero(buffer, 64);

            if (jsonparse_strcmp_value(parser, "MotionDetector") == 0) {
            	jsonparse_next(parser);
            	jsonparse_next(parser);
                intMotionDetector = jsonparse_get_value_as_int(parser);
                INFO("MotionDetector:%d:\n", intMotionDetector);
            } else if (jsonparse_strcmp_value(parser, "LightSensor") == 0) {
            	jsonparse_next(parser);
            	jsonparse_next(parser);
            	intLightSensor = jsonparse_get_value_as_int(parser);
            	INFO("LightSensor:%d:\n", intLightSensor);
            } else if (jsonparse_strcmp_value(parser, "MQ2") == 0) {
            	jsonparse_next(parser);
            	jsonparse_next(parser);
            	intMQ2 = jsonparse_get_value_as_int(parser);
            	INFO("MQ2:%d:\n", intMQ2);
            } else if (jsonparse_strcmp_value(parser, "MQ135") == 0) {
            	jsonparse_next(parser);
            	jsonparse_next(parser);
            	intMQ135 = jsonparse_get_value_as_int(parser);
            	INFO("MQ135:%d:\n", intMQ135);
            } else if (jsonparse_strcmp_value(parser, "LightBulb") == 0) {
            	jsonparse_next(parser);
            	jsonparse_next(parser);
            	intLightBulb = jsonparse_get_value_as_int(parser);
            	INFO("LightBulb:%d:\n", intLightBulb);

            	// Send command to devicehub immediately.
            	os_bzero(str_url, sizeof(str_url));
            	os_bzero(str_buf, sizeof(str_buf));
            	os_sprintf(str_url,"/%s/%s/actuator/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,ActuatorIdLightBulb);
            	os_sprintf(str_buf,"%d", intLightBulb);

            	OLED_Print(0, 4, "Remote MQTT:", 1);
            	OLED_Print(10, 4, "           ", 1);
            	OLED_Print(13, 4, "LightBulb", 1);
            	MQTT_Publish(&mqttClient, str_url, str_buf, os_strlen(str_buf), 0, 0);
            }
        }
    }

    return 0;
}

LOCAL struct jsontree_callback data_callback =
    JSONTREE_CALLBACK(data_get, data_set);

JSONTREE_OBJECT(get_data_tree,
                JSONTREE_PAIR("Temperature", &data_callback),
                JSONTREE_PAIR("Humidity", &data_callback),
                JSONTREE_PAIR("LightBulb", &data_callback),
                JSONTREE_PAIR("Buzzer", &data_callback));

JSONTREE_OBJECT(data_get_tree,
                JSONTREE_PAIR("EnvData", &get_data_tree));

JSONTREE_OBJECT(get_temperature_data_tree,
                JSONTREE_PAIR("Temperature", &data_callback));
JSONTREE_OBJECT(get_humidity_data_tree,
                JSONTREE_PAIR("Humidity", &data_callback));
JSONTREE_OBJECT(get_lightbulb_data_tree,
                JSONTREE_PAIR("LightBulb", &data_callback));
JSONTREE_OBJECT(get_buzzer_data_tree,
                JSONTREE_PAIR("Buzzer", &data_callback));

JSONTREE_OBJECT(temperature_data_get_tree,
                JSONTREE_PAIR("EnvData", &get_temperature_data_tree));
JSONTREE_OBJECT(humidity_data_get_tree,
                JSONTREE_PAIR("EnvData", &get_humidity_data_tree));
JSONTREE_OBJECT(lightbulb_data_get_tree,
                JSONTREE_PAIR("EnvData", &get_lightbulb_data_tree));
JSONTREE_OBJECT(buzzer_data_get_tree,
                JSONTREE_PAIR("EnvData", &get_buzzer_data_tree));




JSONTREE_OBJECT(set_data_tree,
                JSONTREE_PAIR("MotionDetector", &data_callback),
                JSONTREE_PAIR("LightSensor", &data_callback),
                JSONTREE_PAIR("MQ2", &data_callback),
                JSONTREE_PAIR("MQ135", &data_callback),
                JSONTREE_PAIR("LightBulb", &data_callback));

JSONTREE_OBJECT(data_set_tree,
                JSONTREE_PAIR("EnvData", &set_data_tree));




LOCAL void ICACHE_FLASH_ATTR timer_callback(void *arg)
{
	char str_url[100];
	char str_temp[64];

	OLED_CLS();
	OLED_Print(0, 4, "Txing MQTT:", 1);

	os_bzero(str_url, sizeof(str_url));
	os_bzero(str_temp, sizeof(str_temp));
	os_sprintf(str_url,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdMotionDetector);
//	INFO("MotionDetector: %d\n", intMotionDetector);
	os_sprintf(str_temp,"%d", intMotionDetector);
	OLED_Print(10, 4, "           ", 1);
	OLED_Print(10, 4, "Temperature", 1);
	MQTT_Publish(&mqttClient, str_url, str_temp, os_strlen(str_temp), 0, 0);

	os_bzero(str_url, sizeof(str_url));
	os_bzero(str_temp, sizeof(str_temp));
	os_sprintf(str_url,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdLightSensor);
//	INFO("LightSensor: %d\n", intLightSensor);
	os_sprintf(str_temp,"%d", intLightSensor);
	OLED_Print(10, 4, "           ", 1);
	OLED_Print(10, 4, "LightSensor", 1);
	MQTT_Publish(&mqttClient, str_url, str_temp, os_strlen(str_temp), 0, 0);

	os_bzero(str_url, sizeof(str_url));
	os_bzero(str_temp, sizeof(str_temp));
	os_sprintf(str_url,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdMQ2);
//	INFO("MQ2: %d\n", intMQ2);
	os_sprintf(str_temp,"%d", intMQ2);
	OLED_Print(10, 4, "           ", 1);
	OLED_Print(10, 4, "MQ2", 1);
	MQTT_Publish(&mqttClient, str_url, str_temp, os_strlen(str_temp), 0, 0);

	os_bzero(str_url, sizeof(str_url));
	os_bzero(str_temp, sizeof(str_temp));
	os_sprintf(str_url,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdMQ135);
//	INFO("MQ135: %d\n", intMQ135);
	os_sprintf(str_temp,"%d", intMQ135);
	OLED_Print(10, 4, "           ", 1);
	OLED_Print(10, 4, "MQ135", 1);
    MQTT_Publish(&mqttClient, str_url, str_temp, os_strlen(str_temp), 0, 0);
}

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args)
{
	char str_url[100];
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");

	os_bzero(str_url, sizeof(str_url));
	os_sprintf(str_url,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdTemperature);
	MQTT_Subscribe(client, str_url, 0);

	os_bzero(str_url, sizeof(str_url));
	os_sprintf(str_url,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdHumidity);
	MQTT_Subscribe(client, str_url, 0);

	os_bzero(str_url, sizeof(str_url));
	os_sprintf(str_url,"/%s/%s/actuator/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,ActuatorIdLightBulb);
	MQTT_Subscribe(client, str_url, 0);

	os_bzero(str_url, sizeof(str_url));
	os_sprintf(str_url,"/%s/%s/actuator/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,ActuatorIdBuzzer);
	MQTT_Subscribe(client, str_url, 0);
}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *pbuf = (char *)os_zalloc(jsonSize);
	char str_buf[100];
	char *topicBuf = (char*)os_zalloc(topic_len+1);
	char *dataBuf  = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;
	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Received on topic: %s, data: %s \r\n", topicBuf, dataBuf);
	os_delay_us(500);
	OLED_Print(0, 6, "Rxd MQTT:", 1);
	OLED_Print(9, 6, "             ", 1);
	OLED_Print(0, 7, "             ", 1);

	os_bzero(str_buf, sizeof(str_buf));
	os_sprintf(str_buf,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdTemperature);
	if (os_strcmp(topicBuf,str_buf) == 0)
	{
		os_memcpy(strTemperature, dataBuf, data_len);
		OLED_Print(9, 6, "Temperature", 1);
		OLED_Print(0, 7, strTemperature, 1);
        json_ws_send((struct jsontree_value *)&temperature_data_get_tree, "EnvData", pbuf);
	}

	os_bzero(str_buf, sizeof(str_buf));
	os_sprintf(str_buf,"/%s/%s/sensor/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,SensorIdHumidity);
	if (os_strcmp(topicBuf,str_buf) == 0)
	{
		os_memcpy(strHumidity, dataBuf, data_len);
		OLED_Print(9, 6, "Humidity", 1);
		OLED_Print(0, 7, strHumidity, 1);
        json_ws_send((struct jsontree_value *)&humidity_data_get_tree, "EnvData", pbuf);
	}

	os_bzero(str_buf, sizeof(str_buf));
	os_sprintf(str_buf,"/%s/%s/actuator/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,ActuatorIdLightBulb);
	if (os_strcmp(topicBuf,str_buf) == 0)
	{
		intLightBulb = atoi(dataBuf);
		OLED_Print(9, 6, "LightBulb", 1);
		os_bzero(str_buf, sizeof(str_buf));
		os_sprintf(str_buf,"%d",intLightBulb);
		OLED_Print(0, 7, str_buf, 1);
        json_ws_send((struct jsontree_value *)&lightbulb_data_get_tree, "EnvData", pbuf);
	}

	os_bzero(str_buf, sizeof(str_buf));
	os_sprintf(str_buf,"/%s/%s/actuator/%d",YOUR_DEVICEHUB_API_KEY,YOUR_DEVICEHUB_PROJECT,ActuatorIdBuzzer);
	if (os_strcmp(topicBuf,str_buf) == 0)
	{
		intBuzzer = atoi(dataBuf);
		OLED_Print(9, 6, "Buzzer", 1);
		os_bzero(str_buf, sizeof(str_buf));
		os_sprintf(str_buf,"%d",intBuzzer);
		OLED_Print(0, 7, str_buf, 1);
        json_ws_send((struct jsontree_value *)&buzzer_data_get_tree, "EnvData", pbuf);
	}

	// Send to Arduino as Json string
	INFO("%s",pbuf);
	os_delay_us(500);

	os_free(topicBuf);
	os_free(dataBuf);
}

//Main code function
static void ICACHE_FLASH_ATTR loop(os_event_t *events)
{
	char str[256];
	int position = 0;
	int opening_brace = 0;

	int c = uart0_rx_one_char();

	if (c != -1)
	{
		if (c == 123) // {
		{
			os_bzero(str, sizeof(str));
			OLED_Print(0, 0, str, 1);
			struct jsontree_context js;
			str[position] = (char)c;
			position++;
			opening_brace++;

			while(c = uart0_rx_one_char())
			{
				if ((c >= 32) && (c <= 126))
				{
					str[position++] = (char)c;
				}

				if (c == 125)  // }
				{
					opening_brace--;
				}

				if (opening_brace == 0 )
				{
					break;
				}
			}

			str[position++] = 0;
//		    INFO("String received=%s:position=%d\n",str,position);
//		    OLED_CLS();
		    os_delay_us(200000);
			OLED_Print(0, 0, str, 1);
            jsontree_setup(&js, (struct jsontree_value *)&data_set_tree, json_putchar);
            json_parse(&js, str);

   		}
	}
}

#if 0
{"EnvData":{"MotionDetector":1,"LightSensor":1,"MQ2":100,"MQ135":250}}

{"EnvData":{"MotionDetector":0,"LightSensor":111,"MQ2":45,"MQ135":35}}
{"EnvData":{"MotionDetector":1,"LightSensor":1,"MQ2":2,"MQ135":3}}

#endif

void user_init(void)
{
//	uart_init(BIT_RATE_57600, BIT_RATE_57600);
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	gpio_init();
    // check GPIO setting (for config mode selection)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);

	i2c_init();
  	OLED = OLED_Init();
	OLED_Print(2, 0, "ESP8266 MQTT OLED", 1);
	os_delay_us(1000000);

	CFG_Load();

	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	//MQTT_InitConnection(&mqttClient, "192.168.11.122", 1880, 0);

	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	//MQTT_InitClient(&mqttClient, "client_id", "user", "pass", 120, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	//Start os task
	system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
	system_os_post(user_procTaskPrio, 0, 0 );

	// Start a timer that calls timer_callback every DELAY milliseconds.
	os_timer_disarm(&timer);
	os_timer_setfn(&timer, (os_timer_func_t *)timer_callback, (void *)0);
	os_timer_arm(&timer, DELAY, 1);

	INFO("\r\nSystem started ...\r\n");
}
