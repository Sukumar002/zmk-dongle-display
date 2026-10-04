if(CONFIG_HANA_CHARGE_STATUS)
    zephyr_library()
    zephyr_library_sources(src/hana_charge_status.c)
endif()
