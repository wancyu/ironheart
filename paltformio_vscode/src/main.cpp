/**
 * @file main.cpp
 * @author wancyu
 * @brief
 * ESP32 主控的“钢铁侠心脏”DIY项目，
 * 可显示实时时间并配合 WS2812 灯带产生炫酷光效，
 * 通过 NTP 网络自动校时。
 * 
 * @version 1.0
 * @date 2025-10-17
 * @note 开发环境：PlatformIO + VSCode
 *       所需第三方库
 *       TM1637Display：数码管显示
 *       Adafruit_NeoPixel：灯珠控制
 *       若要进一步完善，比如增加语音模块，建议重构整个项目！
 */

/**************************** 引入库文件 ****************************/
#include "Arduino.h"
#include "TM1637Display.h"
#include "Adafruit_NeoPixel.h"
#include "WiFi.h"

/**************************** 引脚定义 ****************************/
// WS2812 灯带控制引脚
#define PIN 17
// TM1637 数码管引脚
#define display_CLK 18
#define display_DIO 19

/**************************** 配置参数 ****************************/
#define NUM_LEDS 35                        // WS2812 灯珠数量
const char *wifi_user = "chenyu";         // WiFi 名称
const char *wifi_password = "cy383245";   // WiFi 密码
const char *ntp_server = "cn.pool.ntp.org"; // NTP 服务器
const int ntp_gmtoffset_sec = 8 * 3600;    // 时区偏移（UTC+8）
int Display_backlight = 7;                // 数码管亮度等级（0~7）


/**************************** 全局变量 ****************************/
int flag = 0;                // 整点标志位，防止重复执行
int hour = 0;                // 当前小时
int minute = 0;              // 当前分钟
struct tm timeinfo;          // 系统时间结构体

Adafruit_NeoPixel strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800); // 灯带对象
TM1637Display display(display_CLK, display_DIO);             // 数码管对象


/**************************** 功能函数 ****************************/

/**
 * @brief 连接 WiFi 网络
 */
void wifi_connect()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_user, wifi_password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        display.showNumberDecEx(8888, 0b01000000, true, 4, 0);
        delay(50);
        display.clear();
    }
}


/**
 * @brief 检查 WiFi 连接状态，掉线自动重连
 */
void wifi_reconnect()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        wifi_connect();
    }
}


/**
 * @brief 从本地系统更新时间信息
 */
void update_time()
{
    getLocalTime(&timeinfo);
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
}


/**
 * @brief 彩虹色生成函数（辅助函数）
 * @param WheelPos 彩虹色相位（0~255）
 * @return uint32_t 生成的 24 位 RGB 颜色值
 * @note 不同区间颜色变化：
 *       0~84：红→绿过渡
 *       85~169：绿→蓝过渡
 *       170~255：蓝→红过渡
 */
uint32_t Wheel(byte WheelPos)
{
    if (WheelPos < 85)
    {
        return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    }
    else if (WheelPos < 170)
    {
        WheelPos -= 85;
        return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    else
    {
        WheelPos -= 170;
        return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}


/**
 * @brief 彩虹滚动效果（连续流动渐变）
 * @note startIndex 每次偏移 1，用于产生循环滚动动画。
 *       一次调用大约12秒
 */
void rainbowScroll()
{
    static int startIndex = 0;

    for (int i = 0; i < NUM_LEDS; i++)
    {
        int colorIndex = (i + startIndex) & 255; // 环状索引
        strip.setPixelColor(i, Wheel(colorIndex));
    }

    strip.show();
    startIndex = (startIndex + 1) % 256; // 每次偏移一个色相
    delay(50);                           // 控制滚动速度
}


/**
 * @brief 彩虹闪烁+淡出效果
 * @note 先循环彩虹流光，再逐步降低亮度至全黑
 */
void flash_cuckoo()
{
    // 彩虹流动阶段
    for (int j = 0; j < 256; j += 5)
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip.setPixelColor(i, Wheel((i * 256 / NUM_LEDS + j) & 255));
        }
        strip.show();
        delay(50);
    }

    // 渐暗淡出阶段
    for (int brightness = 255; brightness >= 0; brightness -= 5)
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            uint32_t color = strip.getPixelColor(i);
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            strip.setPixelColor(i,
                                r * brightness / 255,
                                g * brightness / 255,
                                b * brightness / 255);
        }
        strip.show();
        delay(30);
    }
}


/**
 * @brief 数码管动态报时与光效动画（整点触发）
 * @details
 * 阶段说明：
 * 1. 数字滚动 00~99
 * 2. “88:88” 闪烁 8 次
 * 3. 彩虹闪烁特效（调用 flash_cuckoo）
 * 4. 青色呼吸灯循环
 */
void display_cuckoo()
{
    display.showNumberDecEx(8888, 0b01000000, true, 4, 0);

    // 阶段3：彩虹闪烁
    flash_cuckoo();

    // 阶段1：数字滚动
    for (int i = 0; i < 100; i++)
    {
        display.showNumberDecEx(i, 0b01000000, true, 2, 0);
        display.showNumberDecEx(i, 0b01000000, true, 2, 2);
    }

    // 阶段2：“88:88”闪烁
    for (int i = 0; i < 8; i++)
    {
        display.showNumberDecEx(8888, 0b01000000, true, 4, 0);
        delay(50);
        display.clear();
        delay(50);
    }

    display.showNumberDecEx(88, 0b01000000, true, 2, 0);
    display.showNumberDecEx(88, 0b01000000, true, 2, 2);

    // 阶段4：青色呼吸灯（亮 → 暗）
    for (int brightness = 0; brightness <= 255; brightness += 5)
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip.setPixelColor(i, strip.Color(0, brightness, brightness));
        }
        strip.show();
        delay(30);
    }
    // 青色呼吸灯（暗 → 亮）
    for (int brightness = 255; brightness >= 0; brightness -= 5)
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            strip.setPixelColor(i, strip.Color(0, brightness, brightness));
        }
        strip.show();
        delay(30);
    }
}


/**
 * @brief 初始化用户配置与外设
 * @note 启动时自动连接 WiFi、同步时间、初始化显示设备
 */
void init_user()
{
    Serial.begin(115200);
    display.setBrightness(Display_backlight);
    strip.begin();
    display.showNumberDecEx(88, 0b01000000, true, 2, 0);
    display.showNumberDecEx(88, 0b01000000, true, 2, 2);
    strip.show(); // 显示灯带
    wifi_connect();
    configTime(ntp_gmtoffset_sec, 0, ntp_server);
}



/**************************** 主程序 ****************************/

/**
 * @brief 程序初始化
 */
void setup()
{

    init_user();
    display_cuckoo(); // 启动时执行一次炫酷动画
}

/**
 * @brief 主循环：更新时间显示与动画控制
 */
void loop()
{
    wifi_reconnect();
    update_time();

    // 显示当前时间（小时:分钟）
    display.showNumberDecEx(hour, 0b01000000, true, 2, 0);
    display.showNumberDecEx(minute, 0b01000000, true, 2, 2);

    // 整点报时触发逻辑
    if (minute == 0 && flag == 0)
    {
        display_cuckoo();
        flag = 1;
    }
    else if (minute >= 1)
    {
        flag = 0;
    }

    // 普通时段持续滚动彩虹灯效
    rainbowScroll();
}
