#include "../../wled00/lightmusic_net_utils.h"
#include "test_common.h"

// Arduino IPAddress -> uint32_t byte order: first octet in the least significant byte.
static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return (uint32_t)a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

int main() {
  const uint32_t apIp   = ip4(4, 3, 2, 1);
  const uint32_t apMask = ip4(255, 255, 255, 0);

  // Station connected: upstream formula (~mask | gateway).
  LM_CHECK_EQ(lightmusicBroadcastAddress(ip4(192,168,1,50), ip4(255,255,255,0), ip4(192,168,1,1), false, 0, 0),
              ip4(192,168,1,255));
  // Station connected and AP also active: STA subnet still wins (upstream behaviour preserved).
  LM_CHECK_EQ(lightmusicBroadcastAddress(ip4(10,0,0,7), ip4(255,255,0,0), ip4(10,0,0,1), true, apIp, apMask),
              ip4(10,0,255,255));
  // SoftAP only (the bug case upstream: gateway 0.0.0.0 -> 0.0.0.255): use the AP subnet broadcast.
  LM_CHECK_EQ(lightmusicBroadcastAddress(0, ip4(255,255,255,0), 0, true, apIp, apMask), ip4(4,3,2,255));
  // Nothing up at all: limited broadcast.
  LM_CHECK_EQ(lightmusicBroadcastAddress(0, ip4(255,255,255,0), 0, false, apIp, apMask), kLightmusicLimitedBroadcast);
  // AP flagged active but without an IP yet: limited broadcast rather than garbage.
  LM_CHECK_EQ(lightmusicBroadcastAddress(0, 0, 0, true, 0, apMask), kLightmusicLimitedBroadcast);

  static_assert(lightmusicDirectedBroadcast(0x01020304u, 0x00FFFFFFu) == 0xFF020304u, "directed broadcast");
  return lm_test_summary("net_utils_test");
}
