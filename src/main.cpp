#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <time.h>

// ===== WIFI =====
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ===== MQTT =====
const char* mqtt_server = "9867a0f8b99b43b293147b4f4cb0beba.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "Hoakhanh";
const char* mqtt_password = "Hk12345678";

// ===== MQTT TOPIC =====
const char* topic_pub        = "mqtt/temp_humi";
const char* topic_led        = "mqtt/led";
const char* topic_relay      = "mqtt/relay";
const char* topic_mode       = "mqtt/mode";
const char* topic_threshold  = "mqtt/humi_threshold";
const char* topic_status     = "mqtt/status";

// ===== DHT22 =====
#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ===== RELAY + LED =====
#define RELAY_PIN 2
#define LED_PIN   21

// ===== OLED =====
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ===== MQTT =====
WiFiClientSecure espClient;
PubSubClient client(espClient);

// ===== STATE =====
String relayState = "OFF";
String ledState   = "OFF";

String controlMode = "MANUAL";

float humiThreshold = 80.0;

// ===== AUTO WATER =====
bool autoWatering = false;
unsigned long autoStartMillis = 0;
const unsigned long wateringDuration = 10000; // 10 giây

// ===== WIFI =====
void setup_wifi() {
  Serial.print("Connecting WiFi...");
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

// ===== NTP TIME =====
void setupTime() {
  configTime(7 * 3600, 0, "pool.ntp.org");

  Serial.print("Waiting time");

  time_t now = time(nullptr);

  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println("\nTime OK");
}

// ===== CALLBACK MQTT =====
void callback(char* topic, byte* payload, unsigned int length) {

  String msg = "";

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  msg.trim();
  msg.toLowerCase();

  Serial.print("Nhan topic: ");
  Serial.print(topic);
  Serial.print(" | msg: ");
  Serial.println(msg);

  // ===== LED =====
  if (String(topic) == topic_led) {
    ledState = msg;
  }

  // ===== RELAY =====
  if (String(topic) == topic_relay) {

    // Chỉ cho manual điều khiển relay
    if (controlMode == "MANUAL") {
      relayState = msg;
    }
  }

  // ===== MODE =====
  if (String(topic) == topic_mode) {

    if (msg == "auto") {
      controlMode = "AUTO";
    }

    if (msg == "manual") {
      controlMode = "MANUAL";
    }

    Serial.print("Mode = ");
    Serial.println(controlMode);
  }

  // ===== HUMIDITY THRESHOLD =====
  if (String(topic) == topic_threshold) {

    float value = msg.toFloat();

    if (value > 0 && value <= 100) {
      humiThreshold = value;
    }

    Serial.print("Threshold = ");
    Serial.println(humiThreshold);
  }
}

// ===== MQTT RECONNECT =====
void reconnect() {

  while (!client.connected()) {

    Serial.print("MQTT connecting...");

    String clientId = "ESP32_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(),
                       mqtt_username,
                       mqtt_password)) {

      Serial.println("OK");

      client.subscribe(topic_led);
      client.subscribe(topic_relay);
      client.subscribe(topic_mode);
      client.subscribe(topic_threshold);

      client.publish(topic_status, "ESP32 Connected");

    } else {

      Serial.print("Failed rc=");
      Serial.println(client.state());

      delay(2000);
    }
  }
}

// ===== CHECK TIME WINDOW =====
bool isWateringTime() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return false;
  }

  int hour = timeinfo.tm_hour;
  int minute = timeinfo.tm_min;

  // ===== SÁNG =====
  bool morning =
      (hour == 5 && minute >= 30) ||
      (hour == 6 && minute == 0);

  // ===== CHIỀU =====
  bool evening =
      (hour == 17 && minute >= 30) ||
      (hour == 18 && minute == 0);

  return (morning || evening);
}

// ===== OLED =====
void displayOLED(float t, float h) {

  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.setCursor(0, 12);
  u8g2.print("Temp: ");
  u8g2.print(t, 1);
  u8g2.print(" C");

  u8g2.setCursor(0, 26);
  u8g2.print("Humi: ");
  u8g2.print(h, 1);
  u8g2.print(" %");

  u8g2.setCursor(0, 40);
  u8g2.print("Relay: ");
  u8g2.print(relayState);

  u8g2.setCursor(0, 54);
  u8g2.print(controlMode);

  u8g2.sendBuffer();
}

// ===== SEND MQTT =====
void sendMQTT(float t, float h) {

  String payload = "{";

  payload += "\"temperature\":" + String(t,1) + ",";
  payload += "\"humidity\":" + String(h,1) + ",";
  payload += "\"led\":\"" + ledState + "\",";
  payload += "\"relay\":\"" + relayState + "\",";
  payload += "\"mode\":\"" + controlMode + "\",";
  payload += "\"threshold\":" + String(humiThreshold,1);

  payload += "}";

  Serial.println(payload);

  client.publish(topic_pub, payload.c_str());
}

// ===== SETUP =====
void setup() {

  Serial.begin(115200);

  randomSeed(micros());

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  dht.begin();

  Wire.begin(22, 23);

  u8g2.begin();

  setup_wifi();

  setupTime();

  espClient.setInsecure();

  client.setServer(mqtt_server, mqtt_port);

  client.setCallback(callback);

  Serial.println("System ready!");
}

// ===== LOOP =====
void loop() {

  // ===== MQTT =====
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  // ===== SENSOR =====
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {

    Serial.println("DHT Error!");

    delay(2000);

    return;
  }

  // ===== AUTO MODE =====
  if (controlMode == "AUTO") {

    bool validTime = isWateringTime();

    // Điều kiện bật relay:
    // 1. Độ ẩm < ngưỡng
    // 2. Đúng giờ tưới
    // 3. Chưa bật auto

    if (h < humiThreshold &&
        validTime &&
        !autoWatering) {

      relayState = "on";

      autoWatering = true;

      autoStartMillis = millis();

      Serial.println("AUTO WATERING START");

      client.publish(topic_status,
                     "AUTO WATERING START");
    }

    // Tắt relay sau thời gian tưới
    if (autoWatering &&
        millis() - autoStartMillis >= wateringDuration) {

      relayState = "off";

      autoWatering = false;

      Serial.println("AUTO WATERING STOP");

      client.publish(topic_status,
                     "AUTO WATERING STOP");
    }
  }

  // ===== OUTPUT =====
  bool relayOn =
      (relayState == "on" ||
       relayState == "ON" ||
       relayState == "1");

  bool ledOn =
      (ledState == "on" ||
       ledState == "ON" ||
       ledState == "1");

  digitalWrite(RELAY_PIN,
               relayOn ? HIGH : LOW);

  digitalWrite(LED_PIN,
               ledOn ? HIGH : LOW);

  // ===== OLED =====
  displayOLED(t, h);

  // ===== MQTT SEND =====
  sendMQTT(t, h);

  delay(3000);
}