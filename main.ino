#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <Ticker.h>
#include <time.h>

#define RELAY_PIN D1
#define RESET_BUTTON 0  
#define LED_WATERING D5  // Полив (led)
#define LED_WIFI D7       // Потеря соединения с Wi-fi (led)

const String botToken = "Токен бота ТГ";
const String chatId   = "айди чата ТГ ";

int wateringHour = 8; 
int wateringMinute = 30; // автополив в 8:30

int wateringDuration = 5;  // продолжительность полива
unsigned long lastCheckTime = 0;

ESP8266WebServer server(80);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);

Ticker ledWateringBlink;
Ticker ledWifiBlink;

bool wasDisconnected = false;
time_t disconnectStart;
time_t disconnectEnd;

void blinkLED(uint8_t pin) {
  digitalWrite(pin, !digitalRead(pin));
}

String getDateTime() {
  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", timeinfo);
  return String(buffer);
}

void sendTelegramMessage(const String& msg) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(chatId, msg, "");
  }
}

void startWatering() {
  ledWateringBlink.attach_ms(200, blinkLED, LED_WATERING);
  sendTelegramMessage("\xF0\x9F\x9A\xBF Полив запущен (" + getDateTime() + ")");
  digitalWrite(RELAY_PIN, HIGH);
  delay(wateringDuration * 1000);
  digitalWrite(RELAY_PIN, LOW);
  ledWateringBlink.detach();
  digitalWrite(LED_WATERING, LOW);
  sendTelegramMessage("\xE2\x9C\x85 Полив остановлен (" + getDateTime() + ")");
}

void handleTelegram() {
  int newMsgCount = bot.getUpdates(bot.last_message_received + 1);
  while (newMsgCount) {
    for (int i = 0; i < newMsgCount; i++) {
      String text = bot.messages[i].text;
      if (text == "/start" || text == "назад") {
        String msg = "Выберите команду:\n\n";
        msg += "/water - Запустить полив\n";
        msg += "/info - Настройки полива\n";
        msg += "/check - Проверить соединение\n";
        msg += "/help - Справка по командам";
        bot.sendMessage(chatId, msg, "");
      } else if (text == "/water") {
        startWatering();
      } else if (text == "/info") {
        String info = "Настройки полива:\n";
        info += "Полив: ежедневно\n";
        info += "Время полива: " + String(wateringHour) + ":" + String(wateringMinute) + "\n";
        info += "Время работы: " + String(wateringDuration) + " секунд";
        bot.sendMessage(chatId, info, "");
      } else if (text == "/check") {
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
        digitalWrite(LED_BUILTIN, HIGH);

        if (WiFi.status() == WL_CONNECTED) {
          bot.sendMessage(chatId, "\xE2\x9C\x85 Соединение установлено ✅", "");
        } else {
          bot.sendMessage(chatId, "\xE2\x9D\x8C Соединение разорвано ❌", "");
        }
      } else if (text == "/help") {
        String helpMsg = "Команды:\n\n";
        helpMsg += "/water - Запуск полива (вручную)\n";
        helpMsg += "/info - Настройки полива\n";
        helpMsg += "/check - Проверка соединения\n";
        helpMsg += "/help - Справка по командам";
        bot.sendMessage(chatId, helpMsg, "");
      }
    }
    newMsgCount = bot.getUpdates(bot.last_message_received + 1);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  pinMode(LED_WATERING, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WATERING, LOW);
  digitalWrite(LED_WIFI, LOW);
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  if (digitalRead(RESET_BUTTON) == LOW) {
    Serial.println("Сброс WiFi настроек...");
    digitalWrite(LED_BUILTIN, LOW);
    delay(300);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(300);
    digitalWrite(LED_BUILTIN, LOW);
    delay(300);
    digitalWrite(LED_BUILTIN, HIGH);
    WiFi.disconnect(true);
    delay(1000);
    ESP.restart();
  }

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  bool res = wm.autoConnect("Poliv-Setup", "12345678");
  if (!res) {
    Serial.println("Не удалось подключиться, перезагрузка...");
    ESP.restart();
  }

  secured_client.setInsecure();
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  server.handleClient();
  handleTelegram();

  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);
  if (timeinfo->tm_hour == wateringHour && timeinfo->tm_min == wateringMinute && millis() - lastCheckTime > 60000) {
    lastCheckTime = millis();
    startWatering();
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (!wasDisconnected) {
      wasDisconnected = true;
      disconnectStart = now;
      ledWifiBlink.attach(500, blinkLED, LED_WIFI);
    }
  } else {
    if (wasDisconnected) {
      wasDisconnected = false;
      disconnectEnd = now;
      ledWifiBlink.detach();
      digitalWrite(LED_WIFI, LOW);

      char buffer[60];
      strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", localtime(&disconnectStart));
      String from = String(buffer);
      strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", localtime(&disconnectEnd));
      String to = String(buffer);

      sendTelegramMessage("\xE2\x9D\x8C Соединение разорвано с " + from + " до " + to);
    }
  }
}
