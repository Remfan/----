#include "SGP30.h"    //气体传感器
#include <SPI.h>      //SPI总线协议
#include <TFT_eSPI.h> //TFT显示屏
#include <string.h>
#include <Time.h>
#include <AirQuality.h>
#include <FS.h>
#include <SD.h>
#include <JPEGDecoder.h>

#include <fonts\Ali_Bold16.h>
#include <fonts\Ali_SemiBold20.h>
#include <fonts\Ali_SemiBold24.h>
#include <fonts\Ali_Bold30.h>
#include <fonts\Ali_Bold96.h>
#include <img\logo.h>
// 管脚定义
#define TFT_BL 23      // 背光管脚
#define TFT_CS 2       // TFT片选管脚
#define TOUCH_CS 13    // TFT数据/命令选择管脚
#define SD_CS 32       // TFT数据/命令选择管脚
#define BUTTON 34      // 多功能按键管脚
#define BUTTON_MENU 35 // MENU按键管脚

// 对象建立
SGP mySGP30;               // 创建SGP对象
TFT_eSPI tft = TFT_eSPI(); // 创建TFT对象
AirQuality Air;            // 和风天气对象
SPIClass SDSPI = SPIClass(VSPI);

// 联网数据
const char *ssid = "ZBH";       // 你的网络名称
const char *password = "18531975187"; // 你的网络密码
// 和风天气配置
String UserKey = "b42f7a030ce04fbe9f5c505be9f9cd51"; // 私钥 https://dev.heweather.com/docs/start/get-api-key
String Location = "101010100";                       // 城市代码 https://github.com/heweather/LocationList,表中的 Location_ID
// 屏幕时间更新
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// 全局变量
bool mode = 0;           // 对比模式black:0 ; white:1
int option = 1;          // 触摸位置选项
int option_prev = 0;     // 上一loop的触摸位置选项
int keypad_tone = 1;     // 案件声音
int brightness = 128;    // 背光亮度
int brightness_flag = 0; // 亮度控制flag
float elec = 0.5;        // 电池电量
int Page_num = 1;        // 显示页面编号
int last_page_num = 0;
int val = 0; // 34按钮数据
int data[4];
int SGPdata[2];

String time_string = "24:24";
String last_time_string = "24:24";
String date_string = "2024年11月56日  星期一";

void IRAM_ATTR MenuButtonPress()
{
  Page_num = 1;
}

void setup()
{
  Serial.begin(115200);
  // SGP30_Setup(); // 初始化SGP30

  // 设置背光
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // 将所有SPI总线上的CS拉高
  pinMode(TFT_CS, INPUT);
  pinMode(TOUCH_CS, INPUT);
  pinMode(SD_CS, INPUT);
  pinMode(14,INPUT);
  digitalWrite(TFT_CS, LOW);
  digitalWrite(TOUCH_CS, LOW);
  digitalWrite(SD_CS, LOW);
  pinMode(BUTTON, INPUT);

  SGP30_Setup(); // 初始化SGP30
  delay(2000);

  SD_setup();

  Air.config(UserKey, Location, "m", "zh");
  init_ntp_time();
  pinMode(35, INPUT);
  delay(2000);

  tft.init();
  tft.setRotation(3);
  ledcAttach(TFT_BL, 5000, 8); // 设置背景光PWM
  ledcWrite(TFT_BL, brightness);

  xTaskCreate(timeTask, "TimeTask", 2048, NULL, 1, NULL);
  xTaskCreate(bottonTask, "BottonTask", 2048, NULL, 1, NULL);

  attachInterrupt(digitalPinToInterrupt(BUTTON_MENU), MenuButtonPress, FALLING); // 设置MENU键中断
}

/*每次循环根据Page_num选择显示哪一页，Page_num标识了页码（暂时为1-5）。
进入不同页显示函数后，判断全局变量option是否发生变化来确定是否要刷新页面。
因此，显示的基本逻辑为：
1. 判断页码，进入对应页面函数
2. 重新配置亮度选项（不会引起频闪）
3. 判断触摸选项是否变化，选择性刷新页面
4. 判断触摸控制位置，更新option以及进行当前loop必须执行的行为
5. 本loop函数结束，option的修改将在下一个loop中体现，回到1*/
void loop()
{
  uint16_t x, y;
  option = touch_detect(&x, &y);

  // 亮度控制
  if (brightness_flag)
    brightness += val * 6;
  if (brightness > 255)
    brightness = 1;
  tft.setRotation(3);
  ledcWrite(TFT_BL, brightness);

  float batteryVoltage = analogRead(14) * 0.00359;
  float batteryMin = 3.0;
  float batteryMax = 5.0;

  elec = ((batteryVoltage - batteryMin) / (batteryMax - batteryMin));
  if (elec > 1)
  {
      elec = 1;
  }
  if (elec < 0)
  {
      elec = 0;
  }


  // 页面控制
  if (last_page_num != Page_num ||
      (option && (Page_num == 1 || Page_num == 2)) ||
      last_time_string != time_string)
  {

    if (last_time_string != time_string)
      last_time_string = time_string;

    if (Page_num == 1 && option > 0 && option < 5)
    {
      Menu_page(mode, elec, option);
      delay(200);
      Page_num = option + 1;
      option = 0;
    }
    brightness_flag = 0;
    switch (Page_num)
    {
    case 1:
      Menu_page(mode, elec, option); // 目录页，导向其余各页面
      break;
    case 2:
      Config_page(mode, elec, option); // 设置页面
      if (option == 1)
        brightness_flag = 1;
      if (option == 4)
      {
        init_ntp_time();
        delay(2000);
      }
      if (option == 5)
      {
        init_system();
        delay(200);
        Config_page(mode, elec, option); // 设置页面
      }

      break;
    case 3:
      // Air.get();
      Weather_outside_page( // 显示外部天气的页面
          mode,
          elec,
          Air.getAqi(),
          Air.getTemp(),
          Air.getHumidity(),
          Air.getPm2p5());
      break;
    case 4:
      while (!DHT11_ReadData(15, data))
        SGP30_Read(SGPdata);
      Weather_inside_page(
          mode,
          elec,
          SGPdata[1],
          data[2],
          data[0],
          SGPdata[0]); // 显示室内参数的页面
      break;
    default:
      Screen_saver(); // 屏保/电子相框
      break;
    }
    last_page_num = Page_num;
  }
}

int touch_detect(uint16_t *x, uint16_t *y)
{
  int touch_case;
  uint16_t xaxis;
  uint16_t yaxis;
  if (tft.getTouch(x, y))
  {
    xaxis = *x;
    yaxis = *y;
    if (yaxis >= 72 && yaxis <= 116)
    {
      touch_case = 1;
    }
    else if (yaxis >= 120 && yaxis <= 164)
    {
      touch_case = 2;
      if (Page_num == 2)
      {
        keypad_tone = !keypad_tone;
      }
    }
    else if (yaxis >= 168 && yaxis <= 212)
    {
      touch_case = 3;
      if (Page_num == 2)
      {
        mode = !mode;
      }
    }
    else if (yaxis >= 216 && yaxis <= 260)
    {
      touch_case = 4;
    }
    else if (yaxis >= 264 && yaxis <= 308)
    {
      touch_case = 5;
    }
    else
      touch_case = 0;
  }
  else
  {
    touch_case = 0;
  }
  delay(100);
  // Serial.printf("x: %i     ", xaxis);
  // Serial.printf("y: %i     ", yaxis);
  // Serial.printf("z: %i \n", tft.getTouchRawZ());
  return touch_case;
}

/*#########以下用于Weather_outside_page的控制########*/
void Weather_outside_page(bool mode, float e, int AQI_val, int wendu_val, int shidu_val, int PM25_val)
{
  show_wo_background(mode, e);
  show_wo_data(AQI_val, wendu_val, shidu_val, PM25_val);
}

void show_wo_background(bool mode, float e)
{
  if (!mode)
  {
    tft.fillScreen(0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.pushImage(8, 7, 18, 18, wifi_black);
  }
  else
  {
    tft.fillScreen(0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.pushImage(8, 7, 18, 18, wifi_white);
  }

  tft.loadFont(Page1_Ali_SemiBold20);
  tft.drawString(date_string, 134, 40);
  tft.drawString("室外", 80, 190);
  tft.drawString("北京市海淀区", 190, 190);

  tft.drawString("AQI指数", 340, 190);
  tft.drawString("室外温度", 60, 250);
  tft.drawString("室外湿度", 210, 250);
  tft.drawString("PM2.5", 345, 250);

  tft.loadFont(Ali_Bold96);
  tft.drawString(time_string, 104, 70);

  show_battery(mode, e);
  tft.unloadFont();
}

void show_wo_data(int AQI_val, int wendu_val, int shidu_val, int PM25_val)
{
  tft.loadFont(Page1_Ali_Bold30);
  tft.drawString(String(int(AQI_val)), 355, 217);         // AQI
  tft.drawString(String(int(wendu_val)) + "°C", 72, 277); // wendu
  tft.drawString(String(int(shidu_val)) + "%", 222, 277); // shidu
  tft.drawString(String(int(PM25_val)), 352, 277);        // PM2.5

  if (PM25_val < 100)
  {
    tft.fillRect(195, 223, 105, 10, 0x07E0); // green
  }
  else if (PM25_val < 300)
  {
    tft.fillRect(195, 223, 105, 10, 0xFEA0); // yellow
  }
  else
  {
    tft.fillRect(195, 223, 105, 10, 0xF800); // red
  }

  tft.unloadFont();
}

/*#########以下用于Weather_inside_page的控制########*/

void Weather_inside_page(bool mode, float e, int youji_val, int wendu_val, int shidu_val, int CO2_val)
{
  show_wi_background(mode, e);
  show_wi_data(youji_val, wendu_val, shidu_val, CO2_val);
}

void show_wi_background(bool mode, float e)
{
  if (!mode)
  {
    tft.fillScreen(0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.pushImage(8, 7, 18, 18, wifi_black);
  }
  else
  {
    tft.fillScreen(0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.pushImage(8, 7, 18, 18, wifi_white);
  }

  tft.loadFont(Page2_Ali_SemiBold20);
  tft.drawString(date_string, 134, 40);
  tft.drawString("室内", 80, 190);
  tft.drawString("北京市海淀区", 190, 190);

  tft.drawString("有机气体浓度", 340, 190);
  tft.drawString("室内温度", 60, 250);
  tft.drawString("室内湿度", 210, 250);
  tft.drawString("CO2浓度", 340, 250);

  tft.loadFont(Ali_Bold96);
  tft.drawString(time_string, 104, 70);

  show_battery(mode, e);
  tft.unloadFont();
}

void show_wi_data(int youji_val, int wendu_val, int shidu_val, int CO2_val)
{
  tft.loadFont(Page1_Ali_Bold30);
  tft.drawString(String(int(youji_val)), 355, 217); 
  tft.drawString(String(int(wendu_val)) + "°C", 72, 277); // wendu
  tft.drawString(String(int(shidu_val)) + "%", 222, 277); // shidu
  tft.drawString(String(int(CO2_val)), 352, 277);    

  if (youji_val < 100)
  {
    tft.fillRect(195, 223, 105, 10, 0x07E0); // green
    Serial.printf("green\n");
  }
  else if (youji_val < 300)
  {
    tft.fillRect(195, 223, 105, 10, 0xFEA0); // yellow
    Serial.printf("yellow\n");
  }
  else
  {
    tft.fillRect(195, 223, 105, 10, 0xF800); // red
    Serial.printf("red\n");
  }

  tft.unloadFont();
}

/*#########以下用于Config_page的控制########*/

void Config_page(bool mode, float e, int selected)
{
  show_cp_background(mode, e);
  if (selected == 1)
    cp_option_1(!mode);
  else
    cp_option_1(mode);
  if (selected == 2)
    cp_option_2(!mode);
  else
    cp_option_2(mode);
  if (selected == 3)
    cp_option_3(!mode);
  else
    cp_option_3(mode);
  if (selected == 4)
    cp_option_4(!mode);
  else
    cp_option_4(mode);
  if (selected == 5)
    cp_option_5(!mode);
  else
    cp_option_5(mode);
}

void show_cp_background(bool mode, float e)
{
  if (!mode)
  {
    tft.fillScreen(0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.pushImage(8, 7, 18, 18, wifi_black);
  }
  else
  {
    tft.fillScreen(0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.pushImage(8, 7, 18, 18, wifi_white);
  }
  tft.loadFont(Ali_Bold16);
  tft.drawString(time_string, 40, 10);
  tft.loadFont(PageConfig_Ali_Bold30);
  tft.drawString("设 置", 210, 30);
  show_battery(mode, e);
  tft.unloadFont();
}

void cp_option_1(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 72, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("亮度调节", 50, 84);
    tft.pushImage(410, 80, 30, 30, jiantou_black);
    tft.pushImage(10, 80, 30, 30, liangdu_black);
  }
  else
  {
    tft.fillRect(0, 72, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("亮度调节", 50, 84);
    tft.pushImage(410, 80, 30, 30, jiantou_white);
    tft.pushImage(10, 80, 30, 30, liangdu_white);
  }
  tft.unloadFont();
}
void cp_option_2(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 120, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("按键声音", 50, 132);
    if (keypad_tone)
    {
      tft.drawString("开", 410, 132);
    }
    else
    {
      tft.drawString("关", 410, 132);
    }
    tft.pushImage(10, 128, 30, 30, yinliang_black);
  }
  else
  {
    tft.fillRect(0, 120, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("按键声音", 50, 132);
    if (keypad_tone)
    {
      tft.drawString("开", 410, 132);
    }
    else
    {
      tft.drawString("关", 410, 132);
    }
    tft.pushImage(10, 128, 30, 30, yinliang_white);
  }
  tft.unloadFont();
}
void cp_option_3(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 168, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("主题切换", 50, 180);
    tft.drawString("深", 410, 180);
    tft.pushImage(10, 176, 30, 30, zhuti_black);
  }
  else
  {
    tft.fillRect(0, 168, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("主题切换", 50, 180);
    tft.drawString("浅", 410, 180);
    tft.pushImage(10, 176, 30, 30, zhuti_white);
  }
  tft.unloadFont();
}
void cp_option_4(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 216, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("配置网络", 50, 228);
    tft.pushImage(410, 224, 30, 30, jiantou_black);
    tft.pushImage(10, 224, 30, 30, wangluo_black);
  }
  else
  {
    tft.fillRect(0, 216, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("配置网络", 50, 228);
    tft.pushImage(410, 224, 30, 30, jiantou_white);
    tft.pushImage(10, 224, 30, 30, wangluo_white);
  }
  tft.unloadFont();
}
void cp_option_5(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 264, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("恢复出厂", 50, 276);
    tft.pushImage(410, 272, 30, 30, jiantou_black);
    tft.pushImage(10, 272, 30, 30, chuchang_black);
  }
  else
  {
    tft.fillRect(0, 264, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageConfig_Ali_SemiBold24);
    tft.drawString("恢复出厂", 50, 276);
    tft.pushImage(410, 272, 30, 30, jiantou_white);
    tft.pushImage(10, 272, 30, 30, chuchang_white);
  }
  tft.unloadFont();
}

/*#########以下用于Menu_page的控制########*/
void Menu_page(bool mode, float e, int selected)
{
  show_mp_background(mode, e);
  if (selected == 1)
    mp_option_1(!mode);
  else
    mp_option_1(mode);
  if (selected == 2)
    mp_option_2(!mode);
  else
    mp_option_2(mode);
  if (selected == 3)
    mp_option_3(!mode);
  else
    mp_option_3(mode);
  if (selected == 4)
    mp_option_4(!mode);
  else
    mp_option_4(mode);
}

void show_mp_background(bool mode, float e)
{
  if (!mode)
  {
    tft.fillScreen(0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.pushImage(8, 7, 18, 18, wifi_black);
  }
  else
  {
    tft.fillScreen(0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.pushImage(8, 7, 18, 18, wifi_white);
  }
  tft.loadFont(Ali_Bold16);
  tft.drawString(time_string, 40, 10);
  tft.loadFont(PageMenu_Ali_Bold30);
  tft.drawString("菜 单", 210, 30);
  show_battery(mode, e);
  tft.unloadFont();
}

void mp_option_1(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 72, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("设置页", 50, 84);
    tft.pushImage(410, 80, 30, 30, jiantou_black);
    tft.pushImage(10, 80, 30, 30, shezhi_black);
  }
  else
  {
    tft.fillRect(0, 72, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("设置页", 50, 84);
    tft.pushImage(410, 80, 30, 30, jiantou_white);
    tft.pushImage(10, 80, 30, 30, shezhi_white);
  }
  tft.unloadFont();
}

void mp_option_2(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 120, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("城市天气", 50, 132);
    tft.pushImage(410, 132, 30, 30, jiantou_black);
    tft.pushImage(10, 128, 30, 30, tianqi_black);
  }
  else
  {
    tft.fillRect(0, 120, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("城市天气", 50, 132);
    tft.pushImage(410, 132, 30, 30, jiantou_white);
    tft.pushImage(10, 128, 30, 30, tianqi_white);
  }
  tft.unloadFont();
}

void mp_option_3(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 168, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("室内空气", 50, 180);
    tft.pushImage(410, 180, 30, 30, jiantou_black);
    tft.pushImage(10, 176, 30, 30, kongqi_black);
  }
  else
  {
    tft.fillRect(0, 168, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("室内空气", 50, 180);
    tft.pushImage(410, 180, 30, 30, jiantou_white);
    tft.pushImage(10, 176, 30, 30, kongqi_white);
  }
  tft.unloadFont();
}

void mp_option_4(bool mode)
{
  if (!mode)
  {
    tft.fillRect(0, 216, 480, 44, 0x0000);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("屏保页", 50, 228);
    tft.pushImage(410, 224, 30, 30, jiantou_black);
    tft.pushImage(10, 224, 30, 30, pingbao_black);
  }
  else
  {
    tft.fillRect(0, 216, 480, 44, 0xFFFF);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.loadFont(PageMenu_Ali_SemiBold24);
    tft.drawString("屏保页", 50, 228);
    tft.pushImage(410, 224, 30, 30, jiantou_white);
    tft.pushImage(10, 224, 30, 30, pingbao_white);
  }
  tft.unloadFont();
}

/*#########以下用于Screen_Saver的控制########*/
void SD_setup()
{
  SDSPI.begin(27, 26, 25, SD_CS); // CLK, MISO, MOSI, SS
  pinMode(TFT_BL, OUTPUT);        // 背光管脚设置为输出模式
  digitalWrite(TFT_BL, HIGH);
  pinMode(TFT_CS, OUTPUT); // TFT片选管脚设置为输出模式
  digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT); // TFT数据/命令选择管脚设置为输出模式
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(SD_CS, OUTPUT); // TFT数据/命令选择管脚设置为输出模式
  digitalWrite(SD_CS, HIGH);

  if (!SD.begin(SD_CS, SDSPI))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  Serial.println("SD initialisation done.");
}

//  Draw a JPEG on the TFT pulled from SD Card
//  xpos, ypos is top left corner of plotted image
void drawSdJpeg(const char *filename, int xpos, int ypos)
{

  // Open the named file (the Jpeg decoder library will close it)
  File jpegFile = SD.open(filename, FILE_READ); // or, file handle reference for SD library

  if (!jpegFile)
  {
    Serial.print("ERROR: File \"");
    Serial.print(filename);
    Serial.println("\" not found!");
    return;
  }

  Serial.println("===========================");
  Serial.print("Drawing file: ");
  Serial.println(filename);
  Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  bool decoded = JpegDec.decodeSdFile(jpegFile); // Pass the SD file handle to the decoder,
  // bool decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded)
  {
    // print information about the image to the serial port
    jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  }
  else
  {
    Serial.println("Jpeg file format not supported!");
  }
}

//  Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//  This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
//  fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void jpegRender(int xpos, int ypos)
{

  // jpegInfo(); // Print information from the JPEG file (could comment this line out)

  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // Fetch data from the file, decode and display
  while (JpegDec.read())
  {                        // While there is more data in the file
    pImg = JpegDec.pImage; // Decode a MCU (Minimum Coding Unit, typically a 8x8 or 16x16 pixel block)

    // Calculate coordinates of top left corner of current MCU
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x)
      win_w = mcu_w;
    else
      win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y)
      win_h = mcu_h;
    else
      win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // calculate how many pixels must be drawn
    uint32_t mcu_pixels = win_w * win_h;

    // draw image MCU block only if it will fit on the screen
    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
    {
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
      Serial.println("Push correct!");
    }
    else if ((mcu_y + win_h) >= tft.height())
    {
      JpegDec.abort(); // Image has run off bottom of screen so abort decoding
      Serial.println("Jpeg aborted!");
    }
  }

  tft.setSwapBytes(swapBytes);

  showTime(millis() - drawTime); // These lines are for sketch testing only
}

//  Print image information to the serial port (optional)
//  JpegDec.decodeFile(...) or JpegDec.decodeArray(...) must be called before this info is available!
void jpegInfo()
{

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

//  Show the execution time (optional)
//  WARNING: for UNO/AVR legacy reasons printing text to the screen with the Mega might not work for
//  sketch sizes greater than ~70KBytes because 16-bit address pointers are used in some libraries.
// The Due will work fine with the HX8357_Due library.

void showTime(uint32_t msTime)
{
  // tft.setCursor(0, 0);
  // tft.setTextFont(1);
  // tft.setTextSize(2);
  // tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // tft.print(F(" JPEG drawn in "));
  // tft.print(msTime);
  // tft.println(F(" ms "));
  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}
void Screen_saver()
{
  tft.setRotation(3); // landscape
  tft.fillScreen(0x0000);
  drawSdJpeg("/Mouse480.jpg", 0, 0); // This draws a jpeg pulled off the SD Card
}

/*#########以下用于通用控件的控制########*/
void show_battery(bool mode, float elec)
{
  int len = floor(elec * 12) + 2;
  String elec_str = String(int(round(elec * 100))) + "%";
  tft.loadFont(Ali_Bold16);
  if (!mode)
  {
    tft.pushImage(410, 6, 20, 20, dianchi_black);
    tft.drawString(elec_str, 435, 8);
    tft.fillRect(411, 12, len, 8, 0xFFFF);
  }
  else
  {
    tft.pushImage(410, 6, 20, 20, dianchi_white);
    tft.drawString(elec_str, 435, 8);
    tft.fillRect(411, 12, len, 8, 0x0000);
  }
  tft.unloadFont();
}

struct tm sync_Localtime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
    Serial.println("Failed to obtain time");
  return timeinfo;
}

void init_ntp_time()
{
  int timer = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    if(timer == 20)
      break;
    delay(500);
    Serial.print(".");
    timer ++;
  }
  Serial.println("WiFi connected!");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  sync_Localtime();
  Air.get();
  delay(2000);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected!");
}

void init_system()
{
  mode = 0;         // 对比模式black:0 ; white:1
  keypad_tone = 1;  // 案件声音
  brightness = 128; // 背光亮度
  brightness_flag = 0;
  elec = 0.5; // 电池电量
  val = 0;
  init_ntp_time();
}

void get_time_string()
{
  time_string = "";
  struct tm timeinfo = sync_Localtime();
  if (timeinfo.tm_hour < 10)
    time_string += "0" + String(timeinfo.tm_hour) + ":";
  else
    time_string += String(timeinfo.tm_hour) + ":";
  if (timeinfo.tm_min < 10)
    time_string += "0" + String(timeinfo.tm_min);
  else
    time_string += String(timeinfo.tm_min);
}

void get_date_string()
{
  struct tm timeinfo = sync_Localtime();
  date_string = "" + String(timeinfo.tm_year + 1900) + "年" + String(timeinfo.tm_mon + 1) + "月" + String(timeinfo.tm_mday) + "日  星期";
  switch (timeinfo.tm_wday)
  {
  case 0:
    date_string += "日";
    break;
  case 1:
    date_string += "一";
    break;
  case 2:
    date_string += "二";
    break;
  case 3:
    date_string += "三";
    break;
  case 4:
    date_string += "四";
    break;
  case 5:
    date_string += "五";
    break;
  case 6:
    date_string += "六";
    break;
  }
}

void timeTask(void *pvParameters)
{
  while (1)
  {
    get_date_string();
    get_time_string();
    delay(1000 * 5);
  }
}

void bottonTask(void *pvParameters)
{
  while (1)
  {
    val = !digitalRead(BUTTON);
    delay(20);
  }
}

void SGP30_Setup()
{
  u32_t sgp30_dat;
  u16_t CO2Data, TVOCData;
  mySGP30.SGP30_Init();
  mySGP30.SGP30_Write(0x20, 0x08);
  sgp30_dat = mySGP30.SGP30_Read(); // 读取SGP30的值
  CO2Data = (sgp30_dat & 0xffff0000) >> 16;
  TVOCData = sgp30_dat & 0x0000ffff;
  // SGP30模块开机需要一定时间初始化，在初始化阶段读取的CO2浓度为400ppm，TVOC为0ppd且恒定不变，因此上电后每隔一段时间读取一次
  // SGP30模块的值，如果CO2浓度为400ppm，TVOC为0ppd，发送“正在检测中...”，直到SGP30模块初始化完成。
  // 初始化完成后刚开始读出数据会波动比较大，属于正常现象，一段时间后会逐渐趋于稳定。
  // 气体类传感器比较容易受环境影响，测量数据出现波动是正常的，可自行添加滤波函数。
  while (CO2Data == 400 && TVOCData == 0)
  {
    mySGP30.SGP30_Write(0x20, 0x08);
    sgp30_dat = mySGP30.SGP30_Read();         // 读取SGP30的值
    CO2Data = (sgp30_dat & 0xffff0000) >> 16; // 取出CO2浓度值
    TVOCData = sgp30_dat & 0x0000ffff;        // 取出TVOC值
    Serial.println("Initializing......");
    delay(500);
  }
}

void SGP30_Read(int *result)
{
  u16_t CO2Data, TVOCData; // 定义CO2浓度变量与TVOC浓度变量
  u32_t sgp30_dat;
  mySGP30.SGP30_Write(0x20, 0x08);
  sgp30_dat = mySGP30.SGP30_Read();         // 读取SGP30的值
  CO2Data = (sgp30_dat & 0xffff0000) >> 16; // 取出CO2浓度值
  TVOCData = sgp30_dat & 0x0000ffff;        // 取出TVOC值
  result[0] = CO2Data;
  result[1] = TVOCData;
}

void DHT11_Start(uint8_t pin)
{
  pinMode(pin, OUTPUT_OPEN_DRAIN);
  digitalWrite(pin, LOW);
  delay(20);
  digitalWrite(pin, HIGH);
  delayMicroseconds(30);
  Serial.println("DHT11 Start");
}

int DHT11_Check_Response(uint8_t pin)
{
  pinMode(pin, OUTPUT_OPEN_DRAIN);
  if (digitalRead(pin) == LOW)
  {
    delayMicroseconds(80);
    if (digitalRead(pin) == HIGH)
    {
      Serial.println("DHT11 Responsed");
      return 1;
    }
    else
    {
      Serial.println("DHT11 NO response!!");
      return 0;
    }
  }
  else
  {
    return 0;
  }
}

int DHT11_ReadByte(uint8_t pin)
{
  int i;
  int byte = 0x00;
  for (i = 0; i < 8; i++)
  {
    while (digitalRead(pin) == LOW)
      ;
    delayMicroseconds(40);
    if (digitalRead(pin) == HIGH)
    {
      byte = (byte << 1) | 1;
    }
    else
    {
      byte = (byte << 1) | 0;
    }
    while (digitalRead(pin) == HIGH)
      ;
  }
  return byte;
}

int DHT11_ReadData(uint8_t pin, int *result)
{
  int i;
  int check = 0;
  int data[5];
  DHT11_Start(pin);
  if (DHT11_Check_Response(pin) == 1)
  {
    for (i = 0; i < 5; i++)
    {
      data[i] = DHT11_ReadByte(pin);
    }
    if (data[0] + data[1] + data[2] + data[3] == data[4])
    {
      result[0] = data[0];
      result[1] = data[1];
      result[2] = data[2];
      result[3] = data[3];
      return 1;
    }
    else
    {
      return 0;
    }
  }
  else
  {
    return 0;
  }
}