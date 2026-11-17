#include <Arduino.h>
#include <WiFi.h>

// Ubah menggunakan nama dan password WiFi kamu
const char* ssid = "Pret";
const char* password = "nela1234";

WiFiServer server(80);

String header;
String output37State = "off";
String output38State = "off";

// Variabel pin diganti ke 37 dan 38
const int output37 = 37;
const int output38 = 38;

unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

void setup() {
  Serial.begin(115200);
  
  pinMode(output37, OUTPUT);
  pinMode(output38, OUTPUT);
  digitalWrite(output37, LOW);
  digitalWrite(output38, LOW);
  
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  
  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");
    String currentLine = "";
    
    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Logika kontrol disesuaikan ke pin 37 dan 38
            if (header.indexOf("GET /37/on") >= 0) {
              Serial.println("GPIO 37 on");
              output37State = "on";
              digitalWrite(output37, HIGH);
            } else if (header.indexOf("GET /37/off") >= 0) {
              Serial.println("GPIO 37 off");
              output37State = "off";
              digitalWrite(output37, LOW);
            } else if (header.indexOf("GET /38/on") >= 0) {
              Serial.println("GPIO 38 on");
              output38State = "on";
              digitalWrite(output38, HIGH);
            } else if (header.indexOf("GET /38/off") >= 0) {
              Serial.println("GPIO 38 off");
              output38State = "off";
              digitalWrite(output38, LOW);
            }
            
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Tampilan di web disesuaikan ke GPIO 37
            client.println("<p>GPIO 37 - State " + output37State + "</p>");
            if (output37State == "off") {
              client.println("<p><a href=\"/37/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/37/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            
            // Tampilan di web disesuaikan ke GPIO 38
            client.println("<p>GPIO 38 - State " + output38State + "</p>");
            if (output38State == "off") {
              client.println("<p><a href=\"/38/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/38/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}