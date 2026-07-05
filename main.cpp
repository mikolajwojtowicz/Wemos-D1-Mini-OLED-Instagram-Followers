#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <memory>

// --- USER CONFIGURATION ---
const char *ssid = "WiFi SSID";
const char *password = "WiFi Password";
const char *instagram_username = "Instagram Username";
const char *rapidapi_key = "rapidapi API KEY";
const char *rapidapi_host = "easy-instagram-service.p.rapidapi.com";
const char *rapidapi_path = "/username";

// ~4.3k requests/month, safe under 5k limit
const unsigned long checkIntervalMs = 600000;
const uint8_t apiRetries = 3;

// --- OLED HAT 0.66" CONFIGURATION ---
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 48
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

unsigned long lastCheckTime = 0;
long lastFollowers = -9999;

void drawCentered(const String &text, int y, uint8_t textSize)
{
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(textSize);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - static_cast<int>(w)) / 2;
  if (x < 0)
  {
    x = 0;
  }
  display.setCursor(x, y);
  display.print(text);
}

void showStatus(const String &line1, const String &line2)
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawCentered(line1, 8, 1);
  drawCentered(line2, 24, 1);
  display.display();
}

long extractFollowersFromJson(const DynamicJsonDocument &doc)
{
  if (doc["follower"].is<long>())
  {
    return doc["follower"].as<long>();
  }
  if (doc["followers"].is<long>())
  {
    return doc["followers"].as<long>();
  }
  if (doc["follower_count"].is<long>())
  {
    return doc["follower_count"].as<long>();
  }
  if (doc["data"]["follower_count"].is<long>())
  {
    return doc["data"]["follower_count"].as<long>();
  }
  if (doc["data"]["user"]["follower_count"].is<long>())
  {
    return doc["data"]["user"]["follower_count"].as<long>();
  }
  if (doc["result"]["follower_count"].is<long>())
  {
    return doc["result"]["follower_count"].as<long>();
  }

  return -4;
}

long extractFollowerFastFromPayload(const String &payload)
{
  const char *keys[] = {"\"follower\"", "\"followers\"", "\"follower_count\""};
  size_t payloadLen = payload.length();

  for (size_t i = 0; i < 3; i++)
  {
    int keyPos = payload.indexOf(keys[i]);
    if (keyPos < 0)
    {
      continue;
    }

    int colonPos = payload.indexOf(':', keyPos);
    if (colonPos < 0)
    {
      continue;
    }

    size_t p = static_cast<size_t>(colonPos + 1);
    while (p < payloadLen && (payload.charAt(p) == ' ' || payload.charAt(p) == '\t' || payload.charAt(p) == '"'))
    {
      p++;
    }

    size_t start = p;
    if (p < payloadLen && payload.charAt(p) == '-')
    {
      p++;
    }

    while (p < payloadLen && payload.charAt(p) >= '0' && payload.charAt(p) <= '9')
    {
      p++;
    }

    if (p > start)
    {
      return payload.substring(start, p).toInt();
    }
  }

  return -4;
}

long parseFollowersFromPayload(const String &payload)
{
  long fastFollower = extractFollowerFastFromPayload(payload);
  if (fastFollower >= 0)
  {
    return fastFollower;
  }

  return -4;
}

long fetchFollowersHttpClientFallback(const String &uri)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure());
  HTTPClient http;

  client->setInsecure();
  client->setTimeout(15000);

  if (!http.begin(*client, rapidapi_host, 443, uri, true))
  {
    Serial.println("HTTP begin failed.");
    return -1;
  }

  http.setReuse(false);
  http.setTimeout(15000);
  http.addHeader("x-rapidapi-key", rapidapi_key);
  http.addHeader("x-rapidapi-host", rapidapi_host);
  http.addHeader("Accept", "application/json");
  http.addHeader("Connection", "close");

  int httpCode = http.GET();
  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode <= 0)
  {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(httpCode).c_str());
    http.end();
    return -1;
  }

  String payload = http.getString();
  http.end();

  if (payload.length() == 0)
  {
    Serial.println("Empty body");
    return -3;
  }

  if (httpCode != HTTP_CODE_OK)
  {
    Serial.println("\n--- HTTP ERROR PAYLOAD ---");
    Serial.println(payload);
    Serial.println("--------------------------\n");
    return -2;
  }

  return parseFollowersFromPayload(payload);
}

long getInstagramFollowers(const char *username)
{
  String uri = String(rapidapi_path) + "?username=" + String(username) + "&random=" + String(millis());

  for (uint8_t attempt = 1; attempt <= apiRetries; attempt++)
  {
    Serial.printf("\nStarting RapidAPI request (attempt %u/%u)...\n", attempt, apiRetries);
    long followers = fetchFollowersHttpClientFallback(uri);
    if (followers >= 0)
    {
      return followers;
    }

    if (attempt < apiRetries)
    {
      delay(1200);
    }
  }

  return -1;
}

String formatFollowers(long followersCount)
{
  if (followersCount < 10000)
  {
    return String(followersCount);
  }

  if (followersCount < 100000)
  {
    long thousands = followersCount / 1000;
    long decimal = (followersCount % 1000) / 100;
    return String(thousands) + "." + String(decimal) + "K";
  }

  long roundedThousands = (followersCount + 500) / 1000;
  return String(roundedThousands) + "K";
}

void renderFollowerCount(long followersCount)
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (followersCount >= 0)
  {
    String value = formatFollowers(followersCount);
    uint8_t size = (value.length() >= 6) ? 1 : 2;
    drawCentered(value, 28, size);
  }
  else
  {
    drawCentered("ERR", 28, 2);
    drawCentered(String(followersCount), 36, 1);
  }

  display.display();
}

void ensureWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  showStatus("WiFi", "reconnect...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000)
  {
    delay(250);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  }

  showStatus("Boot", "WiFi...");

  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  showStatus("WiFi", "connected");
  delay(700);
}

void loop()
{
  ensureWifi();

  if (lastCheckTime == 0 || millis() - lastCheckTime >= checkIntervalMs)
  {
    lastCheckTime = millis();
    showStatus("RapidAPI", "updating...");
    lastFollowers = getInstagramFollowers(instagram_username);

    if (lastFollowers >= 0)
    {
      Serial.print("Followers: ");
      Serial.println(lastFollowers);
    }
    else
    {
      Serial.print("Followers fetch error code: ");
      Serial.println(lastFollowers);
    }

    renderFollowerCount(lastFollowers);
  }

  delay(50);
}
