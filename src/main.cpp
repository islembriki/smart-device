#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Preferences.h>
#include "esp32-hal-cpu.h" 

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

// --- VARIABLES INTELLIGENTES ---
float tempLimit = 25.0;              // Seuil d'alarme
unsigned long sleep_timeout = 60000; // Temps avant veille (60s)
bool silent_mode = false;            
bool isSleeping = false;             // <--- STATE VARIABLE (Awake or Asleep)

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
  WiFi.setTxPower(WIFI_POWER_11dBm); 
}

void callback(char* topic, byte* payload, unsigned int length) {
  lastActivity = millis(); // Reset timer whenever a message arrives

  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message reçu: ");
  Serial.println(message);

  // --- 1. LOGIQUE DE REVEIL (SMART WAKE UP) ---
  if (isSleeping) {
    if (message == "START") {
      isSleeping = false;
      Serial.println("☀️ SYSTEME REVEILLE ! (Mode Normal Active)");
      
      // Petit signal visuel (Vert clignote une fois)
      digitalWrite(leds[3], HIGH);
      delay(500);
      digitalWrite(leds[3], LOW);
    } else {
      Serial.println("💤 Ignoré : Le système est en veille (Faites 'START' pour réveiller)");
    }
    return; // On arrête ici si on était endormi
  }

  // --- 2. COMMANDES SYSTEME (Seulement si réveillé) ---

  // Couper le son
  if (message == "SOUND:OFF") {
    silent_mode = true;
    noTone(BUZZER_PIN);
    Serial.println("🔇 Mode Silencieux activé");
    return;
  }
  if (message == "SOUND:ON") {
    silent_mode = false;
    Serial.println("🔊 Son réactivé");
    return;
  }

  // Reboot
  if (message == "REBOOT") {
    Serial.println("⚠️ REBOOT EN COURS...");
    delay(100);
    ESP.restart();
  }

  // Config Admin
  if (message.startsWith("SET_TEMP:")) {
    tempLimit = message.substring(9).toFloat();
    Serial.print("✅ Nouveau seuil Temp: "); Serial.println(tempLimit);
    return;
  }
  if (message.startsWith("SET_TIME:")) {
    sleep_timeout = message.substring(9).toInt() * 1000;
    Serial.print("✅ Nouveau délai Veille: "); Serial.println(sleep_timeout);
    return;
  }

  // --- 3. GESTION DES GESTES (Si température normale) ---
  if (currentTemp <= tempLimit) {
    // Reset LEDs
    for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);

    // Commandes Geste
    if (message == "STOP") digitalWrite(leds[0], HIGH);
    else if (message == "START") digitalWrite(leds[3], HIGH);
    else if (message == "ALARM") digitalWrite(leds[1], HIGH); 
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
  // 1. GESTION MQTT (CRITIQUE: Doit tourner même en veille pour recevoir le réveil)
  if (!client.connected()) {
    if (client.connect("ESP32_Tunisia_Final_V6")) { 
      client.subscribe(topic_sub);
    }
  }
  client.loop();

  // SI EN VEILLE : On arrête le loop ici (On économise les calculs et capteurs)
  if (isSleeping) {
    return; 
  }

  // --- TOUT CE QUI SUIT NE TOURNE QUE SI LE SYSTEME EST REVEILLE ---

  // 2. LECTURE TEMPÉRATURE (Toutes les 1s)
  if (millis() - lastTempCheck > 1000) { 
    currentTemp = dht.readTemperature();
    lastTempCheck = millis(); 
    // Debug discret
    // Serial.print("T: "); Serial.println(currentTemp); 
    
    // Si la température redevient normale après alerte, on coupe tout
    if (currentTemp <= tempLimit && blinkState == HIGH) {
        for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);
        noTone(BUZZER_PIN);
        blinkState = LOW;
    }
  }

  // 3. LOGIQUE ALARME FEU (> Limite)
  if (currentTemp > tempLimit) {
    lastActivity = millis(); // L'alarme empêche la mise en veille

    if (millis() - lastBlink > 200) {
      lastBlink = millis();
      blinkState = !blinkState; 

      for(int i=0; i<5; i++) digitalWrite(leds[i], blinkState);

      if (blinkState == HIGH && !silent_mode) {
        tone(BUZZER_PIN, 1000); 
      } else {
        noTone(BUZZER_PIN);    
      }
    }
  }

  // 4. SMART SLEEP (Mise en veille logicielle)
  if (millis() - lastActivity > sleep_timeout) {
    Serial.println("💤 Inactivité détectée -> Passage en Mode Veille (Standby)");
    Serial.println("   (Le WiFi reste actif. Faites le geste 'START' pour réveiller)");
    
    // Tout éteindre
    for(int i=0; i<5; i++) digitalWrite(leds[i], LOW);
    noTone(BUZZER_PIN);
    
    isSleeping = true; // Activation du mode veille
  }
}