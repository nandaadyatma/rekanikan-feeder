#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "RTClib.h"
#include <Servo.h>
#include <ArduinoJson.h>
#include <TimeLib.h>  // tambahkan pustaka TimeLib

RTC_DS3231 rtc;
Servo myservo;

//Feeder Unique ID
String feederId = "ad12321";

//NTP Server untuk sinkronisasi waktu
const char *ntpServer = "pool.ntp.org";
const int timeZone = 4;

WiFiUDP udp;
unsigned int localPort = 8888;  // Port UDP lokal yang digunakan

unsigned long previousMillis = 0;
const long interval = 60000;
unsigned long previousTime = 0;

int hourSchedule1 = 0;
int minuteSchedule1 = 0;
int portionSchedule1 = 0;

int hourSchedule2 = 0;
int minuteSchedule2 = 0;
int portionSchedule2 = 0;

// For HTTPS requests
WiFiClientSecure client;


//Base of the URL
#define TEST_HOST "rekanikan-dev2-kyrz52in2q-et.a.run.app"


void setup() {
  Serial.begin(115200);


#ifndef ESP8266
  while (!Serial)
    ;  // wait for serial port to connect. Needed for native USB
#endif

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }


  // Connect to the WiFI
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  connectToWiFi();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  //--------

  // If you don't need to check the fingerprint
  client.setInsecure();

  // If you want to check the fingerprint
  // client.setFingerprint(TEST_HOST_FINGERPRINT);

  makeHTTPRequest();

  // Mulai sinkronisasi waktu menggunakan NTP
  configTime(timeZone * 3600, 0, ntpServer);

  // Tunggu hingga waktu terbaca dari server NTP
  while (!time(nullptr)) {
    Serial.println("Menunggu sinkronisasi waktu...");
    delay(1000);
  }
  Serial.println("Waktu terbaca dari server NTP: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));
}

void makeHTTPRequest() {

  // Opening connection to server (Use 80 as port if HTTP)
  if (!client.connect(TEST_HOST, 443)) {
    Serial.println(F("Connection failed"));
    return;
  }

  // give the esp a breather
  yield();

  // Send HTTP request
  client.print(F("GET "));
  // This is the second half of a request (everything that comes after the base URL)
  client.print("/schedule/get/" + feederId);  // %2C == ,
  client.println(F(" HTTP/1.1"));

  //Headers
  client.print(F("Host: "));
  client.println(TEST_HOST);
  client.println(F("Cache-Control: no-cache"));
  client.println(F("Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOiJ1c2VyLTF0RmZSUncyZ0tvaHM0S1ciLCJpYXQiOjE2OTc4NzY2NjZ9.r4OiRminXLAMVuYR0s7rmf3OkyxyMC3zIyHQEzZeXlU"));


  if (client.println() == 0) {
    Serial.println(F("Failed to send request"));
    return;
  }
  //delay(100);
  // Check HTTP status
  char status[32] = { 0 };
  client.readBytesUntil('\r', status, sizeof(status));
  if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
    Serial.print(F("Unexpected response: "));
    Serial.println(status);
    return;
  }

  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return;
  }


  while (client.available() && client.peek() != '{') {
    char c = 0;
    client.readBytes(&c, 1);
    Serial.print(c);
    Serial.println("BAD");
  }

  //  // While the client is still availble read each
  //  // byte and print to the serial monitor
  //  while (client.available()) {
  //    char c = 0;
  //    client.readBytes(&c, 1);
  //    Serial.print(c);
  //  }

  //Use the ArduinoJson Assistant to calculate this:

  //StaticJsonDocument<192> doc;
  DynamicJsonDocument doc(8192);  // Lebih besar untuk menampung respon yang lebih besar

  DeserializationError error = deserializeJson(doc, client);

  if (!error) {
    JsonArray feedingSchedule = doc["data"];

    JsonObject schedule1 = feedingSchedule[0];
    JsonObject schedule2 = feedingSchedule[1];

    int hour1 = schedule1["hour"];
    int minute1 = schedule1["minute"];
    int portion1 = schedule1["portion"];

    int hour2 = schedule2["hour"];
    int minute2 = schedule2["minute"];
    int portion2 = schedule2["portion"];

    hourSchedule1 = hour1;
    minuteSchedule1 = minute1;
    portionSchedule1 = portion1;

    hourSchedule2 = hour2;
    minuteSchedule2 = minute2;
    portionSchedule2 = portion2;

  } else {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
}

void loop() {
  // Dapatkan waktu saat ini
  time_t now = time(nullptr);

  // Cetak waktu saat ini setiap 1 menit
  if (second(now) == 0) {
    Serial.print((hour(now) + 8)%24);
    Serial.print(":");  
    printDigits(minute(now));
    Serial.print(":");  
    printDigits(second(now));

    // Cetak jam jadwal
    Serial.println();
    Serial.print("Schedule1: ");
    printDigits(hourSchedule1);
    Serial.print(":");  
    printDigits(minuteSchedule1);
    Serial.print(" Takaran pakan:");  
    Serial.print(portionSchedule1);
    Serial.print(" gram");   
    Serial.println();

    Serial.print("Schedule2: ");
    printDigits(hourSchedule2);
    Serial.print(":");  
    printDigits(minuteSchedule2);
    Serial.print(" Takaran pakan:");  
    Serial.print(portionSchedule2);
    Serial.print(" gram");    
    Serial.println();

    Serial.print("Temperature: ");
    Serial.print(rtc.getTemperature());
    Serial.println(" C");

    // Periksa sesuai jadwal 1 di api
    if ((hour(now) + 8) == hourSchedule1 && minute(now) == minuteSchedule1) {
      Serial.println("Pakan diberikan:" + portionSchedule1); 
      Serial.print(" gram");
      int buzzerPin = D5;          // Ubah ke pin yang sesuai dengan koneksi buzzer Anda

      for (int i = 0; i <= 3; i++) {
        tone(buzzerPin, 1000);  // Suara buzzer dengan frekuensi 1000 Hz
        delay(500);             // Jeda 1 detik
        noTone(buzzerPin);      // Matikan bunyi buzzer
        delay(500);             // Jeda 1 detik
      }


      myservo.attach(0);

      int pos;
      myservo.attach(0);   // attaches the servo on GIO2 to the servo object
      myservo.write(180);  // tell servo to go to position in variable 'pos'
      delay(200 * portionSchedule1);   // waits 15ms for the servo to reach the position
      
      myservo.detach();
    }

    // Periksa sesuai jadwal 2 di api
    if ((hour(now) + 8) == hourSchedule2 && minute(now) == minuteSchedule2) {
      Serial.println("Pakan diberikan:" + portionSchedule2); 
      Serial.print(" gram");
      int buzzerPin = D5;          // Ubah ke pin yang sesuai dengan koneksi buzzer Anda

      for (int i = 0; i <= 3; i++) {
        tone(buzzerPin, 1000);  // Suara buzzer dengan frekuensi 1000 Hz
        delay(500);             // Jeda 1 detik
        noTone(buzzerPin);      // Matikan bunyi buzzer
        delay(500);             // Jeda 1 detik
      }


      myservo.attach(0);

      int pos;
      myservo.attach(0);   // attaches the servo on GIO2 to the servo object
      myservo.write(180);  // tell servo to go to position in variable 'pos'
      delay(200 * portionSchedule2);   // waits 15ms for the servo to reach the position
      
      myservo.detach();
    }

    if ((minute(now) % 1) == 0) {
      makeHTTPRequest();
    }
  }



  delay(1000);  // Tunggu 1 detik
}

void connectToWiFi() {
  //wifi & password
  const char *ssid = "AndroidAP6604";
  const char *password = "baikhati";

  Serial.println();
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  int buzzerPin = D5;
  for (int i = 0; i <= 2; i++) {    
        tone(buzzerPin, 1000);  // Suara buzzer dengan frekuensi 1000 Hz
        delay(500);             // Jeda 1 detik
        noTone(buzzerPin);      // Matikan bunyi buzzer
        delay(100);             // Jeda 1 detik
      }
}

void printDigits(int digits) {
  // Fungsi untuk mencetak digit waktu dengan format dua digit (00-59)
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}