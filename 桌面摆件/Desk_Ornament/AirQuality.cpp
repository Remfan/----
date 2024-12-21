#include "AirQuality.h"

uint8_t *outbuf;

AirQuality::AirQuality()
{
}

// 配置请求信息，私钥、位置、单位、语言
void AirQuality::config(String userKey, String location, String unit, String lang)
{
    _requserKey = userKey;
    _reqLocation = location;
    _reqUnit = unit;
    _reqLang = lang;
}

// 尝试获取信息，成功返回true，失败返回false
bool AirQuality::get()
{
    // https请求
    String url_1 = "https://devapi.qweather.com/v7/weather/now?location=" + _reqLocation +
                 "&key=" + _requserKey + "&unit=" + _reqUnit + "&lang=" + _reqLang + "&gzip=n";
    String url_2 = "https://devapi.qweather.com/v7/air/now?location=" + _reqLocation +
                 "&key=" + _requserKey + "&unit=" + _reqUnit + "&lang=" + _reqLang + "&gzip=n";
    HTTPClient http_1, http_2;
    if (http_1.begin(url_1) && http_2.begin(url_2))
    {
        #ifdef DEBUG
        Serial.println("HTTPclient setUp done!");
        #endif
    }
    // start connection and send HTTP header
    int httpCode_1 = http_1.GET();
    int httpCode_2 = http_2.GET();
    // httpCode will be negative on error
    if (httpCode_1 > 0)
    {
        // file found at server
        if (httpCode_1 == HTTP_CODE_OK)
        {
            String payload_1 = "";
            // 解压Gzip数据流
            int len = http_1.getSize();
            uint8_t buff[2048] = { 0 };
            WiFiClient *stream = http_1.getStreamPtr();
            while (http_1.connected() && (len > 0 || len == -1)) {
                size_t size = stream->available();  // 还剩下多少数据没有读完？
                if (size) {
                    size_t realsize = ((size > sizeof(buff)) ? sizeof(buff) : size);
                    size_t readBytesSize = stream->readBytes(buff, realsize);
                    if (len > 0) len -= readBytesSize;
                    outbuf = (uint8_t *)malloc(sizeof(uint8_t) * 5120);
                    uint32_t outprintsize = 0;
                    int result = ArduinoZlib::libmpq__decompress_zlib(buff, readBytesSize, outbuf, 5120, outprintsize);
                    for (int i = 0; i < outprintsize; i++) {
                        payload_1 += (char)outbuf[i];
                    }
                    free(outbuf);
                }
                delay(1);
            }
            // String payload = http.getString();
            #ifdef DEBUG
            Serial.println(payload_1);
            #endif
            _parseNowJson_1(payload_1);
        }
    }
    if (httpCode_2 > 0)
    {
        // file found at server
        if (httpCode_2 == HTTP_CODE_OK)
        {
            String payload_2 = "";
            // 解压Gzip数据流
            int len = http_2.getSize();
            uint8_t buff[2048] = { 0 };
            WiFiClient *stream = http_2.getStreamPtr();
            while (http_2.connected() && (len > 0 || len == -1)) {
                size_t size = stream->available();  // 还剩下多少数据没有读完？
                if (size) {
                    size_t realsize = ((size > sizeof(buff)) ? sizeof(buff) : size);
                    size_t readBytesSize = stream->readBytes(buff, realsize);
                    if (len > 0) len -= readBytesSize;
                    outbuf = (uint8_t *)malloc(sizeof(uint8_t) * 5120);
                    uint32_t outprintsize = 0;
                    int result = ArduinoZlib::libmpq__decompress_zlib(buff, readBytesSize, outbuf, 5120, outprintsize);
                    for (int i = 0; i < outprintsize; i++) {
                        payload_2 += (char)outbuf[i];
                    }
                    free(outbuf);
                }
                delay(1);
            }
            // String payload = http.getString();
            #ifdef DEBUG
            Serial.println(payload_2);
            #endif
            _parseNowJson_2(payload_2);
        }
    }
    else
    {
#ifdef DEBUG
        Serial.printf("[HTTP] GET... failed, error: %s\n", http_1.errorToString(httpCode_1).c_str());
#endif
        return false;
    }
    http_1.end();
    http_2.end();
    return true;
}

// 解析Json信息
void AirQuality::_parseNowJson_1(String payload)
{
    const size_t capacity = 2 * JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(10) + 250;
    DynamicJsonDocument doc(capacity);

    deserializeJson(doc, payload);

    String code = doc["code"].as<const char*>();
    String updateTime = doc["updateTime"].as<const char*>();
    //const char *updateTime = doc["updateTime"];
    JsonObject now = doc["now"];

    _response_code = code; 
    _last_update_str = updateTime; 
    _now_temp = now["temp"].as<int>(); 
    _now_humidity = now["humidity"].as<int>(); 
}

void AirQuality::_parseNowJson_2(String payload) 
{    
    const size_t capacity = 2 * JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(10) + 250;
    DynamicJsonDocument doc(capacity);

    deserializeJson(doc, payload);

    String code = doc["code"].as<const char*>();
    String updateTime = doc["updateTime"].as<const char*>();
    //const char *updateTime = doc["updateTime"];
    JsonObject now = doc["now"];

    _now_aqi_int = now["aqi"].as<int>(); 
    _now_pm2p5_int = now["pm2p5"].as<int>(); 

}

String AirQuality::getServerCode()
{
    return _response_code;
}

String AirQuality::getLastUpdate()
{
    return _last_update_str;
}


int AirQuality::getTemp()
{
    return _now_temp;
}

int AirQuality::getHumidity()
{
    return _now_humidity;
}

int AirQuality::getAqi()
{
    return _now_aqi_int;
}

int AirQuality::getPm2p5()
{
    return _now_pm2p5_int;
}