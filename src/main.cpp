#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <esp32_smartdisplay.h>
#include "time.h"
#include "Arial_70.c"
#include "Arial_100.c"

// Wi-Fi asetukset
const char *ssid = "Net-Guest";
const char *password = "swissreader";

// Ajanhakuasetukset
const char* ntpServer = "fi.pool.ntp.org";
const long gmtOffset_sec = 7200; // Aseta aikavyöhyke
const int daylightOffset_sec = 3600; // Kesäaika, jos tarpeen

lv_obj_t *time_label;  // Label kellonajalle
lv_obj_t *date_label;  // Label päivämäärälle
lv_obj_t *chart;       // Graafi lämpötilalukujonolle
lv_chart_series_t *temp_series; // Sarja lämpötiloille
lv_obj_t *temperature_label; // Label lämpötilalle

// URL josta XML haetaan
const char *xmlUrl = "https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&storedquery_id=fmi::forecast::harmonie::surface::point::multipointcoverage&place=Jorvi,Espoo&parameters=Temperature";

// Funktioiden etukäteisilmoitus
void connectWiFi();
std::vector<float> extractTemperatureData(const String &xml);
void fetchAndUpdateTemperatures();
void printLocalTime();
void updateLocalTime();

// Ajastimet
unsigned long lastNtpSync = 0; // Viimeisin NTP-synkronointi
unsigned long lastTempUpdate = 0; // Viimeisin lämpötilapäivitys

// Yhdistä Wi-Fi-verkkoon
void connectWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
}

TaskHandle_t tempFetchTask;

void fetchAndUpdateTemperaturesTask(void *parameter) {
    while (true) {
        fetchAndUpdateTemperatures(); // Hakee ja päivittää lämpötilat graafiin
        vTaskDelay(300000 / portTICK_PERIOD_MS); // Odota 5 minuuttia ennen seuraavaa hakua
    }
}

// Hae lämpötiladata
std::vector<float> extractTemperatureData(const String &xml) {
    std::vector<float> temperatures;

    // Etsi <gml:doubleOrNilReasonTupleList> -tagit
    int start = xml.indexOf("<gml:doubleOrNilReasonTupleList>");
    int end = xml.indexOf("</gml:doubleOrNilReasonTupleList>");

    if (start != -1 && end != -1) {
        // Siirry sisällön alkuun
        start += String("<gml:doubleOrNilReasonTupleList>").length();
        String data = xml.substring(start, end); // Poimi sisältö
        data.trim();

        // Käydään data läpi ja pilkotaan se välilyönnin perusteella
        while (data.length() > 0) {
            int index = data.indexOf(" ");
            String tempStr;

            if (index != -1) {
                tempStr = data.substring(0, index);
                data = data.substring(index + 1);
            } else {
                tempStr = data;
                data = "";
            }

            tempStr.trim();
            if (tempStr.length() > 0 && tempStr.toFloat() != 0.0f) {
                float temp = tempStr.toFloat();
                if (temp != 0.0f || tempStr == "0.0") {
                    temperatures.push_back(temp);
                }
            }
        }
    }

    return temperatures;
}

void fetchAndUpdateTemperatures() {
    HTTPClient http;
    http.begin(xmlUrl);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String payload = http.getString();
        std::vector<float> temperatures = extractTemperatureData(payload);

        if (!temperatures.empty()) {
            lv_chart_set_point_count(chart, temperatures.size());

            for (int i = 0; i < temperatures.size(); i++) {
                lv_chart_set_value_by_id(chart, temp_series, i, temperatures[i]);
                Serial.print(temperatures[i]);
                if (i < temperatures.size() - 1) {
                    Serial.print("; ");
                }
            }

            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%.1f°C", temperatures[0]);
            lv_label_set_text(temperature_label, tempStr);

            lv_chart_refresh(chart);
        } else {
            Serial.println("Lämpötilatietoja ei löytynyt.");
        }
    } else {
        Serial.print("Virhe XML:n hakemisessa, koodi: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }

    char timeStr[64];
    if (timeinfo.tm_hour < 10) {
        snprintf(timeStr, sizeof(timeStr), "%d:%02d.%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        strftime(timeStr, sizeof(timeStr), "%H:%M.%S", &timeinfo);
    }

    char dateStr[64];
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

    lv_label_set_text(date_label, dateStr);
    lv_label_set_text(time_label, timeStr);
}

// Päivitä aika NTP-palvelimelta kerran päivässä
const int gmtOffset_sec_winter = 7200;  // UTC+2 talviaika
const int gmtOffset_sec_summer = 10800; // UTC+3 kesäaika

void updateLocalTime() {
    time_t now = time(nullptr);
    struct tm *currentTime = localtime(&now);

    // Tarkista, onko kesäaika (maaliskuun viimeinen sunnuntai - lokakuun viimeinen sunnuntai)
    bool isSummerTime = (currentTime->tm_mon > 2 && currentTime->tm_mon < 9) || 
                        (currentTime->tm_mon == 2 && currentTime->tm_mday >= (31 - (5 * currentTime->tm_wday + 6) / 8)) ||
                        (currentTime->tm_mon == 9 && currentTime->tm_mday < (31 - (5 * currentTime->tm_wday + 6) / 8));

    int gmtOffset_sec = isSummerTime ? gmtOffset_sec_summer : gmtOffset_sec_winter;
    
    configTime(gmtOffset_sec, 0, ntpServer);
    lastNtpSync = millis();
}

void setup() {
    Serial.begin(115200);
    smartdisplay_init();

    auto display = lv_display_get_default();
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);

    connectWiFi();

    // Aseta ajanhaku NTP-serveriltä
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Luo taustatehtävä lämpötilojen hakemiselle ja päivittämiselle
    xTaskCreatePinnedToCore(
        fetchAndUpdateTemperaturesTask, // Tehtäväfunktio
        "TempFetchTask",                // Tehtävän nimi
        10000,                          // Pino
        NULL,                           // Parametrit
        1,                              // Prioriteetti
        &tempFetchTask,                 // Tehtävän käsittelijä
        1                               // Suoritetaan toisessa ytimessä
    );

    // Päivitä aika NTP-palvelimelta kerran vuorokaudessa
    updateLocalTime();

    static lv_style_t font_style;
    lv_style_init(&font_style);
    lv_style_set_text_font(&font_style, &Arial_70);
    lv_style_set_text_color(&font_style, LV_COLOR_MAKE(255, 255, 255));

    static lv_style_t time_style;
    lv_style_init(&time_style);
    lv_style_set_text_font(&time_style, &Arial_100);
    lv_style_set_text_color(&time_style, LV_COLOR_MAKE(255, 255, 255));

    static lv_style_t bg_style;
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, LV_COLOR_MAKE(0, 0, 0));

    lv_obj_add_style(lv_scr_act(), &bg_style, 0);

    time_label = lv_label_create(lv_scr_act());
    lv_label_set_text(time_label, "00:00.00");
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 115);
    lv_obj_add_style(time_label, &time_style, 0);

    date_label = lv_label_create(lv_scr_act());
    lv_label_set_text(date_label, "--.--.----");
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_style(date_label, &font_style, 0);

    chart = lv_chart_create(lv_scr_act());
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_obj_set_size(chart, 450, 300);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, 0);

    static lv_style_t chart_bg_style;
    lv_style_init(&chart_bg_style);
    lv_style_set_bg_color(&chart_bg_style, LV_COLOR_MAKE(0, 0, 0));
    lv_style_set_bg_opa(&chart_bg_style, LV_OPA_COVER);
    lv_style_set_radius(&chart_bg_style, 0);
    lv_style_set_border_width(&chart_bg_style, 0);

    lv_obj_add_style(chart, &chart_bg_style, 0);

    static lv_style_t chart_axis_style;
    lv_style_init(&chart_axis_style);
    lv_style_set_text_color(&chart_axis_style, LV_COLOR_MAKE(32, 32, 32));
    lv_style_set_line_color(&chart_axis_style, LV_COLOR_MAKE(32, 32, 32));

    lv_obj_add_style(chart, &chart_axis_style, LV_PART_ITEMS);

    temp_series = lv_chart_add_series(chart, LV_COLOR_MAKE(255, 192, 128), LV_CHART_AXIS_PRIMARY_X);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -30, 30);
    lv_chart_set_div_line_count(chart, 7, 2);

    temperature_label = lv_label_create(lv_scr_act());
    lv_label_set_text(temperature_label, "--.-°C");
    lv_obj_align(temperature_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(temperature_label, &time_style, 0);

    fetchAndUpdateTemperatures();
    lastTempUpdate = millis();
}

auto lv_last_tick = millis();

void loop() {
    unsigned long now = millis();
    lv_tick_inc(now - lv_last_tick);
    lv_last_tick = now;

    lv_timer_handler();

    if (now - lastTempUpdate > 300000) { // Päivitä 5 minuutin välein
        fetchAndUpdateTemperatures();
        lastTempUpdate = now;
    }

    if (now - lastNtpSync > 86400000) { // Synkronoi NTP-palvelimen kanssa kerran päivässä
        updateLocalTime();
    }

    printLocalTime();
}
