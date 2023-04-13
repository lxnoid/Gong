#include <Arduino.h>
#include <ESP8266Wifi.h>
#include "LittleFS.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

char wifiSsid[1024]      = "";  
char wifiPassword[1024]  = "";
char mqttServer[1024]    = ""; 
int  mqttPort            = 0; 
char mqttUser[1024]      = "";
char mqttPassword[1024]  = "";
char mqttClientId[1024]  = "";
char printbuffer[1024]      = "";  

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// the setup routine runs once when you press reset:
void setup() {                
  Serial.begin(115200);
  //read config
  if (!LittleFS.begin()) {
    Serial.println("LITTLEFS Mount Failed");
  }
  File cfile = LittleFS.open("/config.json", "r");
  if (cfile) {
    String config_data;
    while (cfile.available()) {
      config_data += char(cfile.read());
    }
    Serial.print(config_data);   
    DynamicJsonDocument cjson(config_data.length());
    deserializeJson(cjson, config_data);

    const char* c_wifiSsid     = cjson["wifiSsid"];  
    const char* c_wifiPassword = cjson["wifiPassword"];
    const char* c_mqttServer   = cjson["mqttServer"]; 
    const char* c_mqttPort     = cjson["mqttPort"]; 
    const char* c_mqttUser     = cjson["mqttUser"]; 
    const char* c_mqttPassword = cjson["mqttPassword"];
    const char* c_mqttClientId = cjson["mqttClientID"];

    Serial.print(c_wifiSsid);

    if (strlen(c_wifiSsid)) { 
      sprintf(wifiSsid, "%s", c_wifiSsid); 
    }
    if (strlen(c_wifiPassword)) { 
      sprintf(wifiPassword, "%s", c_wifiPassword); 
      }
    if (strlen(c_mqttServer)) { 
      sprintf(mqttServer, "%s", c_mqttServer); 
    }
    if (strlen(c_mqttPort)) { 
      mqttPort = atoi(c_mqttPort);
    }
    if (strlen(c_mqttUser)) { 
      sprintf(mqttUser, "%s", c_mqttUser);
    }
    if (strlen(c_mqttPassword)) { 
      sprintf(mqttPassword, "%s", c_mqttPassword);
    }
    if (strlen(c_mqttClientId)) {
      sprintf(mqttClientId, "%s", c_mqttClientId);
    }

  } else {
    Serial.print("Config File missing.");
    for (;;)
      delay(1);
  }
  delay(100);
  cfile.close();

  //DIO
  // initialize the digital pin as an output.
  pinMode(A0, INPUT); // ADC on A0

  // Wifi ------------------------------------------------------------------------------
  Serial.begin(115200);
  delay(100);
  Serial.println("Connecting to WiFi");

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setOutputPower(10.0);
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.persistent(false);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  
  WiFi.begin(wifiSsid, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }  
  Serial.println("Connected to WiFi");
  
  delay(100);
  
  // MQTT ------------------------------------------------------------------------------
  mqtt_client.setServer(mqttServer, mqttPort);
  
  while (!mqtt_client.connected()) {
    Serial.println("...Connecting to MQTT");
    if (mqtt_client.connect(mqttClientId, mqttUser, mqttPassword )) {
      Serial.println("Connected to MQTT");
    } else {
      Serial.print("Failed connecting MQTT with state: ");
      Serial.print(mqtt_client.state());
      delay(2000);
    }
  }

  mqtt_client.publish("misc/gong/cmd", "Hello World.");
  Serial.println("-- End of Setup --");
}

boolean mqtt_reconnect() {
  if (mqtt_client.connect(mqttClientId, mqttUser, mqttPassword )) {
    mqtt_client.publish("misc/gong/cmd","Hello World, again", false);
  }
  return mqtt_client.connected();
}

int adc_read_prev = 0, adc_read_act = 0;
int adc_index = 0;

bool P1_on = false;
bool trigger_gong = false;
bool first_Time = false;

unsigned long act_time = 0;
unsigned long prev_time = 0;
unsigned long next_time = 0;

// the loop routine runs over and over again forever:
void loop() {

   if (!first_Time) 
   {
    first_Time = true;
    delay(100);
    adc_read_act = analogRead(A0);
    adc_read_prev = adc_read_act;
   } 

  // put your main code here, to run repeatedly:
  mqtt_client.loop();

  //check if MQTT and Wifi connected (wifi is set to reconnect on auto)
  if (!mqtt_client.connected()) {
    mqtt_reconnect();
    delay(200);
  }

  //get time passed since boot in ms
  act_time = millis();

  //check for 0.22 V change on ADC each 200ms
  if ((act_time - prev_time) > 200)
  {
    adc_read_act = analogRead(A0);
    snprintf(printbuffer, sizeof(printbuffer), "ADC: %d | time: %ld | next: %ld | prev: %ld | diff: %ld", adc_read_act, act_time, next_time, prev_time, (act_time - next_time));
    Serial.println(printbuffer);

    // 3.3V equals 1024 units, therefore 45 units is roughly the value change we're looking for.
    if ((((long)act_time - (long)next_time) > 0) && !trigger_gong && ((adc_read_act - adc_read_prev) > 10))
    {
      trigger_gong = true;
      next_time = act_time + 125000; //suppress impact of flashing of P2_LED for 125 sec.
    }
    
    //store past time timer and adc value
    prev_time = act_time;
    adc_read_prev = adc_read_act;
  }

  if (trigger_gong) 
  {
    mqtt_client.publish("misc/gong/cmd","Gong!", false);
    Serial.println("Gong!");
    trigger_gong = false;
  } 
}
