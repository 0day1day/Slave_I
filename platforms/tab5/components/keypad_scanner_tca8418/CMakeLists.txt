idf_component_register(
    SRCS 
        "src/keypad_scanner_tca8418.c"
    INCLUDE_DIRS 
        "include"   
    REQUIRES
        esp_driver_i2c
	esp_driver_gpio
	m5stack_tab5
        led_strip
)

