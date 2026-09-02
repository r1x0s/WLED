#ifndef WLED_LIGHTMUSIC_NET_UTILS_H
#define WLED_LIGHTMUSIC_NET_UTILS_H

// WLED-LightMusic: network address helpers.
// Header-only, C++11, no Arduino dependencies (also compiled by host tests in test/lightmusic).
// IPv4 addresses are passed as 32-bit values in the same byte order Arduino's IPAddress uses
// for `uint32_t` conversion (first octet in the least significant byte).

#include <stdint.h>

constexpr uint32_t kLightmusicLimitedBroadcast = 0xFFFFFFFFu;

// Directed broadcast for a given interface (ip | ~mask). Byte order does not matter for this math.
constexpr uint32_t lightmusicDirectedBroadcast(uint32_t ip, uint32_t mask) {
  return ip | ~mask;
}

// Broadcast address for the UDP notifier / heartbeat.
//   - station connected  -> upstream behaviour: ~mask | gateway of the STA interface
//   - SoftAP only        -> directed broadcast of the AP subnet (e.g. 4.3.2.255)
//   - neither            -> limited broadcast 255.255.255.255
// Upstream computes only the first form; with no STA link the gateway is 0.0.0.0 on ESP32,
// which yields 0.0.0.255 and silently loses every packet sent while the master runs as an AP.
inline uint32_t lightmusicBroadcastAddress(uint32_t staIp, uint32_t staMask, uint32_t staGw,
                                           bool apActive, uint32_t apIp, uint32_t apMask) {
  if (staIp != 0) return (~staMask) | staGw;
  if (apActive && apIp != 0) return lightmusicDirectedBroadcast(apIp, apMask);
  return kLightmusicLimitedBroadcast;
}

#endif // WLED_LIGHTMUSIC_NET_UTILS_H
