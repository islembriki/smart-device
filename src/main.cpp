#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include "esp32-hal-cpu.h" // Nécessaire pour l'économie d'énergie CPU

// --- SETTINGS ---
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const char* topic_sub = "INSAT/tunisia/project2025"; 

// --- PINS ---
#define DHTPIN 15
#define DHTTYPE DHT22
#define BUZZER_PIN 18

// LEDs: [0]=Red, [1]=Yellow, [2]=Purple, [3]=Green, [4]=Blue/Cyan
int leds[] = {17, 27, 22, 23, 19}; 

// --- VARIABLES INTELLIGENTES (Modifiables via MQTT) ---
float tempLimit = 25.0;              // Seuil d'alarme (défaut 25.0)
unsigned long sleep_timeout = 60000; // Temps avant veille (défaut 60s)
bool silent_mode = false;            // Mode silencieux (défaut Faux)

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Preferences preferences; 

unsigned long lastActivity = 0;
unsigned long lastTempCheck = 0; 
unsigned long lastBlink = 0;     
boolean blinkState = LOW;        
float currentTemp = 0.0;         

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  
  // ASTUCE 1: Réduire la puissance WiFi (Economie)
  WiFi.setTxPower(WIFI_POWER_11dBm); 
}

void callback(char* topic, byte* payload, unsigned int length) {
  lastActivity = millis(); 

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message reçu: ");
  Serial.println(message);

  // --- NOUVELLES COMMANDES ---

  // 1. Couper le son à distance (tout en gardant l'alarme visuelle)
  if (message == "SOUND:OFF") {
    silent_mode = true;
    noTone(BUZZER_PIN); // Arrêt immédiat du son
    Serial.println("🔇 Alarme rendue silencieuse");
    return;
  }
  if (message == "SOUND:ON") {
    silent_mode = false;
    Serial.println("🔊 Son réactivé");
    return;
  }

  // 2. Redémarrage à distance
  if (message == "REBOOT") {
    Serial.println("⚠️ REBOOT EN COURS...");
    delay(100);
    ESP.restart();
  }

  // 3. Changer seuil Température (Ex: SET_TEMP:30)
  if (message.startsWith("SET_TEMP:")) {
    tempLimit = message.substring(9).toFloat();
    Serial.print("✅ Nouveau seuil Temp: "); Serial.println(tempLimit);
    return;
  }

  // 4. Changer délai Veille (Ex: SET_TIME:30)
  if (message.startsWith("SET_TIME:")) {
    sleep_timeout = message.substring(9).toInt() * 1000;
    Serial.print("✅ Nouveau délai Veille: "); Serial.println(sleep_timeout);
    return;
  }

  // --- GESTION DES GESTES (Uniquement si température normale) ---
  if (currentTemp <= tempLimit) {
    // Reset LEDs
    for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);

    // Commandes Geste
    if (message == "STOP") digitalWrite(leds[0], HIGH);
    else if (message == "START") digitalWrite(leds[3], HIGH);
    else if (message == "ALARM") { // Geste "Rock" (Avertissement simple)
        digitalWrite(leds[1], HIGH); 
    }
    else if (message == "FAN_ON") digitalWrite(leds[4], HIGH);
    else if (message == "CONFIRM") digitalWrite(leds[2], HIGH);

    // Sauvegarde Flash
    preferences.begin("my-app", false);
    preferences.putString("last_cmd", message);
    preferences.end();
  }
}

void setup() {
  Serial.begin(115200);

  // ASTUCE 2: CPU à 80MHz (Economie Energie)
  setCpuFrequencyMhz(80); 

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
  // 1. GESTION MQTT (Toujours active pour recevoir SOUND:OFF même pendant l'alarme)
  if (!client.connected()) {
    if (client.connect("ESP32_Tunisia_Final_V5")) { 
      client.subscribe(topic_sub);
    }
  }
  client.loop();

  // 2. LECTURE TEMPÉRATURE (Toutes les 1s)
  if (millis() - lastTempCheck > 1000) { 
    currentTemp = dht.readTemperature();
    lastTempCheck = millis(); 
    Serial.print("T: "); Serial.print(currentTemp); Serial.print(" / Lim: "); Serial.println(tempLimit);
    
    // Si la température redevient normale, on coupe tout
    if (currentTemp <= tempLimit && blinkState == HIGH) {
        for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);
        noTone(BUZZER_PIN);
        blinkState = LOW;
    }
  }

  // 3. LOGIQUE ALARME FEU (> Limite)
  if (currentTemp > tempLimit) {
    lastActivity = millis(); // On reste éveillé

    // Clignotement Rapide (200ms) - Non bloquant pour écouter MQTT
    if (millis() - lastBlink > 200) {
      lastBlink = millis();
      blinkState = !blinkState; // Inverse ON/OFF

      // Action LEDs (Clignotent toutes)
      for(int i=0; i<5; i++) digitalWrite(leds[i], blinkState);

      // Action Sonore (BIP BIP BIP)
      // On sonne seulement si l'état est HIGH et que le mode silencieux est OFF
      if (blinkState == HIGH && !silent_mode) {
        tone(BUZZER_PIN, 1000); // Son à 1000Hz
      } else {
        noTone(BUZZER_PIN);     // Silence
      }
    }
  }

  // 4. ECONOMIE D'ENERGIE (Deep Sleep)
  if (millis() - lastActivity > sleep_timeout) {
    Serial.println("💤 Dodo (Deep Sleep)...");
    for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);
    noTone(BUZZER_PIN);
    esp_deep_sleep_start();
  }
}
