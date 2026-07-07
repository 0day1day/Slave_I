idf_component_register(SRCS "spectra5_offensive.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi)

# Let our ieee80211_raw_frame_sanity_check() override win over libnet80211's, so
# the C6 can transmit raw deauth/disassoc frames via esp_wifi_80211_tx.
idf_build_set_property(LINK_OPTIONS "-Wl,-zmuldefs" APPEND)
