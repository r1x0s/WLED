#ifndef WLED_LIGHTMUSIC_SYNC_HEARTBEAT_H
#define WLED_LIGHTMUSIC_SYNC_HEARTBEAT_H

// WLED-LightMusic: periodic state-sync heartbeat scheduling (spec FR-4, mode A).
// Header-only, C++11, no Arduino dependencies (also compiled by host tests in test/lightmusic).
//
// The master re-broadcasts its full UDP state snapshot every `interval` ms so that nodes which
// (re)join the network converge to the current state within one heartbeat period.

#include <stdint.h>

// Shortest allowed non-zero heartbeat period. 0 disables the heartbeat.
constexpr uint16_t kLightmusicHeartbeatMinIntervalMs = 1000;

// Compile-time default; the master profile sets 1000, node profiles 0 (see platformio_lightmusic.ini).
#ifndef LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL
  #define LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL 0
#endif
static_assert(LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL == 0 || LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL >= 1000,
              "LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL must be 0 (off) or at least 1000 ms");
static_assert(LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL <= 65535,
              "LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL must fit into uint16_t");

// Normalise a user-supplied interval: 0 stays off, anything below the minimum is raised to it.
constexpr uint16_t lightmusicNormalizeHeartbeatInterval(uint32_t requested) {
  return requested == 0 ? 0
       : (requested < kLightmusicHeartbeatMinIntervalMs ? kLightmusicHeartbeatMinIntervalMs
       : (requested > 65535u ? (uint16_t)65535u : (uint16_t)requested));
}

// True when a heartbeat should be sent now. `lastSentMs` is the timestamp of the most recent
// packet that already carried the full state (heartbeat or regular notification), so a long
// loop stall never produces a burst: the caller resets `lastSentMs = nowMs` after sending.
inline bool lightmusicHeartbeatDue(uint32_t nowMs, uint32_t lastSentMs, uint16_t intervalMs) {
  if (intervalMs == 0) return false;
  return (uint32_t)(nowMs - lastSentMs) >= intervalMs;
}

#endif // WLED_LIGHTMUSIC_SYNC_HEARTBEAT_H
