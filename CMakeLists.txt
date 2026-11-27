if(CONFIG_SHIELD_ZEN_LEFT OR CONFIG_SHIELD_ZEN_RIGHT)
  zephyr_library()
  zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
  zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/drivers/iqs7211e/include)
  # zmk/app's own include dir (drivers/input_processor.h, zmk/*.h, ...) isn't
  # inherited automatically by out-of-tree modules.
  zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
  zephyr_library_sources(src/board.c src/mini_trackpad_init_reg.c src/zen_pointer_settings.c src/zen_config_codec.c
                         src/zen_pointer_rotate.c
                         drivers/zen_input_processor/src/zen_input_processor_live_scale.c
                         drivers/zen_input_processor/src/zen_input_processor_auto_mouse.c)
  # Both halves: Zephyr's default fatal handler spins forever with interrupts
  # locked and there's no watchdog, so a faulting half stays dead until someone
  # cycles its power switch. See the comment at the top of the file.
  zephyr_library_sources(src/zen_fault_recovery.c)
  # See Kconfig: central-or-standalone only, and never alongside ZMK Studio,
  # which listens on the same UART.
  zephyr_library_sources_ifdef(CONFIG_ZEN_CONFIG_BRIDGE src/zen_config_serial_bridge.c)
  # Prototype, off in shipped firmware. See docs/runtime-combo-prototype.md.
  zephyr_library_sources_ifdef(CONFIG_ZEN_RUNTIME_COMBO src/zen_combo_settings.c)
  zephyr_library_sources_ifdef(CONFIG_ZEN_RUNTIME_MACRO src/zen_macro.c)
  zephyr_library_sources_ifdef(CONFIG_PMW3610_ALT drivers/pmw3610_alt/src/pmw3610.c)
  zephyr_library_sources_ifdef(CONFIG_IQS7211E drivers/iqs7211e/src/iqs7211e.c drivers/iqs7211e/src/iqs7211e_reg.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_DELAY_TEMP_LAYER src/input_processor_delay_temp_layer.c)
  zephyr_library_sources_ifdef(CONFIG_ZEN_TELEMETRY src/zen_telemetry.c)
  zephyr_library_sources_ifdef(CONFIG_ZEN_TELEMETRY_BLE src/zen_telemetry_ble.c src/zen_telemetry_hosts.c)
  # Only built by a target that selects the zen-lcd-status snippet. The LCD is
  # an optional build, not part of what ships -- see docs/lcd-build.md.
  zephyr_library_sources_ifdef(CONFIG_ZEN_LCD_STATUS_SCREEN
    src/zen_lcd_status_screen.c
    src/zen_dot_fonts.c
    src/zen_font_unscii_8x16.c
  )
endif()
