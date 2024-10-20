#include <Arduino.h>
#include <lvgl.h>
#include <esp32_smartdisplay.h>
#include "Arial_70.c" // Varmista, että Arial 40 fontti on muunnettu ja tiedosto on oikeassa kansiossa

lv_obj_t *label; // Määritellään muuttuja tekstilabelille
lv_obj_t *btn1, *btn2; // Painikkeet

// Tapahtumakäsittelijä painikkeille
void button_event_handler(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e); // Haetaan painike
    
    // Jos btn1 painetaan, deaktivoi btn2 ja aktivoi btn1
    if (btn == btn1) {
        lv_obj_add_state(btn1, LV_STATE_CHECKED); // Aktivoi btn1
        lv_obj_clear_state(btn2, LV_STATE_CHECKED); // Deaktivoi btn2
        // lv_label_set_text(label, "Otto painettu"); // Päivitä label
    }
    // Jos btn2 painetaan, deaktivoi btn1 ja aktivoi btn2
    else if (btn == btn2) {
        lv_obj_add_state(btn2, LV_STATE_CHECKED); // Aktivoi btn2
        lv_obj_clear_state(btn1, LV_STATE_CHECKED); // Deaktivoi btn1
        // lv_label_set_text(label, "Palautus painettu"); // Päivitä label
    }
}

void setup() {
    smartdisplay_init();

    auto display = lv_display_get_default();
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);

    // Luo taustakappale
    lv_obj_t *background = lv_obj_create(lv_scr_act());
    lv_obj_set_size(background, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(background, lv_color_black(), 0);
    lv_obj_set_style_border_width(background, 0, 0);  // Poista reunat
    lv_obj_set_style_pad_all(background, 0, 0);       // Poista kaikki paddingit
    lv_obj_set_style_radius(background, 0, 0); // Aseta kulmaradius nollaksi

    // Luo globaali tyyli Arial 40 fontilla
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_font(&style, &Arial_70);  // Asetetaan Arial 40 fontti tyyliin
    lv_style_set_text_color(&style, lv_color_white());  // Asetetaan tekstin väri valkoiseksi

    // Aseta tyyli globaalisti taustakappaleeseen
    lv_obj_add_style(background, &style, 0);

    // Luo tyyli painikkeille (ei-aktiivinen tila)
    static lv_style_t btn_style;
    lv_style_init(&btn_style);
    lv_style_set_radius(&btn_style, LV_RADIUS_CIRCLE);  // Aseta painikkeiden pyöristys
    lv_style_set_bg_color(&btn_style, lv_color_make(0, 0, 0)); // Musta tausta ei-aktiiviselle painikkeelle
    lv_style_set_text_font(&btn_style, &Arial_70);  // Arial 40 fontti painikkeissa
    lv_style_set_text_color(&btn_style, lv_color_white());  // Tekstin väri valkoiseksi
    lv_style_set_border_color(&btn_style, lv_color_white()); // Reunan väri valkoiseksi
    lv_style_set_border_width(&btn_style, 2);  // Reunan leveys
    lv_style_set_pad_all(&btn_style, 0); // Poista ylimääräiset marginaalit painikkeilta

    // Luo tyyli aktiiviselle (valitulle) painikkeelle
    static lv_style_t btn_checked_style;
    lv_style_init(&btn_checked_style);
    lv_style_set_radius(&btn_checked_style, LV_RADIUS_CIRCLE);  // Pyöristys säilyy
    lv_style_set_bg_color(&btn_checked_style, lv_color_make(255, 200, 0));  // Keltainen väri (RGB: 255, 255, 0)
    lv_style_set_text_font(&btn_checked_style, &Arial_70);  // Arial 40 fontti aktiivisessa painikkeessa
    lv_style_set_text_color(&btn_checked_style, lv_color_black());  // Tekstin väri mustaksi aktiivisessa tilassa
    lv_style_set_border_color(&btn_checked_style, lv_color_white()); // Reunan väri valkoiseksi
    lv_style_set_border_width(&btn_checked_style, 2);  // Reunan leveys
    lv_style_set_pad_all(&btn_checked_style, 0); // Poista ylimääräiset marginaalit aktiivisilta painikkeilta

    // Luo label
    label = lv_label_create(background);
    lv_label_set_text(label, "Lue tuote");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10); // Asetetaan label yläreunaan keskelle
    lv_obj_add_style(label, &style, 0);  // Asetetaan Arial 40 -fontin tyyli labeliin

    // Luo ensimmäinen painike (Otto)
    btn1 = lv_btn_create(background);
    lv_obj_set_size(btn1, 400, 160);  // Painikkeen koko
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -100); // Keskelle ylös
    lv_obj_add_event_cb(btn1, button_event_handler, LV_EVENT_CLICKED, NULL); // Lisää tapahtumakäsittelijä
    lv_obj_add_flag(btn1, LV_OBJ_FLAG_CHECKABLE); // Aseta painike "toggle"-tilaan
    lv_obj_add_style(btn1, &btn_style, 0); // Lisää ei-aktiivinen tyyli painikkeeseen
    lv_obj_add_style(btn1, &btn_checked_style, LV_STATE_CHECKED); // Lisää aktiivinen (kelta) tyyli

    // Luo painikkeen label ja aseta Arial 40 -fontti
    lv_obj_t *label_btn1 = lv_label_create(btn1);
    lv_label_set_text(label_btn1, "Otto"); // Asetetaan painikkeen teksti
    lv_obj_center(label_btn1); // Keskitetään label painikkeeseen
    lv_obj_add_style(label_btn1, &style, 0);  // Asetetaan Arial 40 -fontin tyyli

    // Luo toinen painike (Palautus)
    btn2 = lv_btn_create(background);
    lv_obj_set_size(btn2, 400, 160);  // Painikkeen koko
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 100); // Keskelle alas
    lv_obj_add_event_cb(btn2, button_event_handler, LV_EVENT_CLICKED, NULL); // Lisää tapahtumakäsittelijä
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE); // Aseta painike "toggle"-tilaan
    lv_obj_add_style(btn2, &btn_style, 0); // Lisää ei-aktiivinen tyyli painikkeeseen
    lv_obj_add_style(btn2, &btn_checked_style, LV_STATE_CHECKED); // Lisää aktiivinen (kelta) tyyli

    // Luo toisen painikkeen label ja aseta Arial 40 -fontti
    lv_obj_t *label_btn2 = lv_label_create(btn2);
    lv_label_set_text(label_btn2, "Palautus"); // Asetetaan painikkeen teksti
    lv_obj_center(label_btn2); // Keskitetään label painikkeeseen
    lv_obj_add_style(label_btn2, &style, 0);  // Asetetaan Arial 40 -fontin tyyli

    // Oletuksena painike 1 (Otto) on aktiivinen
    lv_obj_add_state(btn1, LV_STATE_CHECKED); // Painike 1 on aktiivinen alussa
}

auto lv_last_tick = millis();

void loop() {
    unsigned long now = millis(); // Saadaan nykyinen aikaleima
    // Update the ticker
    lv_tick_inc(now - lv_last_tick);
    lv_last_tick = now;
    // Päivitä käyttöliittymä
    lv_timer_handler();
}
