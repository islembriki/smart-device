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
#define BUZZER_PIN 18  // <--- NEW: Buzzer Pin

// LEDs: [0]=Red, [1]=Yellow, [2]=Purple, [3]=Green, [4]=Blue/Cyan
int leds[] = {17, 27, 22, 23, 19}; 

// --- TEMPERATURE SETTINGS ---
// Wokwi defaults to 24°C. We set the limit to 25°C. 
// If you slide the sensor to 25.1°C or higher -> ALARM!
float tempLimit = 25.0; 

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
  
  // Setup LEDs
  for(int i=0; i<5; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }

  // Setup Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  
  dht.begin();
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  lastActivity = millis();
}

void loop() {
  // 1. READ TEMPERATURE FIRST (Safety Priority)
  float t = dht.readTemperature();
  
  // --- SAFETY MONITOR ---
  if (t > tempLimit) {
    // If temp is too high, IGNORE MQTT and Start Alarm
    Serial.print("🔥 FIRE ALERT! Temp: ");
    Serial.println(t);

    // Flashing Effect (Police Strobe)
    for(int i=0; i<5; i++) digitalWrite(leds[i], HIGH); // All ON
    tone(BUZZER_PIN, 1000); // Beeeep
    delay(200); // Wait

    for(int i=0; i<5; i++) digitalWrite(leds[i], LOW); // All OFF
    noTone(BUZZER_PIN); // Silence
    delay(200); // Wait

    return; // <--- This SKIPs the rest of the loop (No MQTT processing during fire)
  }

  // 2. NORMAL MODE (Only runs if Temp is safe)
  if (!client.connected()) {
    if (client.connect("ESP32_Tunisia_Final_V1")) { 
      client.subscribe(topic_sub);
      Serial.println("MQTT Connected!");
    }
  }
  client.loop();

  // Print Temp to console every 5 seconds for debugging
  static unsigned long lastSensor = 0;
  if(millis() - lastSensor > 5000) {
    Serial.print("Current Temp: "); 
    Serial.print(t); 
    Serial.println(" C (Status: OK)");
    lastSensor = millis();
  }

  // ENERGY SAVING
  if (millis() - lastActivity > 60000) {
    Serial.println("Sleep Mode Activated...");
    esp_deep_sleep_start();
  }
}