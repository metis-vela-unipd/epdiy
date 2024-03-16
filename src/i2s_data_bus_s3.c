#include "i2s_data_bus_s3.h"

#include <driver/periph_ctrl.h>
#include "esp_lcd_panel_io.h"
#include "esp_err.h"
#include "esp_log.h"
#include "epd_internals.h" // EPD_WIDTH

static const char *TAG = "I80";
/**
 * @brief Note that in ESP32 version with I2C in LCD mode it was using 2 buffers
 *        This version is not doing that and I think that is the reason that is slower
 *        But I'm not sure how is possible to do that in new esp_lcd component, needs more research
 */

static esp_lcd_panel_io_handle_t io_handle = NULL;

static intr_handle_t gI2S_intr_handle = NULL;

/**
 * @brief Indicates the device has finished its transmission and is ready again.
 */
static volatile bool output_done = true;


static uint8_t buffer[(EPD_WIDTH + 32) / 4] = { 0 };

volatile uint8_t IRAM_ATTR *i2s_get_current_buffer()
{
    return buffer;
}

bool IRAM_ATTR i2s_is_busy()
{
    return !output_done;
}

void IRAM_ATTR i2s_switch_buffer() {}

void IRAM_ATTR i2s_start_line_output()
{
    output_done = false;
    esp_lcd_panel_io_tx_color(io_handle, 0, buffer, (EPD_WIDTH + 32) / 4);
}


static bool notify_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    output_done = true;
    return output_done;
}


void i2s_bus_init(i2s_bus_config *cfg)
{
    ESP_LOGI(TAG,
        "Initialize Intel 8080 bus\n6:%d 7:%d 4:%d 5:%d 2:%d 3:%d 0:%d 1:%d\n"
        "start_pulse:%d clock:%d\nepd_row_width:%" PRIu32,
        cfg->data_6,cfg->data_7,cfg->data_4,cfg->data_5,cfg->data_2,cfg->data_3,cfg->data_0,
        cfg->data_1, cfg->start_pulse, cfg->clock, cfg->epd_row_width);

    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        // Withouth clk_src parameter in V5 it just hangs on esp_lcd_new_i80_bus instantiation
        .clk_src = LCD_CLK_SRC_PLL160M,
        .dc_gpio_num = cfg->start_pulse,
        .wr_gpio_num = cfg->clock,
        .data_gpio_nums = {
            cfg->data_6,
            cfg->data_7,
            cfg->data_4,
            cfg->data_5,
            cfg->data_2,
            cfg->data_3,
            cfg->data_0,
            cfg->data_1,
        },
        .bus_width = 8,
        .max_transfer_bytes = (cfg->epd_row_width + 32)/4
    };
    
    esp_err_t err = esp_lcd_new_i80_bus(&bus_config, &i80_bus);
    if (err) {
        ESP_LOGI(TAG, "esp_lcd_new_i80_bus Error: %d", err);
    }
    
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = -1,
        .pclk_hz = 50 * 1000 * 1000,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 1,
            .dc_dummy_level = 0,
            .dc_data_level = 0,
        },
        .on_color_trans_done = notify_trans_done,
        .user_ctx = NULL,
        .lcd_cmd_bits = 10,
        .lcd_param_bits = 0,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
    ESP_LOGI(TAG, "8080 esp_lcd_new_panel_io_i80 done");
}

void i2s_deinit()
{
    esp_intr_free(gI2S_intr_handle);
    periph_module_disable(PERIPH_I2S1_MODULE);
}