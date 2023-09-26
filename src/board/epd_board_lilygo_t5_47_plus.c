#include "epd_board.h"

#include <sdkconfig.h>
#include "../output_i2s/i2s_data_bus.h"
#include "../output_i2s/rmt_pulse.h"
#include "../output_i2s/render_i2s.h"

#include <string.h>

// Make this compile von the ESP32 without ifdefing the whole file
#ifndef CONFIG_IDF_TARGET_ESP32S3
#define GPIO_NUM_40 -1
#define GPIO_NUM_41 -1
#define GPIO_NUM_42 -1
#define GPIO_NUM_43 -1
#define GPIO_NUM_44 -1
#define GPIO_NUM_45 -1
#define GPIO_NUM_46 -1
#define GPIO_NUM_47 -1
#define GPIO_NUM_48 -1
#endif

#define CFG_DATA GPIO_NUM_13
#define CFG_CLK GPIO_NUM_12
#define CFG_STR GPIO_NUM_0
#define D7 GPIO_NUM_7
#define D6 GPIO_NUM_6
#define D5 GPIO_NUM_5
#define D4 GPIO_NUM_4
#define D3 GPIO_NUM_3
#define D2 GPIO_NUM_2
#define D1 GPIO_NUM_1
#define D0 GPIO_NUM_8

/* Control Lines */
#define CKV GPIO_NUM_38
#define STH GPIO_NUM_40

/* Edges */
#define CKH GPIO_NUM_41

typedef struct
{
    bool ep_latch_enable : 1;
    bool power_disable : 1;
    bool pos_power_enable : 1;
    bool neg_power_enable : 1;
    bool ep_stv : 1;
    bool ep_scan_direction : 1;
    bool ep_mode : 1;
    bool ep_output_enable : 1;
} epd_config_register_t;

static epd_config_register_t config_reg;

inline static void IRAM_ATTR push_cfg_bit(bool bit)
{
    fast_gpio_set_lo(CFG_CLK);
    if (bit)
    {
        fast_gpio_set_hi(CFG_DATA);
    }
    else
    {
        fast_gpio_set_lo(CFG_DATA);
    }
    fast_gpio_set_hi(CFG_CLK);
}

static void IRAM_ATTR push_cfg(epd_config_register_t *cfg)
{
    fast_gpio_set_lo(CFG_STR);

    // push config bits in reverse order
    push_cfg_bit(cfg->ep_output_enable);
    push_cfg_bit(cfg->ep_mode);
    push_cfg_bit(cfg->ep_scan_direction);
    push_cfg_bit(cfg->ep_stv);

    push_cfg_bit(cfg->neg_power_enable);
    push_cfg_bit(cfg->pos_power_enable);
    push_cfg_bit(cfg->power_disable);
    push_cfg_bit(cfg->ep_latch_enable);

    fast_gpio_set_hi(CFG_STR);
}


void IRAM_ATTR busy_delay(uint32_t cycles)
{
    volatile uint64_t counts = XTHAL_GET_CCOUNT() + cycles;
    while (XTHAL_GET_CCOUNT() < counts) ;
}


void epd_board_init(uint32_t epd_row_width)
{
    config_reg.ep_latch_enable = false;
    config_reg.power_disable = true;
    config_reg.pos_power_enable = false;
    config_reg.neg_power_enable = false;
    config_reg.ep_stv = true;
    config_reg.ep_scan_direction = true;
    config_reg.ep_mode = false;
    config_reg.ep_output_enable = false;

    /* Power Control Output/Off */
    gpio_set_direction(CFG_DATA, GPIO_MODE_OUTPUT);
    gpio_set_direction(CFG_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(CFG_STR, GPIO_MODE_OUTPUT);
    fast_gpio_set_lo(CFG_STR);

    push_cfg(&config_reg);

    // Setup I2S
    i2s_bus_config i2s_config;
    i2s_config.clock = CKH;
    i2s_config.start_pulse = STH;
    i2s_config.data_0 = D0;
    i2s_config.data_1 = D1;
    i2s_config.data_2 = D2;
    i2s_config.data_3 = D3;
    i2s_config.data_4 = D4;
    i2s_config.data_5 = D5;
    i2s_config.data_6 = D6;
    i2s_config.data_7 = D7;

    i2s_bus_init(&i2s_config, epd_row_width + 32);

    rmt_pulse_init(CKV);
}

static void epd_board_set_ctrl(epd_ctrl_state_t *state, const epd_ctrl_state_t * const mask) {
    if (state->ep_sth) {
        fast_gpio_set_hi(STH);
    } else {
        fast_gpio_set_lo(STH);
    }

    if (mask->ep_output_enable || mask->ep_mode || mask->ep_stv || mask->ep_latch_enable) {
        fast_gpio_set_lo(CFG_STR);

        // push config bits in reverse order
        push_cfg_bit(state->ep_output_enable);
        push_cfg_bit(state->ep_mode);
        push_cfg_bit(config_reg.ep_scan_direction);
        push_cfg_bit(state->ep_stv);

        push_cfg_bit(config_reg.neg_power_enable);
        push_cfg_bit(config_reg.pos_power_enable);
        push_cfg_bit(config_reg.power_disable);
        push_cfg_bit(state->ep_latch_enable);

        fast_gpio_set_hi(CFG_STR);
    }
}

void epd_board_poweron()
{
    config_reg.ep_scan_direction = true;
    config_reg.power_disable = false;
    push_cfg(&config_reg);
    busy_delay(100 * 240);
    config_reg.neg_power_enable = true;
    push_cfg(&config_reg);
    busy_delay(500 * 240);
    config_reg.pos_power_enable = true;
    push_cfg(&config_reg);
    busy_delay(100 * 240);
    config_reg.ep_stv = true;
    push_cfg(&config_reg);
    fast_gpio_set_hi(STH);
}

void epd_board_poweroff()
{
    config_reg.pos_power_enable = false;
    push_cfg(&config_reg);
    busy_delay(10 * 240);
    config_reg.neg_power_enable = false;
    push_cfg(&config_reg);
    busy_delay(100 * 240);
    config_reg.power_disable = true;
    push_cfg(&config_reg);

    config_reg.ep_stv = false;
    push_cfg(&config_reg);
}

void epd_poweroff_all()
{
    memset(&config_reg, 0, sizeof(config_reg));
    push_cfg(&config_reg);
}

void epd_start_frame()
{
    while (i2s_is_busy()) ;

    config_reg.ep_mode = true;
    push_cfg(&config_reg);

    pulse_ckv_us(1, 1, true);

    // This is very timing-sensitive!
    config_reg.ep_stv = false;
    push_cfg(&config_reg);
    busy_delay(240);
    pulse_ckv_us(10, 10, false);
    config_reg.ep_stv = true;
    push_cfg(&config_reg);
    pulse_ckv_us(0, 10, true);

    config_reg.ep_output_enable = true;
    push_cfg(&config_reg);

    pulse_ckv_us(1, 1, true);
}

static inline void latch_row()
{
    config_reg.ep_latch_enable = true;
    push_cfg(&config_reg);

    config_reg.ep_latch_enable = false;
    push_cfg(&config_reg);
}

void IRAM_ATTR epd_skip()
{
#if defined(CONFIG_EPD_DISPLAY_TYPE_ED097TC2)
    pulse_ckv_ticks(2, 2, false);
#else
    // According to the spec, the OC4 maximum CKV frequency is 200kHz.
    pulse_ckv_ticks(45, 5, false);
#endif
}

void IRAM_ATTR epd_output_row(uint32_t output_time_dus)
{
    while (i2s_is_busy());

    latch_row();

    pulse_ckv_ticks(output_time_dus, 50, false);

    i2s_start_line_output();
    i2s_switch_buffer();
}

void epd_end_frame()
{
    config_reg.ep_output_enable = false;
    push_cfg(&config_reg);
    config_reg.ep_mode = false;
    push_cfg(&config_reg);
    pulse_ckv_us(1, 1, true);
    pulse_ckv_us(1, 1, true);
}

void IRAM_ATTR epd_switch_buffer()
{
    i2s_switch_buffer();
}

uint8_t * IRAM_ATTR epd_get_current_buffer()
{
    return (uint8_t *)i2s_get_current_buffer();
}

const EpdBoardDefinition epd_board_lilygo_t5_47_plus = {
    .init = epd_board_init,
    .deinit = NULL,
    .set_ctrl = epd_board_set_ctrl,
    .poweron = epd_board_poweron,
    .poweroff = epd_board_poweroff,
    .get_temperature = NULL,
};

const EpdBoardDefinition epd_board_lilygo_t5_47_plus_touch = {
    .init = epd_board_init,
    .deinit = NULL,
    .set_ctrl = epd_board_set_ctrl,
    .poweron = epd_board_poweron,
    .poweroff = epd_board_poweroff,
    .get_temperature = NULL,
    .set_vcom = NULL,
};
