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
const char *xmlUrl = "https://opendata.fmi.fi/wfs?service=WFS&version=2.0.0&request=getFeature&storedquery_id=fmi::forecast::harmonie::surface::point::multipointcoverage&place=Jorvi,Espoo&parameters=Temperature";  // Korvaa omalla URL:lla

// Funktioiden etukäteisilmoitus
void connectWiFi();
std::vector<float> extractTemperatureData(const String &xml);
void fetchAndUpdateTemperatures();
void printLocalTime();

// Yhdistä Wi-Fi-verkkoon
void connectWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
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

        // Poistetaan mahdolliset ylimääräiset välilyönnit alusta ja lopusta
        data.trim();
        
        // Käydään data läpi ja pilkotaan se välilyönnin perusteella
        while (data.length() > 0) {
            int index = data.indexOf(" ");
            String tempStr;

            if (index != -1) {
                tempStr = data.substring(0, index);  // Ota seuraava lämpötila
                data = data.substring(index + 1);  // Siirry seuraavaan osaan
            } else {
                tempStr = data;  // Viimeinen lämpötila
                data = "";  // Tyhjennä data merkiksi lopetuksesta
            }

            // Varmistetaan, ettei tyhjiä, virheellisiä tai ei-numerisia arvoja oteta mukaan
            tempStr.trim();  // Poista välilyönnit
            if (tempStr.length() > 0 && tempStr.toFloat() != 0.0f) {
                float temp = tempStr.toFloat();
                if (temp != 0.0f || tempStr == "0.0") {  // Varmista, että nolla lisätään vain oikeasta nollasta
                    temperatures.push_back(temp);  // Lisää lämpötila vektoriin
                }
            }
        }
    }

    return temperatures;  // Palautetaan lämpötilalista
}

void fetchAndUpdateTemperatures() {
    HTTPClient http;
    http.begin(xmlUrl);  // Asetetaan URL
    int httpResponseCode = http.GET();  // Tehdään GET-pyyntö

    if (httpResponseCode > 0) {
        String payload = http.getString();
        
        // Hae lämpötilatiedot ja aseta ne graafiin
        std::vector<float> temperatures = extractTemperatureData(payload);
        
        if (!temperatures.empty()) {
            // Aseta graafin pisteiden määrä lämpötilojen määrän mukaan
            lv_chart_set_point_count(chart, temperatures.size());

            // Asetetaan kaikki lämpötilat sarjaan
            for (int i = 0; i < temperatures.size(); i++) {
                lv_chart_set_value_by_id(chart, temp_series, i, temperatures[i]);  // Aseta lämpötiladata sarjaan indekseittäin
                Serial.print(temperatures[i]);
                if (i < temperatures.size() - 1) {
                    Serial.print(";");  // Erottele pilkulla ja välilyönnillä
                }
            }
            
            // Näytä ensimmäinen lämpötila keskellä näyttöä
            char tempStr[16];
            snprintf(tempStr, sizeof(tempStr), "%.1f°C", temperatures[0]); // Muotoile lämpötila merkkijonoksi
            lv_label_set_text(temperature_label, tempStr); // Aseta lämpötila labeliin

            // Päivitä graafi, jotta viivakäyrä piirretään
            lv_chart_refresh(chart);  
        } else {
            Serial.println("Lämpötilatietoja ei löytynyt.");
        }
    } else {
        Serial.print("Virhe XML:n hakemisessa, koodi: ");
        Serial.println(httpResponseCode);
    }
    http.end();  // Vapautetaan resurssit
}

// Päivitä käyttöliittymän kellonaika ja päivämäärä
void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    
    // Muodosta kellonaika tekstinä ilman johtavaa nollaa
    char timeStr[64];
    if (timeinfo.tm_hour < 10) {
        snprintf(timeStr, sizeof(timeStr), "%d:%02d.%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);  // Ei johtavaa nollaa
    } else {
        strftime(timeStr, sizeof(timeStr), "%H:%M.%S", &timeinfo);  // Käytä normaalia muotoa
    }
    
    // Muodosta päivämäärä suomenkielisessä muodossa
    char dateStr[64];
    strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo); // Muoto: päivämäärä.kk.vuosi

    // Aseta päivämäärä labeliin
    lv_label_set_text(date_label, dateStr);  // Näytetään vain päivämäärä

    // Muodosta kellonaika merkkijonoksi ja aseta se labeliin
    String timeLabelText = timeStr;
    lv_label_set_text(time_label, timeLabelText.c_str()); // Muuta C-tyyppiseksi merkkijonoksi
}

void setup() {
    Serial.begin(115200);
    smartdisplay_init();

    auto display = lv_display_get_default();
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);

    // Yhdistä Wi-Fi-verkkoon
    connectWiFi();

    // Aseta ajanhaku NTP-serveriltä
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Luo uusi tyyli fontille
    static lv_style_t font_style;
    lv_style_init(&font_style);
    lv_style_set_text_font(&font_style, &Arial_70);  // Käytä Arial 70 -fonttia
    lv_style_set_text_color(&font_style, LV_COLOR_MAKE(255, 255, 255)); // Aseta tekstiväri valkoiseksi

    // Luo uusi tyyli kellonajalle
    static lv_style_t time_style;
    lv_style_init(&time_style);
    lv_style_set_text_font(&time_style, &Arial_100);  // Käytä Arial 100 -fonttia
    lv_style_set_text_color(&time_style, LV_COLOR_MAKE(255, 255, 255)); // Aseta kellon tekstiväri valkoiseksi

    // Luo uusi tyyli taustalle
    static lv_style_t bg_style;
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, LV_COLOR_MAKE(0, 0, 0)); // Aseta taustaväri mustaksi

    // Aseta taustatyylit koko näytölle
    lv_obj_add_style(lv_scr_act(), &bg_style, 0); // Aseta musta tausta koko näytölle

    // Luo label kellonajalle
    time_label = lv_label_create(lv_scr_act());
    lv_label_set_text(time_label, "00:00.00");  // Alustava teksti
    lv_obj_align(time_label, LV_ALIGN_TOP_MID, 0, 115);  // Asetetaan keskelle ylös
    lv_obj_add_style(time_label, &time_style, 0);  // Lisää fonttityyli kelloon

    // Luo label päivämääralle
    date_label = lv_label_create(lv_scr_act());
    lv_label_set_text(date_label, "--.--.----");  // Alustava teksti
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 20);  // Asetetaan kellon alapuolelle
    lv_obj_add_style(date_label, &font_style, 0);  // Lisää fonttityyli päivämäärälle

    // Luo graafi lämpötilalukujonolle
    chart = lv_chart_create(lv_scr_act()); // Luo uusi graafi
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);  // Viivagraafi
    lv_obj_set_size(chart, 450, 300); // Aseta graafin koko 
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, 0); // Keskitetään graafi

    // Luo uusi tyyli graafia varten
    static lv_style_t chart_bg_style;
    lv_style_init(&chart_bg_style);

    // Aseta taustaväriksi musta
    lv_style_set_bg_color(&chart_bg_style, LV_COLOR_MAKE(0, 0, 0)); // Taustaväri mustaksi
    lv_style_set_bg_opa(&chart_bg_style, LV_OPA_COVER);  // Peittää koko taustan

    // Poista pyöristykset asettamalla kulmien radius nollaksi
    lv_style_set_radius(&chart_bg_style, 0);  // Ei pyöristyksiä

    // Aseta reunojen paksuudeksi 0 (ei näkyviä reunoja)
    lv_style_set_border_width(&chart_bg_style, 0);  // Poistaa reunat kokonaan

    // Aseta tyyli graafiin
    lv_obj_add_style(chart, &chart_bg_style, 0);  // Lisää tyyli graafiin

    // Luo uusi tyyli akselien väriä varten
    static lv_style_t chart_axis_style;
    lv_style_init(&chart_axis_style);

    // Määritä akselin asteikon väriksi esimerkiksi valkoinen
    lv_style_set_text_color(&chart_axis_style, LV_COLOR_MAKE(32, 32, 32)); // Asetetaan asteikkojen ja tekstin väriksi valkoinen
    lv_style_set_line_color(&chart_axis_style, LV_COLOR_MAKE(32, 32, 32)); // Asetetaan asteikon viivojen väriksi valkoinen

    // Aseta tyyli graafiin
    lv_obj_add_style(chart, &chart_axis_style, LV_PART_ITEMS);  // Tyyli asteikkojen viivoille

    // Lisää sarja graafiin ja määritä akseli
    temp_series = lv_chart_add_series(chart, LV_COLOR_MAKE(255, 192, 128), LV_CHART_AXIS_PRIMARY_X); // Lisää sarja graafiin
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -30, 30);  // Määritä akselin arvoalue esim. -30°C - 30°C

    // Asetetaan ruudukko viiden asteen välein Y-akselille
    lv_chart_set_div_line_count(chart, 7, 2);  // X-akselille 10 ja Y-akselille 12 ruutua (60°C jaettu 5°C välein)

    // Luo label lämpötilalle
    temperature_label = lv_label_create(lv_scr_act());
    lv_label_set_text(temperature_label, "--.-°C");  // Alustava teksti
    lv_obj_align(temperature_label, LV_ALIGN_CENTER, 0, 0);  // Asetetaan keskelle näyttöä
    lv_obj_add_style(temperature_label, &time_style, 0);  // Käytetään samaa tyyliä kuin kellossa

    // Hae ja näytä lämpötilalukujono
    fetchAndUpdateTemperatures();  // Hae kerralla kaikki lämpötilat
}

auto lv_last_tick = millis();

void loop() {
    unsigned long now = millis();
    lv_tick_inc(now - lv_last_tick);
    lv_last_tick = now;
    
    // Päivitä käyttöliittymä
    lv_timer_handler();

    // Päivitä kellonaika ja päivämäärä näyttöön
    printLocalTime();
    
    // Ei tarvitse hakea XML:ää joka iteraatiolla. Hakee vain alussa.
    delay(1000);  // Päivitä kerran sekunnissa
}
