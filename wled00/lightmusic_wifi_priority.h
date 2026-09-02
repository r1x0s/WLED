#ifndef WLED_LIGHTMUSIC_WIFI_PRIORITY_H
#define WLED_LIGHTMUSIC_WIFI_PRIORITY_H

// WLED-LightMusic: priority-based selection among configured WiFi networks (spec FR-1).
// Header-only, C++11, no Arduino dependencies (also compiled by host tests in test/lightmusic).
//
// Selection rules:
//   1. Only networks visible in the current scan are candidates.
//   2. If a configured entry pins a BSSID, only scan rows with that exact BSSID count for it.
//   3. Highest numeric priority (0..255) wins.
//   4. Equal priority: better (higher) RSSI wins.
//   5. Everything equal: the entry listed first in the settings wins.
// The function is only consulted on boot / reconnect, never while connected (no active roaming).

#include <stdint.h>
#include <stddef.h>

struct LightmusicWifiCandidate {
  uint8_t configIndex;  // index into the configured network list
  uint8_t priority;     // configured priority, 0 = lowest
  int16_t rssi;         // scan RSSI in dBm (negative)
  bool    bssidPinned;  // configured entry has a non-zero BSSID
  bool    bssidMatch;   // scan row BSSID equals the configured BSSID
};

// Returns true if `a` should be preferred over `b`.
inline bool lightmusicWifiCandidateBetter(const LightmusicWifiCandidate& a, const LightmusicWifiCandidate& b) {
  if (a.priority != b.priority) return a.priority > b.priority;
  if (a.rssi != b.rssi) return a.rssi > b.rssi;
  return a.configIndex < b.configIndex;
}

// A pinned entry is only usable through a scan row carrying its own BSSID.
inline bool lightmusicWifiCandidateEligible(const LightmusicWifiCandidate& c) {
  return !(c.bssidPinned && !c.bssidMatch);
}

// Pick the best candidate index (config index) from `count` scan-derived candidates.
// `current` is returned unchanged when no candidate is eligible (keeps the caller's
// round-robin fallback for hidden SSIDs working).
inline int lightmusicSelectWifi(const LightmusicWifiCandidate* candidates, size_t count, int current) {
  int best = -1;
  for (size_t i = 0; i < count; i++) {
    const LightmusicWifiCandidate& c = candidates[i];
    if (!lightmusicWifiCandidateEligible(c)) continue;
    if (best < 0 || lightmusicWifiCandidateBetter(c, candidates[best])) best = (int)i;
  }
  return best < 0 ? current : candidates[best].configIndex;
}

// True if any byte of a 6-byte BSSID is non-zero (i.e. the entry pins an access point).
inline bool lightmusicBssidIsSet(const uint8_t* bssid) {
  for (size_t i = 0; i < 6; i++) if (bssid[i] != 0) return true;
  return false;
}

#endif // WLED_LIGHTMUSIC_WIFI_PRIORITY_H
