#include "SSD1306Wire.h"
#include "SSD1306Brzo.h"
#include "SH1106Brzo.h"
#include "SdsDustSensor.h"
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "Wire.h"
#include "Adafruit_SHT31.h"

#define SHT31_ADDRESS   0x44

const char ssid[] = "-";
const char pass[] = "-";
const char broker[] = "-.-.-";
const int port = 1883;
const char clientID[]= "-";
const char mqttUser[] = "-";
const char mqttPass[] = "-";
const int rxPin = D5;
const int txPin = D6;
const int btnLeft = D3;
const int btnRight = D7;
const char topic0[] = "home/air/hearbeat";
const char topic1[] = "home/air/pm10";
const char topic2[] = "home/air/pm25";
const char topic3[] = "home/air/temp";
const char topic4[] = "home/air/humid";
const int interval = 10 * 100 * 60; // 10 minutes

SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);
Adafruit_SHT31 sht31 = Adafruit_SHT31();
SdsDustSensor sds(rxPin, txPin);
WiFiClient wifi;
MQTTClient mqtt;
long i = 0;
float pm1 = 0.0;
float pm2 = 0.0;
float temp = 0.0;
float humid = 0.0;


void connect() {
  Serial.print("\nConnecting wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  while (!mqtt.connect(clientID, mqttUser, mqttPass, false)) {
    Serial.print(".");
    delay(1000);
  }
}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
}

void drawText(int x, int y, String s) {
  display.drawStringMaxWidth(x, y, 128, s);
}

void splash() {
  display.clear();
  drawText(0, 0, "Smogomiarka");
  display.setFont(ArialMT_Plain_10);
  drawText(0, 20,"by SP5DRS");
  display.display();
}

void showIP() {
  display.clear();
  drawText(0, 0, "WIFI address:");
  display.setFont(ArialMT_Plain_16);
  drawText(0, 17, WiFi.localIP().toString());
  display.display();
}

void setup() {
  i = 0;
  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println();

  display.init();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  sds.begin();
  sds.setQueryReportingMode();

  Wire.begin();
  Wire.setClock(100000);

  if (! sht31.begin(SHT31_ADDRESS)) {
    Serial.println("Couldn't find SHT31");
    display.clear();
    display.setFont(ArialMT_Plain_16);
    drawText(0, 0, "Cannot find Temp Sensor!");
    display.display();
    while (1) delay(1);
  }

  splash();

  WiFi.begin(ssid, pass);
  mqtt.begin(broker, wifi);
  mqtt.onMessage(messageReceived);
  connect();

  showIP();
  delay(3000);

  if (!mqtt.connected()) connect();
  mqtt.publish(topic0, "esp01-alive");
}

void measInProgress() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  drawText(0, 10, "Pomiar...");
  display.display();
}

void mqttInProgress() {
  display.clear();
  display.setFont(ArialMT_Plain_16);
  drawText(0, 10, "Wysylanie...");
  display.display();
}


void displayResult() {
  Serial.println("PM25/10: " + String(pm1) + " / " + String(pm2));
  Serial.println("T/H:" + String(temp) + " / " + String(humid));
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  drawText(0, 0, "PM 2.5"); drawText(65, 0, String(pm2));
  drawText(0,10, "PM 10");  drawText(65,10, String(pm1));
  drawText(0,20, "T/H");    drawText(65,20, String(temp) + " / " + String(humid));
  display.display();
}

void mqttPublish(){
    mqttInProgress();
    if (!mqtt.connected()) connect();
    mqtt.loop();
    Serial.println("Publishing MQTT message...");
    mqtt.publish(topic1, String(pm1));
    mqtt.publish(topic2, String(pm2));
    mqtt.publish(topic3, String(temp));
    mqtt.publish(topic4, String(humid));
}

void makeMeas() {
  bool success = false;
  sds.wakeup();
  measInProgress();
  delay(30000); // warm up
  PmResult pm = sds.queryPm();
  temp = sht31.readTemperature();
  humid = sht31.readHumidity();
  pm1 = pm.pm10;
  pm2 = pm.pm25;
  success = pm.isOk() && !isnan(temp) && ! isnan(humid);

  if (success){
    mqttPublish();
    display.clear();
    display.display();
  } else {
    Serial.println("No measurement");
  }

  WorkingStateResult state = sds.sleep();
  if (state.isWorking()) {
    Serial.println("Problem with sleeping the sensor.");
  } else {
    Serial.println("Sensor is sleeping");
  }
}


void loop() {
  if (i>=interval) {
    makeMeas();
    i = 0;
  }

  if (digitalRead(btnLeft)) {
    display.clear();
    display.display();
  } else {
    displayResult();
  }

  if (!digitalRead(btnRight)){
    makeMeas();
  }

  delay(100);
  i++;
}
