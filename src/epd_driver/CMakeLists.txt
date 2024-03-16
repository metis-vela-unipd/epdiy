set(app_sources "epd_driver.c"
                "epd_board.c"
                "render.c"
                "display_ops.c"
                "tps65185.c"
                "pca9555.c"
                "font.c"
                "lut.c"
                "board_specific.c"
				"builtin_waveforms.c"
                "rmt_pulse.c"
				"highlevel.c"
                "epd_temperature.c"
                "board/epd_board_common.c"
)
# Add this sources only when MCU is ESP32
if (CONFIG_IDF_TARGET_ESP32)
list(APPEND app_sources
                "i2s_data_bus.c"
                "board/epd_board_lilygo_t5_47.c"
                "board/epd_board_v2_v3.c"
                "board/epd_board_v4.c"
                "board/epd_board_v5.c"
                "board/epd_board_v6.c"
)
endif()
# Add this sources only when MCU is ESP32S3
if(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND app_sources "i2s_data_bus_s3.c"
                "board/epd_board_lilygo_s3_47.c")
endif()

if (IDF_VERSION_MAJOR GREATER_EQUAL 5)
  set(COMPONENT_REQUIRES esp_adc esp_timer driver esp_lcd)
  elseif (IDF_VERSION_MAJOR EQUAL 4)
  set(COMPONENT_REQUIRES esp_adc esp_timer driver)
endif()


idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "include" 
        REQUIRES ${COMPONENT_REQUIRES}
        )

set_source_files_properties("lut.c" PROPERTIES COMPILE_OPTIONS -mno-fix-esp32-psram-cache-issue)