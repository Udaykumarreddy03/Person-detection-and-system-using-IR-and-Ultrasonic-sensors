#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define LED_PIN 2

// Wi-Fi credentials
const char* ssid = "DSL";
const char* password = "gprec#iot";

// Telegram Bot settings
const String botToken = "8064739614:AAHnlyYGriVl2JERLNeFgyM4KeXXDG0LbB4";
const String chatID = "5981418375";

int peopleInside = 0;

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 5000; // 5 seconds

int64_t lastUpdateID = 0; // for tracking Telegram updates

// URL encode function for inline keyboard JSON (simple)
String urlencode(const String &str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  for (size_t i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      encoded += '%';
      code0 = (c >> 4) & 0xF;
      code1 = c & 0xF;
      encoded += (code0 > 9) ? (code0 - 10 + 'A') : (code0 + '0');
      encoded += (code1 > 9) ? (code1 - 10 + 'A') : (code1 + '0');
    }
  }
  return encoded;
}

// Send message with inline keyboard buttons
void sendTelegramKeyboard() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String keyboardJson = "{\"inline_keyboard\":[[{\"text\":\"Enter\",\"callback_data\":\"enter\"},{\"text\":\"Exit\",\"callback_data\":\"exit\"}]]}";
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage";

    String postData = "chat_id=" + chatID + "&text=Select option:&reply_markup=" + urlencode(keyboardJson);

    http.begin(url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      Serial.println("Keyboard message sent");
    } else {
      Serial.print("Error sending keyboard: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

// Send simple text message
void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + message;
    http.begin(url);
    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      Serial.println("Telegram message sent successfully");
    } else {
      Serial.print("Error sending Telegram message: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

// Process incoming updates from Telegram API
void processTelegramUpdates() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + botToken + "/getUpdates?offset=" + String(lastUpdateID + 1);
    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      String payload = http.getString();
      // Parse JSON
      StaticJsonDocument<2048> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        JsonArray results = doc["result"].as<JsonArray>();
        for (JsonObject update : results) {
          lastUpdateID = update["update_id"].as<int64_t>();

          if (update.containsKey("callback_query")) {
            JsonObject callback = update["callback_query"];
            String data = callback["data"].as<String>();

            // Handle button pressed
            if (data == "enter") {
              peopleInside++;
              Serial.println("Entry button pressed");
              sendTelegramMessage("🚶‍♂️ Person Entered. Inside now: " + String(peopleInside));
              digitalWrite(LED_PIN, HIGH);
              delay(300);
              digitalWrite(LED_PIN, LOW);
            } else if (data == "exit") {
              if (peopleInside > 0) peopleInside--;
              Serial.println("Exit button pressed");
              sendTelegramMessage("🚪 Person Exited. Inside now: " + String(peopleInside));
              digitalWrite(LED_PIN, HIGH);
              delay(300);
              digitalWrite(LED_PIN, LOW);
            }

            // Answer callback query to remove "loading" circle in Telegram client
            String callbackID = callback["id"].as<String>();
            String answerURL = "https://api.telegram.org/bot" + botToken + "/answerCallbackQuery?callback_query_id=" + callbackID;
            HTTPClient httpAnswer;
            httpAnswer.begin(answerURL);
            httpAnswer.GET();
            httpAnswer.end();
          }
        }
      } else {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("Error getting updates: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");

  // Send initial keyboard message
  sendTelegramKeyboard();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdateTime > updateInterval) {
    processTelegramUpdates();
    lastUpdateTime = currentMillis;
  }
}
