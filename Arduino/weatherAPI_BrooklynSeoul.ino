#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

// --- WiFi 설정 --- // WiFi configuration
char ssid[] = "wifi_id";
char pass[] = "wifi_password";

// --- OpenWeatherMap API 설정 --- // API configuration
const char server[] = "api.openweathermap.org";
String apiKey = "API_key"; // OpenWeatherMap API key
int port = 443;

// --- LED 핀 설정 --- // LED pin setup
#define LED_COUNT 5
int brooklynPins[LED_COUNT] = {3, 4, 5, 6, 7}; // Brooklyn LEDs
int seoulPins[LED_COUNT] = {8, 9, 10, 11, 12}; // Seoul LEDs

// --- 일출/일몰 시간 저장 --- // Sunrise/Sunset time storage
unsigned long sunriseBrooklyn = 0;
unsigned long sunsetBrooklyn = 0;
unsigned long sunriseSeoul = 0;
unsigned long sunsetSeoul = 0;

// --- NTP 설정 --- // NTP time sync
WiFiUDP udp;
const char *ntpServer = "pool.ntp.org";
const unsigned int localPort = 2390;

// === 초기 설정 === // Setup
void setup()
{
  Serial.begin(9600);
  connectToWiFi(); // WiFi 연결 // Connect to WiFi

  // 모든 LED 핀 출력으로 설정 // Set all LED pins as OUTPUT
  for (int i = 0; i < LED_COUNT; i++)
  {
    pinMode(brooklynPins[i], OUTPUT);
    pinMode(seoulPins[i], OUTPUT);
  }

  // 브루클린, 서울 일출/일몰 시간 받아오기 // Fetch sunrise/sunset times for both cities
  getSunTime("40.68", "-73.94", sunriseBrooklyn, sunsetBrooklyn, "Brooklyn");
  getSunTime("37.57", "126.97", sunriseSeoul, sunsetSeoul, "Seoul");

  Serial.println("[NTP] 준비 완료 / Ready for time sync");
}

// === 메인 반복 === // Main loop
void loop()
{
  unsigned long currentTime = getUTCTime(); // 현재 시간 동기화 // Get current UTC time

  if (currentTime == 0)
  {
    Serial.println("[오류] NTP 시간 실패 / NTP time fetch failed. Retrying...");
    delay(5000);
    return;
  }

  updateSky("Brooklyn", brooklynPins, sunriseBrooklyn, sunsetBrooklyn, currentTime);
  updateSky("Seoul", seoulPins, sunriseSeoul, sunsetSeoul, currentTime);

  Serial.println("------");
  delay(10000); // 10초마다 업데이트 // Update every 10 seconds
}

// === WiFi 연결 함수 === // WiFi connection
void connectToWiFi()
{
  Serial.print("[WiFi] 연결 중 / Connecting to: ");
  Serial.println(ssid);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\n[WiFi] 연결 완료 / Connected!");
  Serial.print("[WiFi] IP 주소 / IP Address: ");
  Serial.println(WiFi.localIP());

  udp.begin(localPort); // NTP 요청 위한 UDP 시작 // Start UDP for NTP
}

// === NTP 시간 가져오기 === // Get current UTC time via NTP
unsigned long getUTCTime()
{
  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);
  packetBuffer[0] = 0b11100011; // NTP 요청 코드 // NTP request byte

  udp.beginPacket(ntpServer, 123);
  udp.write(packetBuffer, 48);
  udp.endPacket();

  delay(1000); // 응답 대기 // Wait for response

  int size = udp.parsePacket();
  if (size >= 48)
  {
    udp.read(packetBuffer, 48);
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    unsigned long epoch = secsSince1900 - 2208988800UL; // UNIX epoch 기준으로 변환 // Convert to UNIX time
    return epoch;
  }
  else
  {
    return 0;
  }
}

// === OpenWeatherMap으로부터 일출/일몰 시간 받아오기 ===
// Fetch sunrise and sunset time from OpenWeatherMap
void getSunTime(String lat, String lon, unsigned long &sunrise, unsigned long &sunset, String city)
{
  WiFiSSLClient wifiTemp;
  HttpClient localClient = HttpClient(wifiTemp, server, port);

  String path = "/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&units=metric&appid=" + apiKey;
  Serial.println("[" + city + "] API 요청 중 / Fetching weather data...");
  localClient.get(path);

  int statusCode = localClient.responseStatusCode();
  String response = localClient.responseBody();

  Serial.println("[" + city + "] 응답 코드 / Status code: " + String(statusCode));
  if (statusCode != 200)
  {
    Serial.println("[" + city + "] 요청 실패 / Request failed");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, response);
  if (error)
  {
    Serial.print("[" + city + "] JSON 파싱 실패 / JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  sunrise = doc["sys"]["sunrise"];
  sunset = doc["sys"]["sunset"];

  Serial.println("[" + city + "] Sunrise: " + String(sunrise));
  Serial.println("[" + city + "] Sunset: " + String(sunset));
}

// === LED 상태 업데이트 ===
// Update LED status based on sun position
void updateSky(String cityName, int pins[], unsigned long sunrise, unsigned long sunset, unsigned long currentTime)
{
  Serial.println("[" + cityName + "] 현재 UTC 시간 / Current UTC time: " + String(currentTime));
  Serial.println("[" + cityName + "] Sunrise: " + String(sunrise));
  Serial.println("[" + cityName + "] Sunset: " + String(sunset));

  if (sunrise == 0 || sunset == 0)
    return;

  if (currentTime < sunrise || currentTime > sunset)
  {
    Serial.println("[" + cityName + "] 밤 / Night - LED OFF");
    for (int i = 0; i < LED_COUNT; i++)
    {
      digitalWrite(pins[i], LOW);
    }
    return;
  }

  int sunPos = map(currentTime - sunrise, 0, sunset - sunrise, 0, LED_COUNT - 1);
  sunPos = constrain(sunPos, 0, LED_COUNT - 1);
  Serial.println("[" + cityName + "] 낮 / Day - 태양 위치 LED " + String(sunPos));

  for (int i = 0; i < LED_COUNT; i++)
  {
    digitalWrite(pins[i], i == sunPos ? HIGH : LOW);
  }
}
