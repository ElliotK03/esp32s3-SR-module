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

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

static void event_handler_cb_main_obj0(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_arc_get_value(ta);
            set_var_timer_arc_value(value);
        }
    }
}

static void event_handler_cb_settings_screen_brightness_slider(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            set_var_screen_brightness_val(value);
        }
    }
}

static void event_handler_cb_settings_volume_slider(lv_event_t *e) {
    lv_event_code_t event = lv_event_get_code(e);
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *ta = lv_event_get_target_obj(e);
        if (tick_value_change_obj != ta) {
            int32_t value = lv_slider_get_value(ta);
            set_var_volume(value);
        }
    }
}

//
// Screens
//

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // container_start_pomo_again
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_start_pomo_again = obj;
            lv_obj_set_pos(obj, 0, 204);
            lv_obj_set_size(obj, 240, 116);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // plus_button_1
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.plus_button_1 = obj;
                    lv_obj_set_pos(obj, 127, 0);
                    lv_obj_set_size(obj, 90, 47);
                    lv_obj_add_event_cb(obj, action_button_plus_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, 17, 1);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_plus_bitmap);
                        }
                    }
                }
                {
                    // minus_button_1
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.minus_button_1 = obj;
                    lv_obj_set_pos(obj, 24, 0);
                    lv_obj_set_size(obj, 90, 47);
                    lv_obj_add_event_cb(obj, action_button_minus_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, 18, 1);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_minus_bitmap);
                        }
                    }
                }
                {
                    // pomo_start_end_button_1
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.pomo_start_end_button_1 = obj;
                    lv_obj_set_pos(obj, 82, 58);
                    lv_obj_set_size(obj, 135, 47);
                    lv_obj_add_event_cb(obj, action_button_start_pomo_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_width(obj, 135, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, 37, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_play_arrow_bitmap);
                        }
                    }
                }
                {
                    // pomo_end_session_pressed_2
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.pomo_end_session_pressed_2 = obj;
                    lv_obj_set_pos(obj, 24, 58);
                    lv_obj_set_size(obj, 45, 47);
                    lv_obj_add_event_cb(obj, action_button_end_session_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 45, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, -8, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_stop_bitmap);
                        }
                    }
                }
            }
        }
        {
            // container_pomo_resting
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_pomo_resting = obj;
            lv_obj_set_pos(obj, 0, 204);
            lv_obj_set_size(obj, 240, 116);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // button_fast_forward_resting_time
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.button_fast_forward_resting_time = obj;
                    lv_obj_set_pos(obj, 75, 58);
                    lv_obj_set_size(obj, 90, 47);
                    lv_obj_add_event_cb(obj, action_button_fast_forward_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x7b68ee), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // icon_fast_forward
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.icon_fast_forward = obj;
                            lv_obj_set_pos(obj, 15, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_ff_bitmap);
                        }
                    }
                }
                {
                    // pomo_end_session_pressed_1
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.pomo_end_session_pressed_1 = obj;
                    lv_obj_set_pos(obj, 35, 0);
                    lv_obj_set_size(obj, 70, 47);
                    lv_obj_add_event_cb(obj, action_button_end_session_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x7b68ee), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // icon_end_session_1
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.icon_end_session_1 = obj;
                            lv_obj_set_pos(obj, 5, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_stop_bitmap);
                        }
                    }
                }
                {
                    // button_plus_5
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.button_plus_5 = obj;
                    lv_obj_set_pos(obj, 136, 0);
                    lv_obj_set_size(obj, 72, 47);
                    lv_obj_add_event_cb(obj, action_plus_5_button_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x7b68ee), LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text_static(obj, "+5");
                        }
                    }
                }
            }
        }
        {
            // container_pomo_running
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_pomo_running = obj;
            lv_obj_set_pos(obj, 0, 204);
            lv_obj_set_size(obj, 240, 116);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // button_start_resume_pressed
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.button_start_resume_pressed = obj;
                    lv_obj_set_pos(obj, 60, 58);
                    lv_obj_set_size(obj, 120, 47);
                    lv_obj_add_event_cb(obj, action_button_start_resume_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_height(obj, 47, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // icon_start_resume
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.icon_start_resume = obj;
                            lv_obj_set_pos(obj, 30, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_play_arrow_bitmap);
                        }
                    }
                }
                {
                    // pomo_end_session_pressed
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.pomo_end_session_pressed = obj;
                    lv_obj_set_pos(obj, 60, 0);
                    lv_obj_set_size(obj, 120, 47);
                    lv_obj_add_event_cb(obj, action_button_end_session_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 120, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_height(obj, 47, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            // icon_end_session
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            objects.icon_end_session = obj;
                            lv_obj_set_pos(obj, 30, -2);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_stop_bitmap);
                        }
                    }
                }
            }
        }
        {
            // container_start_pomo
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_start_pomo = obj;
            lv_obj_set_pos(obj, 0, 204);
            lv_obj_set_size(obj, 240, 116);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // pomo_start_end_button
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.pomo_start_end_button = obj;
                    lv_obj_set_pos(obj, 24, 58);
                    lv_obj_set_size(obj, 193, 47);
                    lv_obj_add_event_cb(obj, action_button_start_pomo_pressed, LV_EVENT_PRESSED, (void *)0);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_label_create(parent_obj);
                            objects.obj1 = obj;
                            lv_obj_set_pos(obj, 0, 0);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_label_set_text(obj, "");
                        }
                    }
                }
                {
                    // plus_button
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.plus_button = obj;
                    lv_obj_set_pos(obj, 127, 0);
                    lv_obj_set_size(obj, 90, 47);
                    lv_obj_add_event_cb(obj, action_button_plus_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, 17, 1);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_plus_bitmap);
                        }
                    }
                }
                {
                    // minus_button_2
                    lv_obj_t *obj = lv_button_create(parent_obj);
                    objects.minus_button_2 = obj;
                    lv_obj_set_pos(obj, 24, 0);
                    lv_obj_set_size(obj, 90, 47);
                    lv_obj_add_event_cb(obj, action_button_minus_pressed, LV_EVENT_PRESSED, (void *)0);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
                    {
                        lv_obj_t *parent_obj = obj;
                        {
                            lv_obj_t *obj = lv_image_create(parent_obj);
                            lv_obj_set_pos(obj, 18, 1);
                            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                            lv_image_set_src(obj, &img_minus_bitmap);
                        }
                    }
                }
            }
        }
        {
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.obj0 = obj;
            lv_obj_set_pos(obj, 30, 13);
            lv_obj_set_size(obj, 180, 182);
            lv_arc_set_range(obj, 0, 360);
            lv_arc_set_bg_start_angle(obj, 0);
            lv_arc_set_bg_end_angle(obj, 360);
            lv_obj_add_event_cb(obj, event_handler_cb_main_obj0, LV_EVENT_ALL, 0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
        }
        {
            // time_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.time_text = obj;
            lv_obj_set_pos(obj, 70, 79);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_30, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_height(obj, 50, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj2 = obj;
            lv_obj_set_pos(obj, 20, 129);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_height(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.obj3 = obj;
            lv_obj_set_pos(obj, 81, 51);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_width(obj, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_height(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
    }
    
    tick_screen_main();
}

void tick_screen_main() {
    {
        bool new_val = get_var_start_pomo_again_container_enable();
        bool cur_val = lv_obj_has_flag(objects.container_start_pomo_again, LV_OBJ_FLAG_HIDDEN);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.container_start_pomo_again;
            if (new_val) {
                lv_obj_add_flag(objects.container_start_pomo_again, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(objects.container_start_pomo_again, LV_OBJ_FLAG_HIDDEN);
            }
            tick_value_change_obj = NULL;
        }
    }
    {
        bool new_val = get_var_pomo_resting_container_enable();
        bool cur_val = lv_obj_has_flag(objects.container_pomo_resting, LV_OBJ_FLAG_HIDDEN);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.container_pomo_resting;
            if (new_val) {
                lv_obj_add_flag(objects.container_pomo_resting, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(objects.container_pomo_resting, LV_OBJ_FLAG_HIDDEN);
            }
            tick_value_change_obj = NULL;
        }
    }
    {
        bool new_val = get_var_plus_5_button_disabled();
        bool cur_val = lv_obj_has_state(objects.button_plus_5, LV_STATE_DISABLED);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.button_plus_5;
            if (new_val) {
                lv_obj_add_state(objects.button_plus_5, LV_STATE_DISABLED);
            } else {
                lv_obj_remove_state(objects.button_plus_5, LV_STATE_DISABLED);
            }
            tick_value_change_obj = NULL;
        }
    }
    {
        bool new_val = get_var_pomo_running_container_enable();
        bool cur_val = lv_obj_has_flag(objects.container_pomo_running, LV_OBJ_FLAG_HIDDEN);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.container_pomo_running;
            if (new_val) {
                lv_obj_add_flag(objects.container_pomo_running, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(objects.container_pomo_running, LV_OBJ_FLAG_HIDDEN);
            }
            tick_value_change_obj = NULL;
        }
    }
    {
        bool new_val = get_var_start_pomo_container_enable();
        bool cur_val = lv_obj_has_flag(objects.container_start_pomo, LV_OBJ_FLAG_HIDDEN);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.container_start_pomo;
            if (new_val) {
                lv_obj_add_flag(objects.container_start_pomo, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(objects.container_start_pomo, LV_OBJ_FLAG_HIDDEN);
            }
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_session_start_stop_button_str();
        const char *cur_val = lv_label_get_text(objects.obj1);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj1;
            lv_label_set_text(objects.obj1, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_timer_arc_value();
        int32_t cur_val = lv_arc_get_value(objects.obj0);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.obj0;
            lv_arc_set_value(objects.obj0, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_display_tim_str();
        const char *cur_val = lv_label_get_text(objects.time_text);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.time_text;
            lv_label_set_text(objects.time_text, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_tim_user_text_str();
        const char *cur_val = lv_label_get_text(objects.obj2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj2;
            lv_label_set_text(objects.obj2, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_curr_streak_str();
        const char *cur_val = lv_label_get_text(objects.obj3);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.obj3;
            lv_label_set_text(objects.obj3, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_clock() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.clock = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            // clock_text
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.clock_text = obj;
            lv_obj_set_pos(obj, 26, 69);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // clock_text_1
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.clock_text_1 = obj;
            lv_obj_set_pos(obj, 60, 146);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // clock_text_2
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.clock_text_2 = obj;
            lv_obj_set_pos(obj, 79, 200);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
    }
    
    tick_screen_clock();
}

void tick_screen_clock() {
    {
        const char *new_val = get_var_clcok_str();
        const char *cur_val = lv_label_get_text(objects.clock_text);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.clock_text;
            lv_label_set_text(objects.clock_text, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_date_str();
        const char *cur_val = lv_label_get_text(objects.clock_text_1);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.clock_text_1;
            lv_label_set_text(objects.clock_text_1, new_val);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_day_str();
        const char *cur_val = lv_label_get_text(objects.clock_text_2);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.clock_text_2;
            lv_label_set_text(objects.clock_text_2, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

void create_screen_alarm() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.alarm = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 80, 104);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_label_set_text_static(obj, "Alarm");
        }
        {
            lv_obj_t *obj = lv_keyboard_create(parent_obj);
            lv_obj_set_pos(obj, 10, 160);
            lv_obj_set_size(obj, 220, 120);
            lv_obj_set_style_align(obj, LV_ALIGN_DEFAULT, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
    
    tick_screen_alarm();
}

void tick_screen_alarm() {
}

void create_screen_settings() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.settings = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 240, 320);
    {
        lv_obj_t *parent_obj = obj;
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 69, 6);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Settings");
        }
        {
            // screen_brightness_slider
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.screen_brightness_slider = obj;
            lv_obj_set_pos(obj, 36, 68);
            lv_obj_set_size(obj, 187, 10);
            lv_obj_add_event_cb(obj, action_slider_brightness_released, LV_EVENT_RELEASED, (void *)0);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_screen_brightness_slider, LV_EVENT_ALL, 0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 36, 46);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Screen Brightness");
        }
        {
            // volume_slider
            lv_obj_t *obj = lv_slider_create(parent_obj);
            objects.volume_slider = obj;
            lv_obj_set_pos(obj, 36, 114);
            lv_obj_set_size(obj, 187, 10);
            lv_obj_add_event_cb(obj, action_slider_volume_released, LV_EVENT_RELEASED, (void *)0);
            lv_obj_add_event_cb(obj, event_handler_cb_settings_volume_slider, LV_EVENT_ALL, 0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x2196f3), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 3, LV_PART_KNOB | LV_STATE_DEFAULT);
        }
        {
            lv_obj_t *obj = lv_label_create(parent_obj);
            lv_obj_set_pos(obj, 36, 91);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Volume");
        }
        {
            // locker_lock
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.locker_lock = obj;
            lv_obj_set_pos(obj, 18, 266);
            lv_obj_set_size(obj, 92, 47);
            lv_obj_add_event_cb(obj, action_button_lock_pressed, LV_EVENT_PRESSED, (void *)0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0x2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Lock");
                }
            }
        }
        {
            // locker_unlock
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.locker_unlock = obj;
            lv_obj_set_pos(obj, 125, 267);
            lv_obj_set_size(obj, 92, 47);
            lv_obj_add_event_cb(obj, action_button_unlock_pressed, LV_EVENT_PRESSED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Unlock");
                }
            }
        }
        {
            // reset_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.reset_button = obj;
            lv_obj_set_pos(obj, 18, 206);
            lv_obj_set_size(obj, 92, 47);
            lv_obj_add_event_cb(obj, action_button_reset_device_pressed, LV_EVENT_PRESSED, (void *)0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(0xf32121), LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Reset");
                }
            }
        }
        {
            // wifi_status
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.wifi_status = obj;
            lv_obj_set_pos(obj, 40, 145);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "");
        }
        {
            // wifi_logo
            lv_obj_t *obj = lv_image_create(parent_obj);
            objects.wifi_logo = obj;
            lv_obj_set_pos(obj, 6, 140);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_image_set_src(obj, &img_wifi_bitmap);
        }
        {
            // volume_logo
            lv_obj_t *obj = lv_image_create(parent_obj);
            objects.volume_logo = obj;
            lv_obj_set_pos(obj, 6, 95);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_image_set_src(obj, &img_volume_bitmap);
        }
        {
            // brightness_logo
            lv_obj_t *obj = lv_image_create(parent_obj);
            objects.brightness_logo = obj;
            lv_obj_set_pos(obj, 6, 50);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_image_set_src(obj, &img_brightness_bitmap);
        }
        {
            // pair_button
            lv_obj_t *obj = lv_button_create(parent_obj);
            objects.pair_button = obj;
            lv_obj_set_pos(obj, 126, 206);
            lv_obj_set_size(obj, 92, 47);
            lv_obj_add_event_cb(obj, action_button_start_pairing_pressed, LV_EVENT_PRESSED, (void *)0);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Pair");
                }
            }
        }
    }
    
    tick_screen_settings();
}

void tick_screen_settings() {
    {
        int32_t new_val = get_var_screen_brightness_val();
        int32_t cur_val = lv_slider_get_value(objects.screen_brightness_slider);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.screen_brightness_slider;
            lv_slider_set_value(objects.screen_brightness_slider, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        int32_t new_val = get_var_volume();
        int32_t cur_val = lv_slider_get_value(objects.volume_slider);
        if (new_val != cur_val) {
            tick_value_change_obj = objects.volume_slider;
            lv_slider_set_value(objects.volume_slider, new_val, LV_ANIM_OFF);
            tick_value_change_obj = NULL;
        }
    }
    {
        const char *new_val = get_var_wifi_status_str();
        const char *cur_val = lv_label_get_text(objects.wifi_status);
        if (strcmp(new_val, cur_val) != 0) {
            tick_value_change_obj = objects.wifi_status;
            lv_label_set_text(objects.wifi_status, new_val);
            tick_value_change_obj = NULL;
        }
    }
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
    tick_screen_clock,
    tick_screen_alarm,
    tick_screen_settings,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 4) {
        tick_screen_funcs[screen_index]();
    }
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen(screenId - 1);
}

//
// Fonts
//

ext_font_desc_t fonts[] = {
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_display_t *dispp = lv_display_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_display_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_main();
    create_screen_clock();
    create_screen_alarm();
    create_screen_settings();
}