#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>

// --- SETTINGS ---
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const char* topic_sub = "INSAT/tunisia/project2025"; 

// --- PINS ---
#define DHTPIN 15
#define DHTTYPE DHT22

// LEDs: [0]=Red, [1]=Yellow, [2]=Purple, [3]=Green, [4]=Blue/Cyan
int leds[] = {17, 27, 22, 23, 19}; 

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Preferences preferences; 

unsigned long lastActivity = 0;

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  lastActivity = millis(); 

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received: ");
  Serial.println(message);

  // 1. Reset LEDs
  for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);

  // 2. Activate based on Command
  if (message == "STOP") {
    digitalWrite(leds[0], HIGH); // Red
  }
  else if (message == "START") {
    digitalWrite(leds[3], HIGH); // Green
  }
  else if (message == "ALARM") {
    digitalWrite(leds[0], HIGH);
    digitalWrite(leds[1], HIGH);
    digitalWrite(leds[2], HIGH);
  }
  else if (message == "FAN_ON") {
     digitalWrite(leds[4], HIGH); // Cyan
  }
  else if (message == "CONFIRM") {
     digitalWrite(leds[2], HIGH); // Purple
  }

  // SAVE TO FLASH
  preferences.begin("my-app", false);
  preferences.putString("last_cmd", message);
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  
  for(int i=0; i<5; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }
  
  dht.begin();
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  lastActivity = millis();
}

void loop() {
  if (!client.connected()) {
    if (client.connect("ESP32_Tunisia_Final_V1")) { 
      client.subscribe(topic_sub);
      Serial.println("MQTT Connected!");
    }
  }
  client.loop();

  // READ SENSOR
  static unsigned long lastSensor = 0;
  if(millis() - lastSensor > 5000) {
    float t = dht.readTemperature();
    Serial.print("Temp: "); 
    Serial.print(t); 
    Serial.println(" C");
    lastSensor = millis();
  }

  // ENERGY SAVING
  if (millis() - lastActivity > 60000) {
    Serial.println("Sleep Mode Activated...");
    esp_deep_sleep_start();
  }
}