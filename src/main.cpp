#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>

// ===== WIFI =====
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ===== MQTT =====
const char* mqtt_server = "9867a0f8b99b43b293147b4f4cb0beba.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "Hoakhanh";
const char* mqtt_password = "Hk12345678";

const char* topic_pub   = "mqtt/temp_humi";
const char* topic_led   = "mqtt/led";
const char* topic_relay = "mqtt/relay";

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

// ===== CALLBACK =====
// void callback(char* topic, byte* payload, unsigned int length) {
//   String msg = "";

//   for (int i = 0; i < length; i++) {
//     msg += (char)payload[i];
//   }

//   Serial.print("Nhan: ");
//   Serial.print(topic);
//   Serial.print(" = ");
//   Serial.println(msg);

//   if (String(topic) == topic_led) {
//     ledState = msg;
//   }

//   if (String(topic) == topic_relay) {
//     relayState = msg;
//   }
// }

// ===== CALLBACK =====
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  
  // Xóa khoảng trắng thừa hoặc ký tự ẩn (rất hay gặp khi gửi từ Node-RED)
  msg.trim(); 
  
  Serial.print("Nhan: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(msg);

  // Chuyển toàn bộ msg thành chữ thường để dễ so sánh
  msg.toLowerCase(); 

  if (String(topic) == topic_led) {
    ledState = msg;
  }

  if (String(topic) == topic_relay) {
    relayState = msg;
  }
}

// ===== RECONNECT (CHẮC CHẮN CONNECT) =====
void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connecting...");

    String clientId = "ESP32_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("OK");

      client.subscribe(topic_led);
      client.subscribe(topic_relay);

    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" -> retry in 2s");
      delay(2000);
    }
  }
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
  delay(1000); // 🔥 rất quan trọng

  // TLS (bắt buộc cho port 8883)
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

  Serial.print("MQTT connected: ");
  Serial.println(client.connected());

  // ===== READ SENSOR =====
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("DHT error!");
    delay(2000);
    return;
  }

  // ===== CONTROL =====
  // digitalWrite(RELAY_PIN, relayState == "ON" ? HIGH : LOW);
  // digitalWrite(LED_PIN,   ledState   == "ON" ? HIGH : LOW);

  // ===== CONTROL =====
  // Kiểm tra nếu Node-RED gửi "on", "1", hoặc "true" thì đều bật Relay/Led
  bool isRelayOn = (relayState == "on" || relayState == "1" || relayState == "true");
  bool isLedOn   = (ledState   == "on" || ledState   == "1" || ledState   == "true");

  digitalWrite(RELAY_PIN, isRelayOn ? HIGH : LOW);
  digitalWrite(LED_PIN,   isLedOn   ? HIGH : LOW);
  
  // ===== OLED =====
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);

  u8g2.setCursor(0, 12);
  u8g2.print("Temp: "); u8g2.print(t, 1); u8g2.print(" C");

  u8g2.setCursor(0, 26);
  u8g2.print("Humi: "); u8g2.print(h, 1); u8g2.print(" %");

  u8g2.setCursor(0, 42);
  u8g2.print("Relay: "); u8g2.print(relayState);

  u8g2.setCursor(0, 58);
  u8g2.print("LED: "); u8g2.print(ledState);

  u8g2.sendBuffer();

  // ===== SEND MQTT =====
  if (client.connected()) {
    String payload = "{";
    payload += "\"temperature\":" + String(t,1) + ",";
    payload += "\"humidity\":" + String(h,1);
    payload += "}";

    Serial.print("Sending: ");
    Serial.println(payload);

    if (client.publish(topic_pub, payload.c_str())) {
      Serial.println("Publish OK");
    } else {
      Serial.println("Publish FAIL");
    }
  }

  delay(3000);
}