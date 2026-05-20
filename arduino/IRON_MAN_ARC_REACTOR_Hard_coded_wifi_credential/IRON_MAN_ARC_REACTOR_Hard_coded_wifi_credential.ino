#include <Arduino.h>
#include <TM1637Display.h>
#include <WiFi.h>
#include <time.h>

// ===== WiFi 配置 =====
const char* ssid = "hello";              // 改成你扫描到的 SSID
const char* password = "12345678";  // 改成你的密码

// ===== TM1637 模块配置 =====
#define CLK 22
#define DIO 23

TM1637Display display(CLK, DIO);

// ===== 时间配置 =====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;

// WiFi 连接重试次数
const int MAX_WIFI_ATTEMPTS = 3;
int wifi_attempt = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("   TM1637 时钟显示 - 增强版");
  Serial.println("========================================");

  initDisplay();
  
  Serial.println("\n[系统] ESP32 芯片信息:");
  Serial.print("  MAC地址: ");
  Serial.println(WiFi.macAddress());

  // 多次重试连接
  while (WiFi.status() != WL_CONNECTED && wifi_attempt < MAX_WIFI_ATTEMPTS) {
    wifi_attempt++;
    Serial.print("\n[WiFi] 连接尝试 ");
    Serial.print(wifi_attempt);
    Serial.print("/");
    Serial.println(MAX_WIFI_ATTEMPTS);
    connectWiFi();
    
    if (WiFi.status() != WL_CONNECTED && wifi_attempt < MAX_WIFI_ATTEMPTS) {
      Serial.println("[WiFi] 等待 3 秒后重试...");
      delay(3000);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    syncNetworkTime();
  } else {
    Serial.println("\n[警告] WiFi 连接最终失败");
    Serial.println("[提示] 请检查:");
    Serial.println("  1. 热点是否改为 WPA2/WPA（不是 WPA3）");
    Serial.println("  2. 密码是否正确");
    Serial.println("  3. 热点是否启用了 2.4GHz 频段");
  }

  Serial.println("\n初始化完成，开始显示时间...");
}

void loop() {
  if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = millis();
    updateTimeDisplay();
  }
  yield();
}

void initDisplay() {
  display.setBrightness(0x0f);
  display.clear();
  Serial.println("[TM1637] 显示模块初始化完成");
}

void connectWiFi() {
  Serial.print("正在连接到 '");
  Serial.print(ssid);
  Serial.println("'...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // 断开之前的连接
  delay(500);
  
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi 连接成功！");
    Serial.print("IP 地址: ");
    Serial.println(WiFi.localIP());
    Serial.print("信号强度: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.print("✗ 连接失败 (状态码: ");
    Serial.print(WiFi.status());
    Serial.println(")");
  }
}

void syncNetworkTime() {
  Serial.println("\n[NTP] 正在同步网络时间...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  time_t now = time(nullptr);
  int sync_attempts = 0;
  
  while (now < 24 * 3600 && sync_attempts < 30) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    sync_attempts++;
  }

  Serial.println();

  if (now > 24 * 3600) {
    Serial.println("✓ 时间同步成功！");
    struct tm* timeinfo = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial.print("当��时间: ");
    Serial.println(buffer);
  } else {
    Serial.println("✗ 时间同步失败");
  }
}

void updateTimeDisplay() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  int hours = timeinfo->tm_hour;
  int minutes = timeinfo->tm_min;

  int timeValue = hours * 100 + minutes;
  uint8_t dots = 0b01000000;  // 中间冒号

  display.showNumberDecEx(timeValue, dots, true, 4, 0);

  static unsigned long lastSerialPrint = 0;
  if (millis() - lastSerialPrint >= 10000) {
    lastSerialPrint = millis();
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial.print("[TIME] ");
    Serial.println(buffer);
  }
}