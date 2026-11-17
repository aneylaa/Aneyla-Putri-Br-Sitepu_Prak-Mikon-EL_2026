#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Jangan lupa isi dengan nama dan password WiFi kamu
const char* ssid = "Pret";
const char* password = "nela1234";

// URL untuk mengambil data JSON gempa dari BMKG
const String url = "https://data.bmkg.go.id/DataMKG/TEWS/autogempa.json";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  
  // Perbaikan: Memasukkan delay dan print ke dalam kurung kurawal while
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Perbaikan: Memperbaiki karakter aneh $IP=^{\prime\prime}$
  Serial.print(" OK! IP=");
  Serial.println(WiFi.localIP());
  
  Serial.print("Fetching " + url + "... ");
  
  HTTPClient http;
  http.begin(url);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("HTTP ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println();
    
    // Menampilkan data gempa berformat JSON
    Serial.println(payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.println(":-(");
  }
  
  http.end(); // Menutup koneksi HTTP
}

void loop() {
  delay(100);
}