#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <Firebase_ESP_Client.h>
#include <HX711.h>
#include <DHT.h>
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>


// =====================
// WIFI / FIREBASE
// =====================
#define WIFI_SSID "KRHAN"
#define WIFI_PASSWORD "carskivinogradi"

#define API_KEY "AIzaSyDsa0IStDgJpw4c8AZo88f5SqbsaELBFPQ"
#define DATABASE_URL "https://iot-bee-weight-scale-default-rtdb.europe-west1.firebasedatabase.app"
#define USER_EMAIL "esp@beehive.local"
#define USER_PASSWORD "vaga123"

const char deviceId[] = "beehive1";

// =====================
// PINS
// =====================
#define HX711_DOUT D5
#define HX711_SCK  D6

#define DHTPIN     D7
#define DHTTYPE    DHT22

#define LIGHT_PIN  A0
#define BUZZER_PIN D0

// =====================
// DEVICES
// =====================
HX711 scale;
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =====================
// SCALE
// =====================
float calibrationFactor = -7050;

// =====================
// DEMO / SIMULATION MODE
// =====================
// Ako HX711 ne radi, koristiš demo težinu za prikaz sistema.
// Pritiskom 'r' u Serial Monitoru prebacuješ na sljedeću demo vrijednost.
bool DEMO_MODE = true;

// Demo lista (0–20kg) – realistični skokovi
float demoWeights[] = { 2.5, 6.8, 10.2, 13.7, 15.9, 18.4, 19.6, 12.1, 7.4, 3.0 };
const int demoCount = sizeof(demoWeights) / sizeof(demoWeights[0]);
int demoIndex = 0;
float demoWeight = demoWeights[0];

DateTime vrijeme = rtc.now();

// =====================
// HELPERS
// =====================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Povezujem se na Wi-Fi");
  uint8_t retries = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++retries > 60) {
      Serial.println("\nNeuspjelo spajanje na Wi-Fi");
      return;
    }
  }
  Serial.println("\nWi-Fi povezan!");
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void sendReading(float weight, float temp, float hum, float light, const char* timestamp, const char* mode) {
  FirebaseJson json;
  json.set("ts", timestamp);
  json.set("weight", weight);
  json.set("temp", temp);
  json.set("hum", hum);
  json.set("light", light);
  json.set("mode", mode); // "REAL" ili "DEMO"

  String latestPath = String("/devices/") + deviceId + "/latest";
  String readingsPath = String("/devices/") + deviceId + "/readings";

  if (!Firebase.RTDB.setJSON(&fbdo, latestPath.c_str(), &json)) {
    Serial.print("Greška kod slanja (latest): ");
    Serial.println(fbdo.errorReason());
  }

  if (!Firebase.RTDB.pushJSON(&fbdo, readingsPath.c_str(), &json)) {
    Serial.print("Greška kod slanja (readings): ");
    Serial.println(fbdo.errorReason());
  }
}

// RTC: bez baterije moraš postaviti vrijeme na svakom bootu.
// Ako ti smeta reset na svaki restart, ubaci CR2032.
void setRtcOnBoot() {
  if (!rtc.begin()) {
    Serial.println("RTC nije pronađen!");
    return;
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("RTC postavljen (compile time).");
}

void beepShort() {
  // Ako ti buzzer smeta na boot-u, najbolje je tranzistor ili pasivni buzzer.
  digitalWrite(BUZZER_PIN, HIGH);
  delay(60);
  digitalWrite(BUZZER_PIN, LOW);
}

// Vraća težinu: REAL ako HX711 radi, inače DEMO
float readWeightKg(bool &isDemoOut) {
  if (!DEMO_MODE) {
    // pokušaj realno
    if (scale.is_ready()) {
      isDemoOut = false;
      return scale.get_units(10); // kg (ovisno o kalibraciji)
    } else {
      // fallback na demo ako nije ready
      isDemoOut = true;
      return demoWeight;
    }
  }

  // DEMO mode
  isDemoOut = true;
  return demoWeight;
}

const char* lightToText(int light) {
  if (light > 750) {
    return "Sunny";
  } 
  else if (light > 350) {
    return "Cloudy";
  } 
  else {
    return "Night";
  }
}


// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // HX711 init (real)
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(calibrationFactor);
  scale.tare();

  dht.begin();
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  connectWiFi();
  setRtcOnBoot();
  initFirebase();

  Serial.println("\nKomande:");
  Serial.println("  r  -> sljedeca DEMO tezina (0–20kg)");
  Serial.println("  d  -> toggle DEMO_MODE (DEMO <-> REAL pokusaj)");
  Serial.println("  t  -> tare (samo ako si u REAL i HX711 radi)");
  Serial.println("--------------------------------------------");

  lcd.setCursor(0, 0);
  lcd.print("Beehive scale");
  lcd.setCursor(0, 1);
  delay(1200);
  lcd.clear();
}

// =====================
// LOOP
// =====================
void loop() {
  // ===== serial commands =====
  while (Serial.available()) {
    char ch = (char)Serial.read();

    if (ch == 'r' || ch == 'R') {
      // sljedeca demo tezina
      demoIndex = (demoIndex + 1) % demoCount;
      demoWeight = demoWeights[demoIndex];

      Serial.print("DEMO tezina promijenjena na: ");
      Serial.print(demoWeight, 2);
      Serial.println(" kg");

      beepShort();
    }

    if (ch == 'd' || ch == 'D') {
      DEMO_MODE = !DEMO_MODE;
      Serial.print("DEMO_MODE = ");
      Serial.println(DEMO_MODE ? "ON (DEMO)" : "OFF (pokusaj REAL)");
      beepShort();
    }

    if (ch == 't' || ch == 'T') {
      if (!DEMO_MODE && scale.is_ready()) {
        scale.set_scale(calibrationFactor);
        scale.tare();
        Serial.println("Tare OK (REAL).");
      } else {
        Serial.println("Tare preskocen (DEMO ili HX711 nije ready).");
      }
      beepShort();
    }
  }

  // ===== readings =====
  bool isDemo = false;
  float weight = readWeightKg(isDemo);

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  int lightRaw = analogRead(LIGHT_PIN);
  float light = (float)lightRaw;
  const char* lightText = lightToText(lightRaw);


  // time
  DateTime now = rtc.now();
  char timestamp[20];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

  const char* modeStr = isDemo ? "DEMO" : "REAL";

  // ===== serial print =====
  Serial.println("--------------");
  Serial.print("Vrijeme: "); Serial.println(timestamp);
  Serial.print("Mode: "); Serial.println(modeStr);
  Serial.print("Tezina: "); Serial.print(weight, 2); Serial.println(" kg");
  Serial.print("Temperatura: "); Serial.print(temperature, 1); Serial.println(" C");
  Serial.print("Vlaznost: "); Serial.print(humidity, 1); Serial.println(" %");
  Serial.print("Svjetlost (ADC): "); Serial.println(light);
  Serial.println("--------------");

  // ===== LCD =====
  // ===== RED 0 =====
lcd.setCursor(0, 0);
lcd.print("W:");
lcd.print(weight, 1);
lcd.print("kg");

// malo razmaka do temperature
lcd.setCursor(8, 0);
lcd.print(" T:");
lcd.print(temperature, 1);
lcd.print("C");

// ===== RED 1 =====
lcd.setCursor(0, 1);
lcd.print("H:");
lcd.print(humidity, 0);
lcd.print("%");

// stanje svjetla (Sunny / Cloudy / Night)
lcd.setCursor(6, 1);
lcd.print(lightText);


  // ===== firebase =====
  sendReading(weight, temperature, humidity, light, timestamp, modeStr);

  delay(10000);
}
