#ifndef _AIRQUALITY_H
#define _AIRQUALITY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "ArduinoZlib.h"
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
//#include <WiFiClientSecureBearSSL.h>
#include <WiFiClientSecure.h>
#define DEBUG   // 调试用

class AirQuality {
  public:
    AirQuality();

    void config(String userKey, String location, String unit, String lang);
    bool get();
    String getLastUpdate();
    String getServerCode();

    int getTemp();
    int getHumidity();
    int getAqi();
    int getPm2p5();

  private:
    const char* _host = "devapi.qweather.net"; // 服务器地址
    const int httpsPort = 443;

    String _requserKey;  // 私钥
    String _reqLocation; // 位置
    String _reqUnit;     // 单位
    String _reqLang;     // 语言

    void _parseNowJson_1(String payload);
    void _parseNowJson_2(String payload);

    String _response_code =  "no_init";   // API状态码
    String _last_update_str = "no_init";  // 当前API最近更新时间
    int _now_temp = 999;
    int _now_humidity = 999;
    int _now_aqi_int = 999;
    int _now_pm2p5_int = 999; 

};
#endif
