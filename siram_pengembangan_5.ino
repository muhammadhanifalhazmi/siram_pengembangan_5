#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

#define DHT_PIN D6  // Pin untuk sensor DHT11
#define RELAY1_PIN D4  // Relay 1
#define RELAY2_PIN D5  // Relay 2
#define RELAY3_PIN D7  // Relay 3 (baru)
#define SOIL_MOISTURE_PIN A0  // Pin untuk sensor kelembaban tanah

LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address, 16 kolom dan 2 baris
DHT dht(DHT_PIN, DHT11);

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID " "
#define WIFI_PASSWORD " "

#define API_KEY " "
#define DATABASE_URL " "

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;
bool isAutomationEnabled = false;  // Status otomatisasi dari Firebase

void setup() {
  pinMode(DHT_PIN, INPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);  // Tambahan untuk Relay 3
  digitalWrite(RELAY1_PIN, HIGH); 
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);  // Pastikan relay 3 mati saat startup

  dht.begin();  // Inisialisasi sensor DHT

  lcd.begin();  // Inisialisasi LCD
  lcd.backlight();  // Nyalakan backlight LCD

  // Tampilkan pesan awal
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Halo Petani");
  delay(3000);  // Tampilkan pesan selama 3 detik

  Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback; 
  Firebase.begin(&config, &auth);
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int soilMoisture = analogRead(SOIL_MOISTURE_PIN);

  lcd.clear();
  
  // Cek apakah data sensor tersedia
  if (isnan(temperature) || isnan(humidity)) {
    lcd.setCursor(0, 0);
    lcd.print("Sedang mengambil");
    lcd.setCursor(0, 1);
    lcd.print("data...");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print("Humi: ");
    lcd.print(humidity);
    lcd.print(" %");
    delay(2000);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Soil: ");
    lcd.print(soilMoisture);
    lcd.print(" %");
    delay(2000);
  }

  // Update status relay dan otomatisasi dari Firebase
  String pumpState1, pumpState2, pumpState3;

  if (Firebase.ready() && signupOK) {
    // Dapatkan status otomatisasi dari Firebase
    if (Firebase.RTDB.getString(&fbdo, "Automation/Status")) {
      isAutomationEnabled = fbdo.stringData() == "ON";
    }

    // Dapatkan status relay dari Firebase
    if (Firebase.RTDB.getString(&fbdo, "Relay1/pumpState")) {
      pumpState1 = fbdo.stringData();
      digitalWrite(RELAY1_PIN, pumpState1 == "ON" ? LOW : HIGH);  // ON: LOW, OFF: HIGH
    }

    if (Firebase.RTDB.getString(&fbdo, "Relay2/pumpState")) {
      pumpState2 = fbdo.stringData();
      digitalWrite(RELAY2_PIN, pumpState2 == "ON" ? LOW : HIGH);  
    }

    if (Firebase.RTDB.getString(&fbdo, "Relay3/pumpState")) {  // Relay 3
      pumpState3 = fbdo.stringData();
      digitalWrite(RELAY3_PIN, pumpState3 == "ON" ? LOW : HIGH);  
    }

    // Kirim data sensor ke Firebase
    Firebase.RTDB.setInt(&fbdo, "SoilMoisture", soilMoisture);
    Firebase.RTDB.setFloat(&fbdo, "DHT/humidity", humidity);
    Firebase.RTDB.setFloat(&fbdo, "DHT/temperature", temperature);

    // Tampilkan status relay di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("P-Kebun: ");
    lcd.print(pumpState1);
    
    lcd.setCursor(0, 1);
    lcd.print("P-Irigasi: ");
    lcd.print(pumpState2);

    delay(2000);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("P-Pupuk: ");
    lcd.print(pumpState3);

    delay(2000);
    
    // Tambahkan status otomatisasi di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Otomatisasi: ");
    lcd.print(isAutomationEnabled ? "ON" : "OFF");
    
    delay(2000);

    // Otomatisasi pompa
    if (isAutomationEnabled) {
      if (temperature > 32) {
        digitalWrite(RELAY1_PIN, LOW);  // Nyalakan relay 1 (pompa kebun)
      } else {
        digitalWrite(RELAY1_PIN, HIGH); // Matikan relay 1
      }

      if (soilMoisture > 900) {
        digitalWrite(RELAY2_PIN, LOW);  // Nyalakan relay 2 (pompa irigasi)
      } else {
        digitalWrite(RELAY2_PIN, HIGH); // Matikan relay 2
      }
    }
  }
}
