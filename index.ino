#include <DHT.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <stdlib.h>
#include <Wire.h>
#include "OzOLED.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

#define DHTPIN D7
#define DHTTYPE DHT22
#define DEBUG true

WiFiClient client;
DHT dht(DHTPIN, DHTTYPE);

// NTP 클라이언트 설정
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String apiKey30 = "***********"; //내 서버의 주소
String apiKey360 = "***********";
String apiKey1800 = "***********";
String apiKey3960 = "***********";
const char* ssid = "***********";
const char* password = "***********";
const char* server = "api.thingspeak.com";

int reset = D5;

extern volatile unsigned long timer0_millis;

int  err_count, second_30_count, minute_6_count, minute_30_count, minute_66_count;
float h, t;
float second_30_total_H, second_30_total_T;
float minute_6_total_H, minute_6_total_T;
float minute_30_total_H, minute_30_total_T;
float minute_66_total_H, minute_66_total_T;

unsigned long arduino_previous_time = 0;
unsigned long arduino_interval_time = 990;

unsigned long current_NTP_time = 0;
unsigned long previous_NTP_time_5 = 0;
unsigned long previous_NTP_time_30 = 0;
unsigned long previous_NTP_time_360 = 0;
unsigned long previous_NTP_time_1800 = 0;
unsigned long previous_NTP_time_3960 = 0;
unsigned long offset_NTP_time = 0;

String formatted_NTP_time = "";



void setup()
{
  Serial.begin(115200);
  Wire.begin(D2, D1); // D2 = SDA, D1 = SCL
  OzOled.init();  // OLED 초기화
  delay(10);
  dht.begin();

  WiFi.begin(ssid, password);
  Serial.println();
  Serial.print("와이파이 ");
  Serial.print(ssid);
  Serial.print("와 연결중");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("연결 완료");
  digitalWrite(reset, HIGH);
  pinMode(reset, OUTPUT);

  // NTP 클라이언트 시작
  timeClient.begin();
  // 시간대 설정 (예: GMT + 9시간)
  timeClient.setTimeOffset(3600 * 9);
  timeClient.update();
  offset_NTP_time = timeClient.getEpochTime();
}

void loop()
{
  unsigned long arduino_current_time = millis();

  // 0.99초마다 서버시간 받아옴
  if (arduino_current_time - arduino_previous_time > arduino_interval_time) {
    arduino_previous_time = arduino_current_time;
    timeClient.update();
    // 21:43:22 형식
    formatted_NTP_time = String(timeClient.getFormattedTime());
    // 1706368543 형식
    current_NTP_time = timeClient.getEpochTime();
    OzOled.printString(formatted_NTP_time.c_str(), 4, 1);

    String current_NTP_time_str = String(current_NTP_time);
    String err_count_str = "Err : " + String(err_count);
    OzOled.printString(current_NTP_time_str.c_str(), 3, 6);
    OzOled.printString(err_count_str.c_str(), 7, 7);
  }

  // 5초 마다 온도 받아옴
  if ((current_NTP_time - offset_NTP_time) % 5 == 0 && current_NTP_time - previous_NTP_time_5 >= 1 ) {
    previous_NTP_time_5 = current_NTP_time;

    t = dht.readTemperature();
    h = dht.readHumidity();
    if (isnan(h) || isnan(t))
    {
      err_count += 1;
      Serial.print("Failed to read from DHT sensor! err_count : ");
      Serial.println(err_count);
      Serial.println(t);
      Serial.println(h);
      return;
    }

    String tStr = "t : " + String(t) + "C";
    String hStr = "h : " + String(h) + "%";

    OzOled.printString(tStr.c_str(), 3, 3);  // 문자열로 변환된 온도 값과 "°C"
    OzOled.printString(hStr.c_str(), 3, 4);  // 문자열로 변환된 습도 값과 "%"

    second_30_total_T = second_30_total_T + t;
    minute_6_total_T = minute_6_total_T + t;
    minute_30_total_T = minute_30_total_T + t;
    minute_66_total_T = minute_66_total_T + t;

    second_30_total_H = second_30_total_H + h;
    minute_6_total_H = minute_6_total_H + h;
    minute_30_total_H = minute_30_total_H + h;
    minute_66_total_H = minute_66_total_H + h;

    second_30_count = second_30_count + 1;
    minute_6_count = minute_6_count + 1;
    minute_30_count = minute_30_count + 1;
    minute_66_count = minute_66_count + 1;

  }

  // 30초마다 서버
  if (second_30_count > 0 && (current_NTP_time - offset_NTP_time) % 30 == 0 && current_NTP_time - previous_NTP_time_30 >= 1) {

    String postStr30;
    float second_30_result_T = second_30_total_T / second_30_count;
    float second_30_result_H = second_30_total_H / second_30_count;

    if (client.connect(server, 80)) {
      previous_NTP_time_30 = current_NTP_time;

      postStr30 += apiKey30;
      postStr30 += "&field1=";
      postStr30 += String(second_30_result_T);
      postStr30 += "&field2=";
      postStr30 += String(second_30_result_H);
      postStr30 += "\r\n\r\n";

      Serial.print("T/30s : ");
      Serial.println(second_30_result_T);
      Serial.print("H/30s : ");
      Serial.println(second_30_result_H);

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey30 + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr30.length());
      client.print("\n\n");
      client.print(postStr30);
      second_30_count = 0;
      second_30_total_T = 0;
      second_30_total_H = 0;
      err_count = 0;
      delay(100);
    }
    else {
      err_count += 1;
      delay(100);
      Serial.println("30초때 에러");
    }
    client.stop();
  }

  // 6분마다 서버
  if (minute_6_count > 0 && (current_NTP_time - offset_NTP_time) % 359 == 0 && current_NTP_time - previous_NTP_time_360 >= 1) {
    delay(1000);
    String postStr360;
    float minute_6_result_T = minute_6_total_T / minute_6_count;
    float minute_6_result_H = minute_6_total_H / minute_6_count;

    if (client.connect(server, 80)) {
      previous_NTP_time_360 = current_NTP_time;

      postStr360 += apiKey360;
      postStr360 += "&field1=";
      postStr360 += String(minute_6_result_T);
      postStr360 += "&field2=";
      postStr360 += String(minute_6_result_H);
      postStr360 += "\r\n\r\n";

      Serial.print("T/6m : ");
      Serial.println(minute_6_result_T);
      Serial.print("H/6m : ");
      Serial.println(minute_6_result_H);

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey360 + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr360.length());
      client.print("\n\n");
      client.print(postStr360);
      minute_6_count = 0;
      minute_6_total_T = 0;
      minute_6_total_H = 0;
      err_count = 0;
      delay(100);
    }
    else {
      err_count += 1;
      delay(100);
      Serial.println("6분때 에러");
    }
    client.stop();
  }

  // 30분마다 서버
  if (minute_30_count > 0 && (current_NTP_time - offset_NTP_time) % 1799 == 0 && current_NTP_time - previous_NTP_time_1800 >= 1) {
    delay(1000);
    String postStr1800;
    float minute_30_result_T = minute_30_total_T / minute_30_count;
    float minute_30_result_H = minute_30_total_H / minute_30_count;

    if (client.connect(server, 80)) {
      previous_NTP_time_1800 = current_NTP_time;

      postStr1800 += apiKey1800;
      postStr1800 += "&field1=";
      postStr1800 += String(minute_30_result_T);
      postStr1800 += "&field2=";
      postStr1800 += String(minute_30_result_H);
      postStr1800 += "\r\n\r\n";

      Serial.print("H/30m : ");
      Serial.println(minute_30_result_H);
      Serial.print("T/30m : ");
      Serial.println(minute_30_result_T);

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey1800 + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr1800.length());
      client.print("\n\n");
      client.print(postStr1800);
      minute_30_count = 0;
      minute_30_total_T = 0;
      minute_30_total_H = 0;
      err_count = 0;
      delay(100);
    }
    else {
      err_count += 1;
      delay(100);
      Serial.println("30분때 에러");
    }
    client.stop();
  }

  // 66분 마다 서버
  if (minute_66_count > 0 && (current_NTP_time - offset_NTP_time) % 3959 == 0 && current_NTP_time - previous_NTP_time_3960 >= 1) {
    delay(1000);
    String postStr3960;
    float minute_66_result_T = minute_66_total_T / minute_66_count;
    float minute_66_result_H = minute_66_total_H / minute_66_count;

    if (client.connect(server, 80)) {
      previous_NTP_time_3960 = current_NTP_time;

      postStr3960 += apiKey3960;
      postStr3960 += "&field1=";
      postStr3960 += String(minute_66_result_T);
      postStr3960 += "&field2=";
      postStr3960 += String(minute_66_result_H);
      postStr3960 += "\r\n\r\n";

      Serial.print("T/66m : ");
      Serial.println(minute_66_result_T);
      Serial.print("H/66m : ");
      Serial.println(minute_66_result_H);

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + apiKey3960 + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr3960.length());
      client.print("\n\n");
      client.print(postStr3960);
      minute_66_count = 0;
      minute_66_total_T = 0;
      minute_66_total_H = 0;
      err_count = 0;
      delay(100);
    }
    else {
      err_count += 1;
      delay(100);
      Serial.println("66분때 에러");
    }
    client.stop();
  }


  if (arduino_current_time >= 4294957294 || err_count > 100) { //49일후 타이머 초기화
    digitalWrite(reset, LOW);
  }

}