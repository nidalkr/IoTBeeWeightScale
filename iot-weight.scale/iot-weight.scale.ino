#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <Firebase_ESP_Client.h>
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
#define DHTPIN     D7
#define DHTTYPE    DHT22

#define LIGHT_PIN  A0
#define BUZZER_PIN D0

// =====================
// DEVICES
// =====================
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// =====================
// DEMO / SIMULATION MODE (SADA JE UVIJEK DEMO)
// =====================
// Pritiskom 'r' u Serial Monitoru prebacuješ na sljedeću demo vrijednost.
// Pritiskom 't' postavljaš trenutnu vrijednost kao 0kg (tara).
float demoWeights[] = { 2.5, 6.8, 10.2, 13.7, 15.9, 18.4, 19.6, 12.1, 7.4, 3.0 };
const int demoCount = sizeof(demoWeights) / sizeof(demoWeights[0]);
int demoIndex = 0;
float demoWeight = demoWeights[0];

// "tara" offset za DEMO
float tareOffsetKg = 0.0f;

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
#if defined(ESP8266)
    yield();
#endif
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
  json.set("mode", mode); // uvijek "DEMO"

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
void setRtcOnBoot() {
  if (!rtc.begin()) {
    Serial.println("RTC nije pronađen!");
    return;
  }
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("RTC postavljen (compile time).");
}

void beepShort() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(60);
  digitalWrite(BUZZER_PIN, LOW);
}

const char* lightToText(int light) {
  if (light > 750) return "Sunny";
  if (light > 350) return "Cloudy";
  return "Night";
}

// DEMO težina sa tarom (nikad ne ide ispod 0)
float getWeightKgDemo() {
  float w = demoWeight - tareOffsetKg;
  if (w < 0) w = 0;
  return w;
}

// =====================
// SETUP
// =====================
void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Boot...");

  dht.begin();

  connectWiFi();
  setRtcOnBoot();
  initFirebase();

  Serial.println("\nKomande:");
  Serial.println("  r  -> sljedeca DEMO tezina");
  Serial.println("  t  -> tara (postavi trenutno na 0kg)");
  Serial.println("--------------------------------------------");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Beehive scale");
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
      demoIndex = (demoIndex + 1) % demoCount;
      demoWeight = demoWeights[demoIndex];

      Serial.print("DEMO tezina: ");
      Serial.print(demoWeight, 2);
      Serial.print(" kg, tareOffset=");
      Serial.print(tareOffsetKg, 2);
      Serial.println(" kg");

      beepShort();
    }

    if (ch == 't' || ch == 'T') {
      // postavi trenutnu demo vrijednost kao nulu
      tareOffsetKg = demoWeight;

      Serial.print("Tara OK. Offset postavljen na: ");
      Serial.print(tareOffsetKg, 2);
      Serial.println(" kg");

      beepShort();
    }
  }

  // ===== readings =====
  float weight = getWeightKgDemo();

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

  const char* modeStr = "DEMO";

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
  lcd.setCursor(0, 0);
  lcd.print("W:");
  lcd.print(weight, 1);
  lcd.print("kg   ");

  lcd.setCursor(8, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C  ");

  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(humidity, 0);
  lcd.print("% ");

  lcd.setCursor(6, 1);
  lcd.print(lightText);
  lcd.print("     ");

  // ===== firebase =====
  sendReading(weight, temperature, humidity, light, timestamp, modeStr);

  delay(10000);
#if defined(ESP8266)
  yield();
#endif
}
