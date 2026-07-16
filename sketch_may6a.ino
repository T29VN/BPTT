#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <math.h>
#include <esp_sleep.h>

#include <Adafruit_MLX90614.h>
#include "Adafruit_VL53L0X.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
//                    CẤU HÌNH FIREBASE
// ============================================================

#define FIREBASE_HOST \
  "https://datk2-39ea0-default-rtdb.asia-southeast1.firebasedatabase.app"

#define FIREBASE_AUTH \
  "YnGeHlj4syptyZOEYJnyyEey1UVIscus5cKheQwJ"

const char* DEVICE_ID = "esp32_01";

// ============================================================
//                    WIFI CẤU HÌNH ESP32
// ============================================================

const char* CONFIG_AP_SSID = "ESP32_TEMP_CONFIG";
const char* CONFIG_AP_PASS = "12345678";

IPAddress CONFIG_AP_IP(192, 168, 4, 1);
IPAddress CONFIG_AP_GATEWAY(192, 168, 4, 1);
IPAddress CONFIG_AP_SUBNET(255, 255, 255, 0);

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_RECONNECT_DELAY_MS = 5000;

// ============================================================
//                         OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool displayReady = false;

// ============================================================
//                        CẢM BIẾN
// ============================================================

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

bool sensorsReady = false;

// ============================================================
//                    HỆ SỐ HIỆU CHỈNH
// ============================================================

// Y = a*x^2 + b*x + c
const double COEFF_A =
  -0.000098456252379291384366467792496713520072808170694711;

const double COEFF_B =
  -0.0024372651873168229851349984831631105268480129436748;

const double COEFF_C =
  0.94848043163011363636363636363636363636363636363636;

const double Y_MIN_THRESHOLD = 0.01;

// ============================================================
//                   TRẠNG THÁI HỆ THỐNG
// ============================================================

enum SystemState {
  STATE_CONFIG,
  STATE_RUNNING,
  STATE_SHUTDOWN
};

SystemState currentState = STATE_CONFIG;

// ============================================================
//              WEB SERVER VÀ PREFERENCES
// ============================================================

WebServer server(80);
Preferences preferences;

const char* WIFI_PREF_NAMESPACE = "wifi_cfg";
const char* SYSTEM_PREF_NAMESPACE = "system_cfg";

const char* KEY_WIFI_SSID = "ssid";
const char* KEY_WIFI_PASS = "pass";

const char* KEY_INTERVAL = "interval";
const char* KEY_LOGGING = "logging";
const char* KEY_LAST_COMMAND = "last_cmd";

String savedSSID = "";
String savedPassword = "";

bool webServerStarted = false;
bool restartPending = false;
unsigned long restartRequestedAt = 0;

const unsigned long RESTART_DELAY_MS = 1500;

// ============================================================
//                    BIẾN VẬN HÀNH
// ============================================================

unsigned long measurementIntervalMs = 1000;
bool loggingEnabled = true;
bool measurementEnabled = true;
bool measureNowRequested = false;

const unsigned long COMMAND_CHECK_INTERVAL_MS = 1000;
const unsigned long STATUS_HEARTBEAT_INTERVAL_MS = 5000;

unsigned long lastMeasurementTime = 0;
unsigned long lastCommandCheckTime = 0;
unsigned long lastStatusHeartbeatTime = 0;
unsigned long lastReconnectAttempt = 0;

bool wifiLostMessageShown = false;
uint64_t lastProcessedCommandId = 0;

// ============================================================
//                  HÀM HỖ TRỢ CHUNG
// ============================================================

String uint64ToString(uint64_t value) {
  char buffer[32];
  snprintf(
    buffer,
    sizeof(buffer),
    "%llu",
    static_cast<unsigned long long>(value)
  );
  return String(buffer);
}

uint64_t stringToUInt64(const String& value) {
  return strtoull(value.c_str(), nullptr, 10);
}

String jsonEscape(const String& input) {
  String output;
  output.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);

    switch (c) {
      case '\\':
        output += "\\\\";
        break;
      case '"':
        output += "\\\"";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        output += c;
        break;
    }
  }

  return output;
}

int findJsonValueStart(const String& json, const String& key) {
  String token = "\"" + key + "\"";
  int keyPosition = json.indexOf(token);

  if (keyPosition < 0) {
    return -1;
  }

  int colonPosition = json.indexOf(':', keyPosition + token.length());

  if (colonPosition < 0) {
    return -1;
  }

  int valuePosition = colonPosition + 1;

  while (
    valuePosition < static_cast<int>(json.length()) &&
    isspace(static_cast<unsigned char>(json.charAt(valuePosition)))
  ) {
    valuePosition++;
  }

  return valuePosition;
}

bool jsonGetString(
  const String& json,
  const String& key,
  String& output
) {
  int valuePosition = findJsonValueStart(json, key);

  if (
    valuePosition < 0 ||
    valuePosition >= static_cast<int>(json.length()) ||
    json.charAt(valuePosition) != '"'
  ) {
    return false;
  }

  valuePosition++;
  String result;
  bool escaped = false;

  for (int i = valuePosition; i < static_cast<int>(json.length()); i++) {
    char c = json.charAt(i);

    if (escaped) {
      switch (c) {
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        default:
          result += c;
          break;
      }

      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
      continue;
    }

    if (c == '"') {
      output = result;
      return true;
    }

    result += c;
  }

  return false;
}

bool jsonGetUInt64(
  const String& json,
  const String& key,
  uint64_t& output
) {
  int valuePosition = findJsonValueStart(json, key);

  if (valuePosition < 0) {
    return false;
  }

  int endPosition = valuePosition;

  while (
    endPosition < static_cast<int>(json.length()) &&
    isdigit(static_cast<unsigned char>(json.charAt(endPosition)))
  ) {
    endPosition++;
  }

  if (endPosition == valuePosition) {
    return false;
  }

  output = stringToUInt64(
    json.substring(valuePosition, endPosition)
  );

  return true;
}

bool jsonGetLong(
  const String& json,
  const String& key,
  long& output
) {
  int valuePosition = findJsonValueStart(json, key);

  if (valuePosition < 0) {
    return false;
  }

  int endPosition = valuePosition;

  if (
    endPosition < static_cast<int>(json.length()) &&
    json.charAt(endPosition) == '-'
  ) {
    endPosition++;
  }

  while (
    endPosition < static_cast<int>(json.length()) &&
    isdigit(static_cast<unsigned char>(json.charAt(endPosition)))
  ) {
    endPosition++;
  }

  if (
    endPosition == valuePosition ||
    (
      endPosition == valuePosition + 1 &&
      json.charAt(valuePosition) == '-'
    )
  ) {
    return false;
  }

  output = json.substring(valuePosition, endPosition).toInt();
  return true;
}

bool jsonGetBool(
  const String& json,
  const String& key,
  bool& output
) {
  int valuePosition = findJsonValueStart(json, key);

  if (valuePosition < 0) {
    return false;
  }

  if (json.startsWith("true", valuePosition)) {
    output = true;
    return true;
  }

  if (json.startsWith("false", valuePosition)) {
    output = false;
    return true;
  }

  return false;
}

// ============================================================
//                         OLED
// ============================================================

bool initializeOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] Khong tim thay OLED SSD1306.");
    Serial.println("[OLED] He thong van tiep tuc chay.");
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();

  Serial.println("[OLED] Khoi tao thanh cong.");
  return true;
}

void showOLEDMessage(
  const String& line1,
  const String& line2 = "",
  const String& line3 = ""
) {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 4);
  display.println(line1);

  display.setCursor(0, 24);
  display.println(line2);

  display.setCursor(0, 44);
  display.println(line3);

  display.display();
}

void showTC(const String& value) {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("T_C");

  display.setTextSize(3);
  display.setCursor(0, 28);
  display.print(value);

  display.display();
}

void showConnectingWiFi(const String& ssid) {
  String shortenedSSID = ssid;

  if (shortenedSSID.length() > 18) {
    shortenedSSID = shortenedSSID.substring(0, 18);
  }

  showOLEDMessage(
    "CONNECTING WIFI",
    shortenedSSID,
    "Please wait..."
  );
}

void showConfigModeScreen() {
  showOLEDMessage(
    "CONFIG MODE",
    CONFIG_AP_SSID,
    WiFi.softAPIP().toString()
  );
}

// ============================================================
//                    PREFERENCES
// ============================================================

bool loadWiFiCredentials() {
  if (!preferences.begin(WIFI_PREF_NAMESPACE, true)) {
    Serial.println("[Preferences] Khong mo duoc bo nho WiFi.");
    return false;
  }

  savedSSID = preferences.getString(KEY_WIFI_SSID, "");
  savedPassword = preferences.getString(KEY_WIFI_PASS, "");

  preferences.end();

  if (savedSSID.length() == 0) {
    Serial.println("[Preferences] Chua co WiFi duoc luu.");
    return false;
  }

  Serial.print("[Preferences] WiFi da luu: ");
  Serial.println(savedSSID);

  return true;
}

bool saveWiFiCredentials(
  const String& ssid,
  const String& password
) {
  if (!preferences.begin(WIFI_PREF_NAMESPACE, false)) {
    Serial.println("[Preferences] Khong mo duoc bo nho de ghi.");
    return false;
  }

  size_t ssidResult = preferences.putString(KEY_WIFI_SSID, ssid);
  size_t passwordResult = preferences.putString(KEY_WIFI_PASS, password);

  preferences.end();

  if (ssidResult == 0 || passwordResult == 0) {
    Serial.println("[Preferences] Luu WiFi that bai.");
    return false;
  }

  savedSSID = ssid;
  savedPassword = password;

  Serial.println("[Preferences] Da luu WiFi thanh cong.");
  return true;
}

bool clearWiFiCredentials() {
  if (!preferences.begin(WIFI_PREF_NAMESPACE, false)) {
    Serial.println("[Preferences] Khong mo duoc bo nho de xoa.");
    return false;
  }

  bool result = preferences.clear();
  preferences.end();

  savedSSID = "";
  savedPassword = "";

  Serial.println(
    result
      ? "[Preferences] Da xoa WiFi."
      : "[Preferences] Xoa WiFi that bai."
  );

  return result;
}

void loadSystemSettings() {
  if (!preferences.begin(SYSTEM_PREF_NAMESPACE, true)) {
    Serial.println("[Preferences] Khong doc duoc cau hinh he thong.");
    return;
  }

  measurementIntervalMs = preferences.getULong(
    KEY_INTERVAL,
    1000
  );

  loggingEnabled = preferences.getBool(
    KEY_LOGGING,
    true
  );

  String lastCommandText = preferences.getString(
    KEY_LAST_COMMAND,
    "0"
  );

  preferences.end();

  if (
    measurementIntervalMs < 500 ||
    measurementIntervalMs > 600000
  ) {
    measurementIntervalMs = 1000;
  }

  lastProcessedCommandId = stringToUInt64(lastCommandText);

  Serial.print("[Config] Chu ky do: ");
  Serial.print(measurementIntervalMs);
  Serial.println(" ms");

  Serial.print("[Config] Ghi log: ");
  Serial.println(loggingEnabled ? "BAT" : "TAT");
}

void saveSystemSettings() {
  if (!preferences.begin(SYSTEM_PREF_NAMESPACE, false)) {
    Serial.println("[Preferences] Khong luu duoc cau hinh he thong.");
    return;
  }

  preferences.putULong(KEY_INTERVAL, measurementIntervalMs);
  preferences.putBool(KEY_LOGGING, loggingEnabled);
  preferences.end();
}

void saveLastProcessedCommandId(uint64_t commandId) {
  lastProcessedCommandId = commandId;

  if (!preferences.begin(SYSTEM_PREF_NAMESPACE, false)) {
    Serial.println("[Preferences] Khong luu duoc command id.");
    return;
  }

  preferences.putString(
    KEY_LAST_COMMAND,
    uint64ToString(commandId)
  );

  preferences.end();
}

// ============================================================
//               HỖ TRỢ TRANG HTML CẤU HÌNH
// ============================================================

String htmlEscape(const String& input) {
  String output;
  output.reserve(input.length() + 16);

  for (size_t i = 0; i < input.length(); i++) {
    char character = input.charAt(i);

    switch (character) {
      case '&':
        output += "&amp;";
        break;
      case '<':
        output += "&lt;";
        break;
      case '>':
        output += "&gt;";
        break;
      case '"':
        output += "&quot;";
        break;
      case '\'':
        output += "&#39;";
        break;
      default:
        output += character;
        break;
    }
  }

  return output;
}

String createWiFiOptions() {
  String options;

  Serial.println("[WiFi Scan] Dang tim WiFi xung quanh...");

  int networkCount = WiFi.scanNetworks();

  if (networkCount <= 0) {
    options +=
      "<option value=\"\">Khong tim thay mang WiFi</option>";

    Serial.println("[WiFi Scan] Khong tim thay mang nao.");
    WiFi.scanDelete();

    return options;
  }

  Serial.print("[WiFi Scan] Tim thay ");
  Serial.print(networkCount);
  Serial.println(" mang.");

  for (int i = 0; i < networkCount; i++) {
    String networkSSID = WiFi.SSID(i);

    if (networkSSID.length() == 0) {
      continue;
    }

    options += "<option value=\"";
    options += htmlEscape(networkSSID);
    options += "\">";
    options += htmlEscape(networkSSID);
    options += " (";
    options += String(WiFi.RSSI(i));
    options += " dBm)";
    options += "</option>";
  }

  WiFi.scanDelete();
  return options;
}

// ============================================================
//                     WEB CẤU HÌNH WIFI
// ============================================================

void handleConfigPage() {
  String wifiOptions = createWiFiOptions();

  String html;
  html.reserve(7000);

  html += F(
    "<!DOCTYPE html>"
    "<html lang=\"vi\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" "
    "content=\"width=device-width, initial-scale=1.0\">"
    "<title>Cau hinh WiFi ESP32</title>"
    "<style>"
    "*{box-sizing:border-box;}"
    "body{margin:0;font-family:Arial,sans-serif;"
    "background:#f1f4f8;color:#202124;}"
    ".container{max-width:460px;margin:40px auto;padding:24px;"
    "background:white;border-radius:14px;"
    "box-shadow:0 4px 20px rgba(0,0,0,.12);}"
    "h1{font-size:24px;margin-top:0;text-align:center;}"
    ".info{padding:12px;background:#eef5ff;border-radius:8px;"
    "margin-bottom:18px;line-height:1.6;}"
    "label{display:block;font-weight:bold;margin-top:15px;"
    "margin-bottom:6px;}"
    "input{width:100%;padding:12px;font-size:16px;"
    "border:1px solid #b8bec7;border-radius:8px;}"
    "button{width:100%;padding:13px;margin-top:20px;"
    "font-size:16px;font-weight:bold;border:0;"
    "border-radius:8px;cursor:pointer;}"
    ".save{background:#1769e0;color:white;}"
    ".clear{background:#d93025;color:white;}"
    ".note{font-size:13px;color:#5f6368;margin-top:16px;"
    "line-height:1.5;}"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1>Cau hinh WiFi ESP32</h1>"
    "<div class=\"info\">"
    "<b>WiFi cau hinh:</b> ESP32_TEMP_CONFIG<br>"
    "<b>Dia chi:</b> 192.168.4.1<br>"
  );

  html += "<b>WiFi dang luu:</b> ";

  if (savedSSID.length() > 0) {
    html += htmlEscape(savedSSID);
  } else {
    html += "Chua co";
  }

  html += F(
    "</div>"
    "<form method=\"POST\" action=\"/save\">"
    "<label for=\"ssid\">Ten WiFi</label>"
    "<input id=\"ssid\" name=\"ssid\" list=\"wifiList\" "
    "placeholder=\"Chon hoac nhap ten WiFi\" required>"
    "<datalist id=\"wifiList\">"
  );

  html += wifiOptions;

  html += F(
    "</datalist>"
    "<label for=\"pass\">Mat khau WiFi</label>"
    "<input id=\"pass\" name=\"pass\" type=\"password\" "
    "placeholder=\"Nhap mat khau WiFi\">"
    "<button class=\"save\" type=\"submit\">"
    "Luu va ket noi"
    "</button>"
    "</form>"
    "<form method=\"POST\" action=\"/clear\">"
    "<button class=\"clear\" type=\"submit\">"
    "Xoa WiFi da luu"
    "</button>"
    "</form>"
    "<div class=\"note\">"
    "Sau khi bam Luu, ESP32 se khoi dong lai va thu ket noi "
    "vao mang WiFi vua nhap. Neu ket noi that bai, ESP32 se "
    "phat lai mang ESP32_TEMP_CONFIG."
    "</div>"
    "</div>"
    "</body>"
    "</html>"
  );

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSaveWiFi() {
  if (!server.hasArg("ssid")) {
    server.send(
      400,
      "text/plain; charset=utf-8",
      "Thieu ten WiFi."
    );
    return;
  }

  String newSSID = server.arg("ssid");
  String newPassword = server.arg("pass");

  newSSID.trim();

  if (newSSID.length() == 0) {
    server.send(
      400,
      "text/plain; charset=utf-8",
      "Ten WiFi khong duoc de trong."
    );
    return;
  }

  if (!saveWiFiCredentials(newSSID, newPassword)) {
    server.send(
      500,
      "text/plain; charset=utf-8",
      "Khong luu duoc cau hinh WiFi."
    );
    return;
  }

  String response =
    "<!DOCTYPE html><html lang=\"vi\"><head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" "
    "content=\"width=device-width, initial-scale=1.0\">"
    "<title>Da luu WiFi</title></head>"
    "<body style=\"font-family:Arial;text-align:center;"
    "padding:40px;\">"
    "<h2>Da luu cau hinh WiFi</h2>"
    "<p>ESP32 se khoi dong lai.</p>"
    "<p>WiFi duoc chon: <b>" +
    htmlEscape(newSSID) +
    "</b></p>"
    "<p>Hay ket noi lai may tinh vao mang WiFi thong thuong.</p>"
    "</body></html>";

  server.send(
    200,
    "text/html; charset=utf-8",
    response
  );

  restartPending = true;
  restartRequestedAt = millis();

  Serial.println("[Web] Da nhan cau hinh WiFi.");
  Serial.println("[Web] Chuan bi khoi dong lai ESP32.");
}

void handleClearWiFi() {
  if (!clearWiFiCredentials()) {
    server.send(
      500,
      "text/plain; charset=utf-8",
      "Khong xoa duoc cau hinh WiFi."
    );
    return;
  }

  server.send(
    200,
    "text/html; charset=utf-8",
    "<html><body style=\"font-family:Arial;text-align:center;"
    "padding:40px;\"><h2>Da xoa WiFi</h2>"
    "<p>ESP32 se khoi dong lai o che do cau hinh.</p>"
    "</body></html>"
  );

  restartPending = true;
  restartRequestedAt = millis();
}

void configureWebServerRoutes() {
  server.on("/", HTTP_GET, handleConfigPage);
  server.on("/save", HTTP_POST, handleSaveWiFi);
  server.on("/clear", HTTP_POST, handleClearWiFi);

  server.on("/test", HTTP_GET, []() {
    server.send(
      200,
      "text/plain; charset=utf-8",
      "ESP32 WEB SERVER OK"
    );
  });

  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
}

// ============================================================
//                     CHẾ ĐỘ CONFIG
// ============================================================

void startConfigMode() {
  currentState = STATE_CONFIG;

  Serial.println();
  Serial.println("============================================");
  Serial.println("       BAT DAU CHE DO CAU HINH WIFI");
  Serial.println("============================================");

  WiFi.disconnect();
  delay(200);

  WiFi.mode(WIFI_AP_STA);
  delay(200);

  WiFi.softAPConfig(
    CONFIG_AP_IP,
    CONFIG_AP_GATEWAY,
    CONFIG_AP_SUBNET
  );

  bool accessPointStarted =
    WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASS);

  if (!accessPointStarted) {
    Serial.println("[Config] Khong tao duoc WiFi cau hinh.");

    showOLEDMessage(
      "CONFIG ERROR",
      "Cannot start AP",
      ""
    );

    while (true) {
      delay(1000);
    }
  }

  if (!webServerStarted) {
    server.begin();
    webServerStarted = true;
  }

  Serial.print("[Config] Ten WiFi: ");
  Serial.println(CONFIG_AP_SSID);

  Serial.print("[Config] Mat khau: ");
  Serial.println(CONFIG_AP_PASS);

  Serial.print("[Config] Dia chi web: http://");
  Serial.println(WiFi.softAPIP());

  Serial.println("============================================");
  Serial.println();

  showConfigModeScreen();
}

// ============================================================
//                    KẾT NỐI WIFI THẬT
// ============================================================

bool connectToSavedWiFi(
  const String& ssid,
  const String& password,
  unsigned long timeoutMs
) {
  if (ssid.length() == 0) {
    return false;
  }

  Serial.println();
  Serial.println("============================================");
  Serial.print("[WiFi] Dang ket noi: ");
  Serial.println(ssid);
  Serial.println("============================================");

  showConnectingWiFi(ssid);

  if (webServerStarted) {
    server.stop();
    webServerStarted = false;
  }

  WiFi.softAPdisconnect(false);
  WiFi.disconnect();
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (password.length() == 0) {
    WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  unsigned long connectionStartTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - connectionStartTime < timeoutMs
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("============================================");
    Serial.println("      DA KET NOI WIFI THANH CONG");
    Serial.print("[WiFi] SSID: ");
    Serial.println(ssid);

    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());

    Serial.println("============================================");
    Serial.println();

    return true;
  }

  Serial.println("============================================");
  Serial.println("      KHONG KET NOI DUOC WIFI");
  Serial.println("      CHUYEN SANG CONFIG MODE");
  Serial.println("============================================");
  Serial.println();

  WiFi.disconnect();
  return false;
}

// ============================================================
//                    FIREBASE REST API
// ============================================================

String firebaseUrl(const String& path) {
  return (
    String(FIREBASE_HOST) +
    path +
    ".json?auth=" +
    FIREBASE_AUTH
  );
}

bool firebaseRequest(
  const char* method,
  const String& path,
  const String& payload,
  String& responseBody
) {
  responseBody = "";

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Firebase] Khong co WiFi.");
    return false;
  }

  HTTPClient http;
  http.begin(firebaseUrl(path));
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");

  int httpCode;

  if (strcmp(method, "GET") == 0) {
    httpCode = http.GET();
  } else {
    httpCode = http.sendRequest(method, payload);
  }

  responseBody = http.getString();

  bool success =
    httpCode >= 200 &&
    httpCode < 300;

  if (!success) {
    Serial.print("[Firebase] ");
    Serial.print(method);
    Serial.print(" ");
    Serial.print(path);
    Serial.print(" loi HTTP: ");
    Serial.println(httpCode);

    if (responseBody.length() > 0) {
      Serial.print("[Firebase] Phan hoi: ");
      Serial.println(responseBody);
    }
  }

  http.end();
  return success;
}

bool firebaseWrite(
  const char* method,
  const String& path,
  const String& payload
) {
  String response;
  return firebaseRequest(
    method,
    path,
    payload,
    response
  );
}

String deviceBasePath() {
  return "/devices/" + String(DEVICE_ID);
}

// ============================================================
//                    TRẠNG THÁI THIẾT BỊ
// ============================================================

bool sendDeviceStatus(
  bool online,
  const String& mode,
  const String& message
) {
  String json;
  json.reserve(360);

  json += "{";
  json += "\"online\":";
  json += online ? "true" : "false";
  json += ",";
  json += "\"mode\":\"";
  json += jsonEscape(mode);
  json += "\",";
  json += "\"message\":\"";
  json += jsonEscape(message);
  json += "\",";
  json += "\"last_seen\":{\".sv\":\"timestamp\"},";
  json += "\"last_command_id\":";
  json += uint64ToString(lastProcessedCommandId);
  json += ",";
  json += "\"measurement_interval_ms\":";
  json += String(measurementIntervalMs);
  json += ",";
  json += "\"logging_enabled\":";
  json += loggingEnabled ? "true" : "false";
  json += ",";
  json += "\"measurement_enabled\":";
  json += measurementEnabled ? "true" : "false";
  json += ",";
  json += "\"wifi_ssid\":\"";
  json += jsonEscape(savedSSID);
  json += "\",";
  json += "\"ip\":\"";
  json += jsonEscape(WiFi.localIP().toString());
  json += "\"";
  json += "}";

  return firebaseWrite(
    "PUT",
    deviceBasePath() + "/status",
    json
  );
}

bool acknowledgeCommand(
  uint64_t commandId,
  const String& status,
  const String& message
) {
  String json;
  json.reserve(220);

  json += "{";
  json += "\"status\":\"";
  json += jsonEscape(status);
  json += "\",";
  json += "\"message\":\"";
  json += jsonEscape(message);
  json += "\",";
  json += "\"processed_at\":{\".sv\":\"timestamp\"}";
  json += "}";

  bool success = firebaseWrite(
    "PATCH",
    deviceBasePath() + "/command",
    json
  );

  if (success) {
    Serial.print("[Command] Da phan hoi command ");
    Serial.print(uint64ToString(commandId));
    Serial.print(": ");
    Serial.println(status);
  }

  return success;
}

// ============================================================
//                    KHỞI TẠO CẢM BIẾN
// ============================================================

bool initializeSensors() {
  if (sensorsReady) {
    return true;
  }

  Serial.println("[Sensor] Khoi tao MLX90614...");

  if (!mlx.begin()) {
    Serial.println("[Sensor] Khong tim thay MLX90614.");

    showOLEDMessage(
      "SENSOR ERROR",
      "MLX90614",
      "Not found"
    );

    return false;
  }

  Serial.println("[Sensor] MLX90614 san sang.");

  Serial.println("[Sensor] Khoi tao VL53L0X...");

  if (!lox.begin()) {
    Serial.println("[Sensor] Khong tim thay VL53L0X.");

    showOLEDMessage(
      "SENSOR ERROR",
      "VL53L0X",
      "Not found"
    );

    return false;
  }

  Serial.println("[Sensor] VL53L0X san sang.");

  sensorsReady = true;
  return true;
}

// ============================================================
//                    DỮ LIỆU ĐO FIREBASE
// ============================================================

String buildMeasurementJson(
  double T_C,
  float T_O,
  float T_ambient,
  double x_cm,
  int distance_mm
) {
  String json;
  json.reserve(240);

  json += "{";
  json += "\"T_C\":" + String(T_C, 2) + ",";
  json += "\"T_O\":" + String(T_O, 2) + ",";
  json += "\"T_ambient\":" + String(T_ambient, 2) + ",";
  json += "\"distance_cm\":" + String(x_cm, 1) + ",";
  json += "\"distance_mm\":" + String(distance_mm) + ",";
  json += "\"timestamp\":{\".sv\":\"timestamp\"}";
  json += "}";

  return json;
}

bool sendMeasurementToFirebase(
  double T_C,
  float T_O,
  float T_ambient,
  double x_cm,
  int distance_mm
) {
  String json = buildMeasurementJson(
    T_C,
    T_O,
    T_ambient,
    x_cm,
    distance_mm
  );

  bool currentSuccess = firebaseWrite(
    "PUT",
    deviceBasePath() + "/current",
    json
  );

  bool logSuccess = true;

  if (loggingEnabled) {
    logSuccess = firebaseWrite(
      "POST",
      "/sensor_log",
      json
    );
  }

  if (currentSuccess && logSuccess) {
    Serial.print("[Firebase] Da gui T_C = ");
    Serial.print(T_C, 2);
    Serial.println(" *C");
  }

  return currentSuccess && logSuccess;
}

// ============================================================
//                  ĐỌC VÀ XỬ LÝ CẢM BIẾN
// ============================================================

void measureAndSendData() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

  int distance_mm = -1;

  if (measure.RangeStatus != 4) {
    distance_mm = measure.RangeMilliMeter;
  }

  float T_ambient = mlx.readAmbientTempC();
  float T_O = mlx.readObjectTempC();

  Serial.println("------------------------------------------------------------");

  if (isnan(T_ambient) || isnan(T_O)) {
    Serial.println("[Nhiet do] Du lieu cam bien khong hop le.");
    showTC("LOI");
    return;
  }

  Serial.print("[Nhiet do] T_ambient = ");
  Serial.print(T_ambient, 2);
  Serial.print(" *C | T_O = ");
  Serial.print(T_O, 2);
  Serial.println(" *C");

  if (distance_mm == -1) {
    Serial.println("[Khoang cach] Ngoai pham vi do.");
    Serial.println("[Nhiet do] T_C = N/A");
    showTC("N/A");
    return;
  }

  double x_cm = distance_mm / 10.0 - 3.5;

  Serial.print("[Khoang cach] ");
  Serial.print(distance_mm);
  Serial.print(" mm | x = ");
  Serial.print(x_cm, 1);
  Serial.println(" cm");

  double Y =
    COEFF_A * x_cm * x_cm +
    COEFF_B * x_cm +
    COEFF_C;

  if (fabs(Y) < Y_MIN_THRESHOLD) {
    Serial.print("[Nhiet do] Khong the tinh T_C, Y = ");
    Serial.println(Y, 6);
    showTC("LOI");
    return;
  }

  double T_C = T_O / Y;

  Serial.print("[Nhiet do] T_C = ");
  Serial.print(T_C, 2);
  Serial.print(" *C | Y = ");
  Serial.println(Y, 6);

  showTC(String(T_C, 2));

  sendMeasurementToFirebase(
    T_C,
    T_O,
    T_ambient,
    x_cm,
    distance_mm
  );
}

// ============================================================
//                     SHUTDOWN / DEEP SLEEP
// ============================================================

void shutdownSystem() {
  currentState = STATE_SHUTDOWN;

  Serial.println();
  Serial.println("============================================");
  Serial.println("             DUNG HE THONG");
  Serial.println("============================================");

  showOLEDMessage(
    "SHUTDOWN",
    "WiFi OFF",
    "Reset to wake"
  );

  delay(800);

  if (displayReady) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  delay(200);

  Serial.println("[System] Vao deep sleep.");
  Serial.flush();

  esp_deep_sleep_start();
}

// ============================================================
//                    XỬ LÝ COMMAND
// ============================================================

void processCommand(const String& commandJson) {
  if (
    commandJson.length() == 0 ||
    commandJson == "null"
  ) {
    return;
  }

  uint64_t commandId = 0;
  String action;
  String status;

  if (!jsonGetUInt64(commandJson, "id", commandId)) {
    return;
  }

  if (!jsonGetString(commandJson, "action", action)) {
    return;
  }

  if (!jsonGetString(commandJson, "status", status)) {
    return;
  }

  if (status != "pending") {
    return;
  }

  if (commandId == lastProcessedCommandId) {
    acknowledgeCommand(
      commandId,
      "done",
      "Command da duoc xu ly truoc do"
    );
    return;
  }

  Serial.println();
  Serial.println("============================================");
  Serial.print("[Command] ID: ");
  Serial.println(uint64ToString(commandId));
  Serial.print("[Command] Action: ");
  Serial.println(action);
  Serial.println("============================================");

  if (action == "set_interval") {
    long requestedInterval = 0;

    if (
      !jsonGetLong(commandJson, "value", requestedInterval) ||
      requestedInterval < 500 ||
      requestedInterval > 600000
    ) {
      acknowledgeCommand(
        commandId,
        "error",
        "Interval phai tu 500 den 600000 ms"
      );
      return;
    }

    measurementIntervalMs =
      static_cast<unsigned long>(requestedInterval);

    saveLastProcessedCommandId(commandId);
    saveSystemSettings();

    acknowledgeCommand(
      commandId,
      "done",
      "Da doi chu ky do"
    );

    sendDeviceStatus(
      true,
      measurementEnabled ? "running" : "paused",
      "Measurement interval updated"
    );

    Serial.print("[Command] Chu ky moi: ");
    Serial.print(measurementIntervalMs);
    Serial.println(" ms");

    return;
  }

  if (action == "set_measurement") {
    bool requestedMeasurement = true;

    if (!jsonGetBool(commandJson, "value", requestedMeasurement)) {
      acknowledgeCommand(
        commandId,
        "error",
        "Gia tri measurement khong hop le"
      );
      return;
    }

    measurementEnabled = requestedMeasurement;
    measureNowRequested = false;

    saveLastProcessedCommandId(commandId);

    if (measurementEnabled) {
      showTC("--.--");
      lastMeasurementTime = millis() - measurementIntervalMs;
      Serial.println("[System] Tiep tuc do.");
    } else {
      showOLEDMessage(
        "MEASUREMENT",
        "PAUSED",
        ""
      );
      Serial.println("[System] Tam dung do.");
    }

    acknowledgeCommand(
      commandId,
      "done",
      measurementEnabled
        ? "Da tiep tuc do"
        : "Da tam dung do"
    );

    sendDeviceStatus(
      true,
      measurementEnabled ? "running" : "paused",
      measurementEnabled
        ? "Measurement resumed"
        : "Measurement paused"
    );

    return;
  }

  if (action == "measure_now") {
    if (!measurementEnabled) {
      saveLastProcessedCommandId(commandId);

      acknowledgeCommand(
        commandId,
        "error",
        "Khong the do ngay khi dang tam dung"
      );
      return;
    }

    saveLastProcessedCommandId(commandId);

    acknowledgeCommand(
      commandId,
      "done",
      "Da nhan yeu cau do ngay"
    );

    measureNowRequested = true;
    return;
  }

  if (action == "set_logging") {
    bool requestedLogging = true;

    if (!jsonGetBool(commandJson, "value", requestedLogging)) {
      acknowledgeCommand(
        commandId,
        "error",
        "Gia tri logging khong hop le"
      );
      return;
    }

    loggingEnabled = requestedLogging;

    saveLastProcessedCommandId(commandId);
    saveSystemSettings();

    acknowledgeCommand(
      commandId,
      "done",
      loggingEnabled
        ? "Da bat ghi log"
        : "Da tat ghi log"
    );

    sendDeviceStatus(
      true,
      measurementEnabled ? "running" : "paused",
      loggingEnabled
        ? "Logging enabled"
        : "Logging disabled"
    );

    return;
  }

  if (action == "clear_wifi") {
    saveLastProcessedCommandId(commandId);

    acknowledgeCommand(
      commandId,
      "done",
      "Dang xoa WiFi va khoi dong lai"
    );

    sendDeviceStatus(
      false,
      "config",
      "Clearing saved WiFi"
    );

    showOLEDMessage(
      "CLEAR WIFI",
      "Restarting...",
      ""
    );

    delay(500);
    clearWiFiCredentials();
    delay(500);

    ESP.restart();
    return;
  }

  if (action == "shutdown") {
    saveLastProcessedCommandId(commandId);

    acknowledgeCommand(
      commandId,
      "done",
      "He thong dang shutdown"
    );

    sendDeviceStatus(
      false,
      "shutdown",
      "System entered deep sleep"
    );

    shutdownSystem();
    return;
  }

  acknowledgeCommand(
    commandId,
    "error",
    "Action khong duoc ho tro"
  );
}

void checkFirebaseCommand() {
  String response;

  bool success = firebaseRequest(
    "GET",
    deviceBasePath() + "/command",
    "",
    response
  );

  if (success) {
    processCommand(response);
  }
}

// ============================================================
//                    CHẾ ĐỘ RUNNING
// ============================================================

void startRunningMode() {
  if (!initializeSensors()) {
    Serial.println("[System] Loi cam bien. He thong dung.");

    while (true) {
      delay(1000);
    }
  }

  currentState = STATE_RUNNING;

  lastMeasurementTime =
    millis() - measurementIntervalMs;

  lastCommandCheckTime =
    millis() - COMMAND_CHECK_INTERVAL_MS;

  lastStatusHeartbeatTime =
    millis() - STATUS_HEARTBEAT_INTERVAL_MS;

  lastReconnectAttempt = millis();
  wifiLostMessageShown = false;

  showTC("--.--");

  Serial.println();
  Serial.println("============================================");
  Serial.println("          HE THONG BAT DAU DO");
  Serial.println("============================================");
  Serial.println();

  sendDeviceStatus(
    true,
    "running",
    "System started"
  );
}

void handleRunningMode() {
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiLostMessageShown) {
      Serial.println("[WiFi] Da mat ket noi.");

      showOLEDMessage(
        "WIFI LOST",
        "Reconnecting...",
        ""
      );

      wifiLostMessageShown = true;
    }

    if (
      millis() - lastReconnectAttempt >=
      WIFI_RECONNECT_DELAY_MS
    ) {
      lastReconnectAttempt = millis();

      bool reconnected = connectToSavedWiFi(
        savedSSID,
        savedPassword,
        WIFI_CONNECT_TIMEOUT_MS
      );

      if (reconnected) {
        wifiLostMessageShown = false;

        if (measurementEnabled) {
          lastMeasurementTime =
            millis() - measurementIntervalMs;
        }

        lastCommandCheckTime =
          millis() - COMMAND_CHECK_INTERVAL_MS;

        lastStatusHeartbeatTime =
          millis() - STATUS_HEARTBEAT_INTERVAL_MS;

        if (measurementEnabled) {
          showTC("--.--");
        } else {
          showOLEDMessage(
            "MEASUREMENT",
            "PAUSED",
            ""
          );
        }

        sendDeviceStatus(
          true,
          measurementEnabled ? "running" : "paused",
          measurementEnabled
            ? "WiFi reconnected"
            : "WiFi reconnected - measurement paused"
        );
      } else {
        startConfigMode();
      }
    }

    return;
  }

  if (
    millis() - lastCommandCheckTime >=
    COMMAND_CHECK_INTERVAL_MS
  ) {
    lastCommandCheckTime = millis();
    checkFirebaseCommand();

    if (currentState != STATE_RUNNING) {
      return;
    }
  }

  if (
    measurementEnabled &&
    (
      measureNowRequested ||
      millis() - lastMeasurementTime >=
      measurementIntervalMs
    )
  ) {
    measureNowRequested = false;
    lastMeasurementTime = millis();
    measureAndSendData();
  }

  if (
    millis() - lastStatusHeartbeatTime >=
    STATUS_HEARTBEAT_INTERVAL_MS
  ) {
    lastStatusHeartbeatTime = millis();

    sendDeviceStatus(
      true,
      measurementEnabled ? "running" : "paused",
      measurementEnabled ? "OK" : "Measurement paused"
    );
  }
}

// ============================================================
//                          SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("============================================");
  Serial.println(" ESP32 TEMP + WIFI CONFIG + FIREBASE CONTROL ");
  Serial.println("============================================");
  Serial.println();

  Wire.begin(21, 22);

  displayReady = initializeOLED();

  if (displayReady) {
    showOLEDMessage(
      "SYSTEM START",
      "Checking WiFi",
      ""
    );
  }

  configureWebServerRoutes();
  loadSystemSettings();

  bool credentialsAvailable = loadWiFiCredentials();

  if (credentialsAvailable) {
    bool connected = connectToSavedWiFi(
      savedSSID,
      savedPassword,
      WIFI_CONNECT_TIMEOUT_MS
    );

    if (connected) {
      startRunningMode();
      return;
    }
  }

  startConfigMode();
}

// ============================================================
//                           LOOP
// ============================================================

void loop() {
  switch (currentState) {
    case STATE_CONFIG:
      server.handleClient();

      if (
        restartPending &&
        millis() - restartRequestedAt >=
        RESTART_DELAY_MS
      ) {
        Serial.println("[System] Khoi dong lai...");
        delay(100);
        ESP.restart();
      }

      delay(2);
      break;

    case STATE_RUNNING:
      handleRunningMode();
      delay(2);
      break;

    case STATE_SHUTDOWN:
      delay(1000);
      break;
  }
}
