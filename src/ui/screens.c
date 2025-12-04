#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    {
        lv_obj_t *parent_obj = obj;
        {
            // powerConsumption
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.power_consumption = obj;
            lv_obj_set_pos(obj, 2, 2);
            lv_obj_set_size(obj, 1276, 200);
            lv_obj_add_event_cb(obj, action_show_all_power_consumption, LV_EVENT_PRESSED, (void *)1);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // lbl_battery
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_battery = obj;
                    lv_obj_set_pos(obj, 1147, -13);
                    lv_obj_set_size(obj, 91, 40);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "100%");
                }
                {
                    // bar_actualPercentage
                    lv_obj_t *obj = lv_bar_create(parent_obj);
                    objects.bar_actual_percentage = obj;
                    lv_obj_set_pos(obj, 408, 78);
                    lv_obj_set_size(obj, 803, 49);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff000000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff29865e), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0xffff0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }
                {
                    // lbl_powerConsumption
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_power_consumption = obj;
                    lv_obj_set_pos(obj, 30, 27);
                    lv_obj_set_size(obj, 268, 90);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto_120, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "0");
                }
                {
                    // lbl_kW
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.lbl_k_w = obj;
                    lv_obj_set_pos(obj, 310, 57);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(obj, &ui_font_roboto_80, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "W");
                }
            }
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 650, 220);
            lv_obj_set_size(obj, 600, 230);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 650, 470);
            lv_obj_set_size(obj, 600, 230);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 30, 220);
            lv_obj_set_size(obj, 600, 230);
        }
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            lv_obj_set_pos(obj, 30, 470);
            lv_obj_set_size(obj, 600, 230);
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
}

void create_screen_all_power_consumption() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.all_power_consumption = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 1280, 720);
    
    tick_screen_all_power_consumption();
}

void tick_screen_all_power_consumption() {
}



typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    tick_screen_all_power_consumption,
};
void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    create_screen_main();
    create_screen_all_power_consumption();
}
