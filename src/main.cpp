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
#define BUZZER_PIN 18

// LEDs
int leds[] = {17, 27, 22, 23, 19}; 

// Limit for Alarm
float tempLimit = 25.0; 

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Preferences preferences; 

unsigned long lastActivity = 0;
unsigned long lastTempCheck = 0; // <--- NEW TIMER
float currentTemp = 0.0;         // Store the temp here

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

  // Reset LEDs
  for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);

  // Commands
  if (message == "STOP") digitalWrite(leds[0], HIGH);
  else if (message == "START") digitalWrite(leds[3], HIGH);
  else if (message == "ALARM") {
    digitalWrite(leds[0], HIGH);
    digitalWrite(leds[1], HIGH);
    digitalWrite(leds[2], HIGH);
  }
  else if (message == "FAN_ON") digitalWrite(leds[4], HIGH);
  else if (message == "CONFIRM") digitalWrite(leds[2], HIGH);

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
  pinMode(BUZZER_PIN, OUTPUT);
  
  dht.begin();
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  lastActivity = millis();
}

void loop() {
  // --- 1. SMART TEMP READING (Only once every 1 second) ---
  if (millis() - lastTempCheck > 1000) { 
    currentTemp = dht.readTemperature();
    lastTempCheck = millis(); // Reset timer
    
    // Debug print
    Serial.print("Temp Update: ");
    Serial.println(currentTemp);
  }

  // --- 2. SAFETY CHECK ---
  if (currentTemp > tempLimit) {
    // FIRE ALARM LOGIC
    Serial.println("🔥 ALARM! FIRE!");
    for(int i=0; i<5; i++) digitalWrite(leds[i], HIGH); 
    tone(BUZZER_PIN, 1000); 
    delay(200); // Short delay is okay here because it's an emergency
    for(int i=0; i<5; i++) digitalWrite(leds[i], LOW); 
    noTone(BUZZER_PIN); 
    delay(200);
    return; // Stop here, ignore MQTT until fire is gone
  }

  // --- 3. NORMAL MQTT LISTENING ---
  // This runs very fast now because we aren't reading the sensor every time!
  if (!client.connected()) {
    if (client.connect("ESP32_Tunisia_Final_V1")) { 
      client.subscribe(topic_sub);
    }
  }
  client.loop();

  // --- 4. ENERGY SAVING ---
  if (millis() - lastActivity > 60000) {
    Serial.println("Sleep Mode Activated...");
    esp_deep_sleep_start();
  }
}