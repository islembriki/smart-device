#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>

// --- SETTINGS ---
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const char* topic_sub = "isima/tunisia/project2025"; 

// --- PINS (YOUR ORIGINAL CONFIGURATION) ---
#define DHTPIN 15
#define DHTTYPE DHT22

// Your specific pin list that works:
int leds[] = {17, 27, 22, 23, 19}; 
// Assuming order: [0]=Red, [1]=Yellow, [2]=Purple, [3]=Green, [4]=Blue/Cyan

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
  lastActivity = millis(); // Reset sleep timer because we got a command

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received: ");
  Serial.println(message);

  // --- LOGIC FOR NEW GESTURES ---
  
  // 1. Turn off all LEDs first (Reset state)
  for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);

  // 2. Activate based on AI Command
  if (message == "STOP") {
    // Gesture: FIST -> RED LED (Emergency)
    digitalWrite(leds[0], HIGH); 
  }
  else if (message == "START") {
    // Gesture: THUMBS UP -> GREEN LED (Production Active)
    digitalWrite(leds[3], HIGH);
  }
  else if (message == "ALARM") {
    // Gesture: ROCK SIGN -> RED + YELLOW + PURPLE (Visual Alarm)
    digitalWrite(leds[0], HIGH);
    digitalWrite(leds[1], HIGH);
    digitalWrite(leds[2], HIGH);
  }
  else if (message == "FAN_ON") {
     // Gesture: VICTORY/PEACE -> BLUE/CYAN LED (Ventilation)
     digitalWrite(leds[4], HIGH);
  }
  else if (message == "CONFIRM") {
     // Gesture: OK SIGN -> PURPLE LED (System Check)
     digitalWrite(leds[2], HIGH);
  }

  // SAVE TO FLASH (Requirement: User Parameters/History)
  preferences.begin("my-app", false);
  preferences.putString("last_cmd", message);
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Your Pins
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
    if (client.connect("ESP32_Tunisia_Student_123")) { // Unique ID
      client.subscribe(topic_sub);
      Serial.println("MQTT Connected! Waiting for gestures...");
    }
  }
  client.loop();

  // READ SENSOR (Requirement: Acquisition)
  static unsigned long lastSensor = 0;
  if(millis() - lastSensor > 5000) {
    float t = dht.readTemperature();
    Serial.print("Sensor Status -> Temp: "); 
    Serial.print(t); 
    Serial.println(" C");
    lastSensor = millis();
  }

  // ENERGY OPTIMIZATION (Requirement: Sleep)
  // If no gesture received for 60 seconds, sleep
  if (millis() - lastActivity > 60000) {
    Serial.println("No gestures detected. Saving Energy...");
    esp_deep_sleep_start();
  }
}