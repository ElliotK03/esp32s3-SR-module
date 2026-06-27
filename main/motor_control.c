#include "motor_control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"

#include "pinout.h"

static const char *TAG_MOTOR = "motor";

#define TIMER_RESOLUTION 80000000 // 80Mhz which is half of the 160Mhz source used
#define COUNTER_PERIOD 8000 // 8000 ticks for 10kHz PWM

static mcpwm_cmpr_handle_t cmp_m_a_h, cmp_m_a_l, cmp_m_b_h, cmp_m_b_l;
static mcpwm_gen_handle_t gen_m_a_h, gen_m_a_l, gen_m_b_h, gen_m_b_l;
static TaskHandle_t motor_run_task_handle = NULL;

void motor_brake(void) {
    mcpwm_comparator_set_compare_value(cmp_m_a_h, 0);
    mcpwm_comparator_set_compare_value(cmp_m_b_h, 0);
    mcpwm_comparator_set_compare_value(cmp_m_b_l, COUNTER_PERIOD);
    mcpwm_comparator_set_compare_value(cmp_m_a_l, COUNTER_PERIOD);
}

void motor_turn_cw(uint32_t cmpr_value) {
    if (cmpr_value > COUNTER_PERIOD) {
        cmpr_value = COUNTER_PERIOD;
    }

    mcpwm_comparator_set_compare_value(cmp_m_a_h, cmpr_value);
    mcpwm_comparator_set_compare_value(cmp_m_b_h, 0);
    mcpwm_comparator_set_compare_value(cmp_m_b_l, cmpr_value);
    mcpwm_comparator_set_compare_value(cmp_m_a_l, 0);
}

void motor_turn_ccw(uint32_t cmpr_value) {
    if (cmpr_value > COUNTER_PERIOD) {
        cmpr_value = COUNTER_PERIOD;
    }

    mcpwm_comparator_set_compare_value(cmp_m_a_h, 0);
    mcpwm_comparator_set_compare_value(cmp_m_b_h, cmpr_value);
    mcpwm_comparator_set_compare_value(cmp_m_b_l, 0);
    mcpwm_comparator_set_compare_value(cmp_m_a_l, cmpr_value);
}

void motor_init(void) {
    mcpwm_timer_handle_t timer0 = NULL;
    mcpwm_timer_config_t timer0_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_PLL160M,
        .resolution_hz = TIMER_RESOLUTION,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = COUNTER_PERIOD
    };

    mcpwm_new_timer(&timer0_config, &timer0);

    mcpwm_oper_handle_t operator0 = NULL, operator1 = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    mcpwm_new_operator(&operator_config, &operator0);
    mcpwm_new_operator(&operator_config, &operator1);
    mcpwm_operator_connect_timer(operator0, timer0);
    mcpwm_operator_connect_timer(operator1, timer0);

    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tep = true
    };

    mcpwm_new_comparator(operator0, &comparator_config, &cmp_m_a_h);
    mcpwm_new_comparator(operator0, &comparator_config, &cmp_m_b_l);
    mcpwm_new_comparator(operator1, &comparator_config, &cmp_m_a_l);
    mcpwm_new_comparator(operator1, &comparator_config, &cmp_m_b_h);

    mcpwm_generator_config_t gen_m_a_h_config = {.gen_gpio_num = MOTOR_A_H};
    mcpwm_generator_config_t gen_m_a_l_config = {.gen_gpio_num = MOTOR_A_L};
    mcpwm_generator_config_t gen_m_b_h_config = {.gen_gpio_num = MOTOR_B_H};
    mcpwm_generator_config_t gen_m_b_l_config = {.gen_gpio_num = MOTOR_B_L};

    mcpwm_new_generator(operator0, &gen_m_a_h_config, &gen_m_a_h);
    mcpwm_new_generator(operator0, &gen_m_b_l_config, &gen_m_b_l);
    mcpwm_new_generator(operator1, &gen_m_a_l_config, &gen_m_a_l);
    mcpwm_new_generator(operator1, &gen_m_b_h_config, &gen_m_b_h);

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_m_a_h,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_m_a_h,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_m_a_h, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_m_a_l,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_m_a_l,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_m_a_l, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_m_b_h,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_m_b_h,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_m_b_h, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_m_b_l,
                    MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_m_b_l,
                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, cmp_m_b_l, MCPWM_GEN_ACTION_LOW)));

    mcpwm_dead_time_config_t dead_time_config = {
        .posedge_delay_ticks = 50,
        .negedge_delay_ticks = 0,
    };
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gen_m_b_l, gen_m_b_l, &dead_time_config));
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gen_m_a_l, gen_m_a_l, &dead_time_config));

    dead_time_config.posedge_delay_ticks = 0;
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gen_m_b_h, gen_m_b_h, &dead_time_config));
    ESP_ERROR_CHECK(mcpwm_generator_set_dead_time(gen_m_a_h, gen_m_a_h, &dead_time_config));

    mcpwm_comparator_set_compare_value(cmp_m_a_h, 0);
    mcpwm_comparator_set_compare_value(cmp_m_a_l, 0);
    mcpwm_comparator_set_compare_value(cmp_m_b_h, 0);
    mcpwm_comparator_set_compare_value(cmp_m_b_l, 0);

    mcpwm_timer_enable(timer0);
    mcpwm_timer_start_stop(timer0, MCPWM_TIMER_START_NO_STOP);
}

static void motor_run_timer_task(void *pvParameters) {
    int dir = (int)pvParameters; // 1 = CW (lock), 2 = CCW (unlock)
    if (dir == 1) {
        ESP_LOGI(TAG_MOTOR, "Turning motor CW (Lock) for 3 seconds...");
        motor_turn_cw(COUNTER_PERIOD);
    } else {
        ESP_LOGI(TAG_MOTOR, "Turning motor CCW (Unlock) for 3 seconds...");
        motor_turn_ccw(COUNTER_PERIOD);
    }

    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG_MOTOR, "Braking motor...");
    motor_brake();

    motor_run_task_handle = NULL;
    vTaskDelete(NULL);
}

static void trigger_motor_run(int dir) {
    if (motor_run_task_handle != NULL) {
        vTaskDelete(motor_run_task_handle);
        motor_brake();
        motor_run_task_handle = NULL;
    }
    xTaskCreate(motor_run_timer_task, "motor_run_timer_task", 2048, (void *)dir, 2, &motor_run_task_handle);
}

void motor_lock(void) {
    trigger_motor_run(1);
}

void motor_unlock(void) {
    ESP_LOGI(TAG_MOTOR, "test unlick");
    trigger_motor_run(2);
}
