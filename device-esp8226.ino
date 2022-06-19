#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define LED_ERROR D3
#define LED_OK D4
#define DHTPIN D5
#define LIGHT_PIN A0
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define LED_BLINK_DELAY      500
#define WIFI_RECONNECT_DELAY 1000
#define MQTT_RECONNECT_DELAY 5000

#define WIFI_SSID        ""
#define WIFI_PASSWORD    ""

#define MQTT_SERVER      ""
#define MQTT_PORT        8883 //1883
#define MQTT_USER        ""
#define MQTT_PASSWORD    ""
#define MQTT_CLIENT_NAME "DTH11-01"

#define REFRESH_INTERVAL 60000
#define ROOM_ID          "living"

WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);
unsigned long last_update = 0;

void setup() {
  Serial.begin(9600);
  pinMode(LED_ERROR, OUTPUT);
  pinMode(LED_OK, OUTPUT);
  pinMode(LIGHT_PIN, INPUT);
  dht.begin();
  espClient.setInsecure();
  setup_wifi();
  setup_mqtt();
}

void ledBlink(int pin) {
  digitalWrite(pin, HIGH);
  delay(LED_BLINK_DELAY / 2);
  digitalWrite(pin, LOW);
  delay(LED_BLINK_DELAY / 2);
}


void setup_wifi() {
  delay(500);
  Serial.print(String("Attempting to connect to ") + WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    ledBlink(LED_ERROR);
    delay(WIFI_RECONNECT_DELAY - LED_BLINK_DELAY);
  }
  Serial.print(String(" done. IP address: "));
  Serial.println(WiFi.localIP());
  ledBlink(LED_OK);
}


void setup_mqtt() {
  Serial.println(String("Attempting to connect to ") + MQTT_SERVER + " as " + MQTT_CLIENT_NAME);
  while (!mqtt_client.connected()) {
    mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
    if (mqtt_client.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("done.");
      ledBlink(LED_OK);
    }
    else {
      Serial.println(String("Could not connect to broker: ") +  mqtt_client.state());
      ledBlink(LED_ERROR);
      delay(MQTT_RECONNECT_DELAY - LED_BLINK_DELAY);
    }
  }
}


void loop() {
  mqtt_client.loop();
  unsigned long now = millis();
  if (now - last_update >= REFRESH_INTERVAL) {
    last_update = now;
    sensRead();
  }
}

String buildMessage(double value, double absoluteError) {
  String message = 
    String("{\"deviceIdentifier\":\"") + MQTT_CLIENT_NAME
    + "\",\"value\":" + value
    + ",\"absoluteError\":" + absoluteError + "}";
  return message;
}

void publishMessage(String type, String message) {
  String topic = String("sensors/") + ROOM_ID + "/" + type;
  mqtt_client.publish(topic.c_str(), message.c_str(), true);
}

void sensRead() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  float heat_index = dht.computeHeatIndex(temperature, humidity, false);
  float light_level = analogRead(LIGHT_PIN) / 1023.0f;
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Could not read DHT11 sensor.");
    ledBlink(LED_ERROR);
    return;
  }
  
  publishMessage("temperature", buildMessage(temperature, 2.0));
  publishMessage("humidity", buildMessage(humidity, 5.0));
  publishMessage("heatIndex", buildMessage(heat_index, 5.0));
  publishMessage("lightLevel", buildMessage(light_level, 1.0f / 1024.0f));
  ledBlink(LED_OK);
}
