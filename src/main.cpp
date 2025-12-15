#include <Arduino.h> // Keep this line for VS Code
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
// LEDs: [0]=Red, [1]=Orange, [2]=Magenta, [3]=Green, [4]=Cyan
int leds[] = {17, 27, 22, 23, 19}; 

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Preferences preferences; 

unsigned long lastActivity = 0;
unsigned long sleepDuration = 60000; 

void setup_wifi() {
  delay(10);
  Serial.println("Connexion WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connecté!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  lastActivity = millis(); 

  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  Serial.print("Reçu: "); Serial.println(message);

  // --- ACTIONS ---
  for(int i=0; i<5; i++) digitalWrite(leds[i], LOW); // Reset all

  if (message == "STOP") {
      digitalWrite(leds[0], HIGH); // Rouge: Arret Urgence
  }
  else if (message == "ALARM") {
      digitalWrite(leds[1], HIGH); // Orange: Avertissement
  }
  else if (message == "CONFIRM") {
      digitalWrite(leds[2], HIGH); // Magenta: Bloquer Portes
  }
  else if (message == "START") {
      digitalWrite(leds[3], HIGH); // Vert: Eclairage
  }
  else if (message == "FAN_ON") {
      // NOTE: "FAN_ON" code means "Appel Assistance" now
      digitalWrite(leds[4], HIGH); // Cyan: Appel Assistance
  }
  
  // --- ADMIN SETTINGS ---
  else if (message == "SET_LONG") {
    sleepDuration = 120000;
    Serial.println("Admin: Veille = 2 mins");
  }
  else if (message == "SET_SHORT") {
    sleepDuration = 10000;
    Serial.println("Admin: Veille = 10 secs");
  }

  // Flash Memory Save
  preferences.begin("my-app", false);
  preferences.putString("last_cmd", message);
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  for(int i=0; i<5; i++) pinMode(leds[i], OUTPUT);
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastActivity = millis();
}

void loop() {
  if (!client.connected()) {
    if (client.connect("ESP32_Medical_Tunisia")) { 
      client.subscribe(topic_sub);
      Serial.println("MQTT Prêt!");
    }
  }
  client.loop();

  // Sensor Read
  static unsigned long lastSensor = 0;
  if(millis() - lastSensor > 5000) {
    float t = dht.readTemperature();
    Serial.print("Temp: "); Serial.println(t);
    lastSensor = millis();
  }

  // Deep Sleep Logic
  if (millis() - lastActivity > sleepDuration) {
    Serial.println("Dodo...");
    esp_deep_sleep_start();
  }
}