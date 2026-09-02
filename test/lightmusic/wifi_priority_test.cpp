#include "../../wled00/lightmusic_wifi_priority.h"
#include "test_common.h"

static LightmusicWifiCandidate cand(uint8_t idx, uint8_t prio, int16_t rssi, bool pinned = false, bool match = false) {
  LightmusicWifiCandidate c;
  c.configIndex = idx; c.priority = prio; c.rssi = rssi; c.bssidPinned = pinned; c.bssidMatch = match;
  return c;
}

int main() {
  // Priority beats RSSI.
  {
    LightmusicWifiCandidate c[] = { cand(0, 0, -40), cand(1, 10, -80) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 2, 0), 1);
  }
  // Equal priority: better RSSI wins regardless of config order.
  {
    LightmusicWifiCandidate c[] = { cand(0, 5, -70), cand(1, 5, -50) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 2, 0), 1);
    LightmusicWifiCandidate d[] = { cand(0, 5, -50), cand(1, 5, -70) };
    LM_CHECK_EQ(lightmusicSelectWifi(d, 2, 1), 0);
  }
  // Full tie: earlier config entry wins, independent of scan order.
  {
    LightmusicWifiCandidate c[] = { cand(2, 5, -60), cand(0, 5, -60), cand(1, 5, -60) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 3, 2), 0);
  }
  // Scan may contain the same SSID twice (two APs): pick the stronger row.
  {
    LightmusicWifiCandidate c[] = { cand(0, 0, -75), cand(0, 0, -55), cand(1, 0, -60) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 3, 1), 0);
  }
  // BSSID pinning: pinned entry only eligible through a matching row.
  {
    // Pinned entry 0 has a top priority but its BSSID is not in the air -> entry 1 wins.
    LightmusicWifiCandidate c[] = { cand(0, 200, -40, true, false), cand(1, 1, -70) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 2, 1), 1);
    // Same SSID seen from two APs, only one is the pinned BSSID (and weaker) -> still that one.
    LightmusicWifiCandidate d[] = { cand(0, 200, -40, true, false), cand(0, 200, -80, true, true), cand(1, 1, -50) };
    LM_CHECK_EQ(lightmusicSelectWifi(d, 3, 1), 0);
  }
  // No eligible candidate: keep the caller's current selection (round-robin fallback for hidden SSIDs).
  {
    LM_CHECK_EQ(lightmusicSelectWifi(nullptr, 0, 2), 2);
    LightmusicWifiCandidate c[] = { cand(0, 9, -40, true, false) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 1, 1), 1);
  }
  // Legacy config without priority == all zero -> pure RSSI ranking (old behaviour minus hysteresis).
  {
    LightmusicWifiCandidate c[] = { cand(0, 0, -65), cand(1, 0, -62), cand(2, 0, -90) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 3, 0), 1);
  }
  // Priority extremes.
  {
    LightmusicWifiCandidate c[] = { cand(0, 255, -90), cand(1, 254, -30) };
    LM_CHECK_EQ(lightmusicSelectWifi(c, 2, 1), 0);
  }
  // BSSID set detection.
  {
    const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};
    const uint8_t set[6]  = {0, 0, 0, 0, 0, 1};
    LM_CHECK(!lightmusicBssidIsSet(zero));
    LM_CHECK(lightmusicBssidIsSet(set));
  }
  return lm_test_summary("wifi_priority_test");
}
