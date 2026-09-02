#ifndef WLED_LIGHTMUSIC_AP_CONFIG_H
#define WLED_LIGHTMUSIC_AP_CONFIG_H

// WLED-LightMusic: SoftAP station-limit helpers.
// Header-only, C++11, no Arduino dependencies (also compiled by host tests in test/lightmusic).

#include <stdint.h>

// Default number of simultaneous SoftAP stations the master accepts (spec FR-2).
// Both ESP cores default to 4; ESP-IDF 4.4 DHCP server hands out at most 8 leases
// (CONFIG_LWIP_DHCPS_MAX_STATION_NUM=8), so 8 is the practical ceiling without rebuilding IDF.
constexpr uint8_t kLightmusicApDefaultMaxConnections = 8;

// Hard limits of the WiFi drivers: ESP32 family (ESP_WIFI_MAX_CONN_NUM) and ESP8266 SDK.
constexpr uint8_t kLightmusicApPlatformLimitEsp32   = 10;
constexpr uint8_t kLightmusicApPlatformLimitEsp8266 = 8;

// Clamp a requested station count into the valid range [1, platformLimit].
// Single-return expression so it stays a valid C++11 constexpr.
constexpr uint8_t lightmusicClampApMaxConnections(uint8_t requested, uint8_t platformLimit) {
  return requested == 0 ? 1 : (requested > platformLimit ? platformLimit : requested);
}

#endif // WLED_LIGHTMUSIC_AP_CONFIG_H
