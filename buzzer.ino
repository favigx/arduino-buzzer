#include <ArduinoHttpClient.h>
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include "settings.h"

char ssid[] = SECRET_SSID;
char password[] = SECRET_PASSWORD;
char serverAdress[] = "";
int port = 8080;

WiFiClient wifi;
HttpClient client = HttpClient(wifi, serverAdress, port);

WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 0, 60000);

TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 60};
Timezone tz(CEST, CET);

const int greenLedPin = 7;
const int yellowLedPin = 4;
const int redLedPin = 8;
const int buzzerPin = 2;
const int trigPin = 12;
const int echoPin = 13;

long duration;
float distance;
int buzzCount = 0;
bool buzzerActivated = false;

unsigned long lastSendTime = 0;
const unsigned long interval = 3600000;

void setup() {
  pinMode(greenLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(9600);

  Serial.println("Ansluter till Wifi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(500);
  }
  Serial.println("Ansluten till Wifi");

  timeClient.begin();
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration * 0.0343) / 2;

  if (distance <= 10.0) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
    if (!buzzerActivated) {
      buzzCount++;
      buzzerActivated = true;
    }
  } else if (distance <= 20.0) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
    if (!buzzerActivated) {
      buzzCount++;
      buzzerActivated = true;
    }
  } else {
    digitalWrite(buzzerPin, LOW);   
    digitalWrite(redLedPin, LOW);    
    digitalWrite(yellowLedPin, LOW); 
    digitalWrite(greenLedPin, HIGH);
    buzzerActivated = false;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= interval) {
    lastSendTime = currentMillis;
    sendBuzzerData(buzzCount, distance);
    buzzCount = 0;
  }

  timeClient.update();
  delay(100);
}

void sendBuzzerData(int buzzCount, float distance) {
  StaticJsonDocument<200> doc;

  time_t utc = timeClient.getEpochTime();

  time_t localTime = tz.toLocal(utc);

  String timestamp = String(hour(localTime)) + ":" + (minute(localTime) < 10 ? "0" : "") + String(minute(localTime)) + ":" + (second(localTime) < 10 ? "0" : "") + String(second(localTime));
  String date = String(year(localTime)) + "-" + (month(localTime) < 10 ? "0" : "") + String(month(localTime)) + "-" + (day(localTime) < 10 ? "0" : "") + String(day(localTime));

  doc["timestamp"] = timestamp;  
  doc["date"] = date;
  doc["buzzCount"] = buzzCount;

  String output;
  serializeJson(doc, output);

  Serial.println("Skickar data till backend: ");
  Serial.println(output);

  client.beginRequest();
  client.post("/buzzerdata");

  client.sendHeader("Content-Type", "application/json");
  client.sendHeader("Content-Length", output.length());

  client.beginBody();
  client.print(output);
  client.endRequest();

  int statusCode = client.responseStatusCode();
  String response = client.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  delay(2000);
}
