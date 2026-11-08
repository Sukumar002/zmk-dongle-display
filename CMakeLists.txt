zephyr_include_directories(include)

if(CONFIG_HANA_CHARGE_STATUS)
    zephyr_library()

    zephyr_library_include_directories(
        include
        ${APPLICATION_SOURCE_DIR}/include
    )

    zephyr_library_sources(
        src/hana_charge_status.c
        src/hana_charge_state_changed.c
    )
endif()

if (CONFIG_ZMK_HID_INDICATORS)
    zephyr_library_sources(
        boards/shields/dongle_display/widgets/hid_indicators.c
    )
endif()
