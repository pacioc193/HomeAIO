#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main_2;
    lv_obj_t *panel_sidebar;
    lv_obj_t *lbl_power_title;
    lv_obj_t *lbl_power_val;
    lv_obj_t *lbl_power_unit;
    lv_obj_t *bar_power;
    lv_obj_t *cont_rooms;
    lv_obj_t *panel_room_1;
    lv_obj_t *lbl_name_camera_da_letto;
    lv_obj_t *lbl_temp_camera_da_letto;
    lv_obj_t *panel_room_2;
    lv_obj_t *lbl_name_salotto;
    lv_obj_t *lbl_temp_salotto;
    lv_obj_t *panel_room_3;
    lv_obj_t *lbl_name_cameretta;
    lv_obj_t *lbl_temp_cameretta;
    lv_obj_t *panel_room_4;
    lv_obj_t *lbl_name_bagno;
    lv_obj_t *lbl_temp_bagno;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN_2 = 1,
};

void create_screen_main_2();
void tick_screen_main_2();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/