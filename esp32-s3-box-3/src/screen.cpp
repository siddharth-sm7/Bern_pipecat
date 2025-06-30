#include <bsp/esp-bsp.h>

#include <lvgl.h>

#include "main.h"

static lv_obj_t *screen = NULL;
static lv_obj_t *log_container = NULL;

lv_style_t STYLE_BLUE;
lv_style_t STYLE_GREEN;
lv_style_t STYLE_DEFAULT;

static void create_scrollable_log(lv_obj_t *parent) {
    log_container = lv_obj_create(parent);
    lv_obj_set_size(log_container, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_align(log_container, LV_ALIGN_CENTER, 0, 0);
    // Enable vertical scrolling
    lv_obj_set_scroll_dir(log_container, LV_DIR_VER);
    // Enable flex layout
    lv_obj_set_layout(log_container, LV_LAYOUT_FLEX);
    // Vertical stacking
    lv_obj_set_flex_flow(log_container, LV_FLEX_FLOW_COLUMN);
    // Space between rows
    lv_obj_set_style_pad_row(log_container, 4, 0);
}

static void init_styles() {
    lv_style_init(&STYLE_BLUE);
    lv_style_set_text_color(&STYLE_BLUE, lv_color_hex(0x3333FF));

    lv_style_init(&STYLE_GREEN);
    lv_style_set_text_color(&STYLE_GREEN, lv_color_hex(0x33FF33));

    lv_style_init(&STYLE_DEFAULT);
    lv_style_set_text_color(&STYLE_DEFAULT, lv_color_hex(0x333333));
}

void pipecat_init_screen() {
    ESP_LOGI(LOG_TAG, "Free heap: %lu", esp_get_free_heap_size());

    bsp_display_start();

    bsp_display_backlight_on();

    bsp_display_lock(0);

    init_styles();

    screen = lv_scr_act();

    create_scrollable_log(screen);

    ESP_LOGI(LOG_TAG, "Display initialized");
}

void pipecat_screen_loop() {
  lv_timer_handler();
}

void pipecat_screen_system_log(const char *text) {
  if (!log_container) {
    return;
  }
  pipecat_screen_add_log(text, &STYLE_DEFAULT);
}

void pipecat_screen_add_log(const char *text, const lv_style_t *style) {
  if (!log_container) {
    return;
  }

    lv_obj_t *label = lv_label_create(log_container);
    lv_label_set_text(label, text);

    if (style != NULL) {
        lv_obj_add_style(label, style, 0);
    }

    // Scroll to bottom to show the latest message
    lv_obj_scroll_to_view_recursive(label, LV_ANIM_ON);
}
