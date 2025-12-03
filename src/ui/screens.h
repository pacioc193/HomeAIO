#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *all_power_consumption;
    lv_obj_t *power_consumption;
    lv_obj_t *lbl_battery;
    lv_obj_t *bar_actual_percentage;
    lv_obj_t *lbl_power_consumption;
    lv_obj_t *lbl_k_w;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_ALL_POWER_CONSUMPTION = 2,
};

void create_screen_main();
void tick_screen_main();

void create_screen_all_power_consumption();
void tick_screen_all_power_consumption();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/