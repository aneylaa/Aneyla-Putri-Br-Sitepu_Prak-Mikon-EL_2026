#include <Arduino.h>
#include <ESP32Servo.h>

#define SERVO_PIN 25

Servo myServo;

void setup() {
  Serial.begin(115200);

  // Alokasi timer PWM ESP32 untuk Servo
  ESP32PWM::allocateTimer(0);
  
  // Set frekuensi standar servo (50Hz)
  myServo.setPeriodHertz(50);
  
  // Hubungkan GPIO 25 ke servo dengan rentang pulsa 500us - 2400us
  myServo.attach(SERVO_PIN, 500, 2400);

  Serial.println("Sistem Servo GPIO 25 Siap!");
}

void loop() {
  // Posisi 0 Derajat
  Serial.println("Sudut: 0");
  myServo.write(0);
  delay(1500);

  // Posisi 90 Derajat
  Serial.println("Sudut: 90");
  myServo.write(90);
  delay(1500);

  // Posisi 180 Derajat
  Serial.println("Sudut: 180");
  myServo.write(180);
  delay(1500);
}