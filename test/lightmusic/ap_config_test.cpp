#include "../../wled00/lightmusic_ap_config.h"
#include "test_common.h"

int main() {
  LM_CHECK_EQ(kLightmusicApDefaultMaxConnections, 8);

  // In-range values pass through unchanged.
  LM_CHECK_EQ(lightmusicClampApMaxConnections(8, kLightmusicApPlatformLimitEsp32), 8);
  LM_CHECK_EQ(lightmusicClampApMaxConnections(8, kLightmusicApPlatformLimitEsp8266), 8);
  LM_CHECK_EQ(lightmusicClampApMaxConnections(4, 10), 4);

  // Zero is not a valid station count.
  LM_CHECK_EQ(lightmusicClampApMaxConnections(0, 10), 1);

  // Values above the platform limit are clamped to it.
  LM_CHECK_EQ(lightmusicClampApMaxConnections(12, kLightmusicApPlatformLimitEsp32), 10);
  LM_CHECK_EQ(lightmusicClampApMaxConnections(9, kLightmusicApPlatformLimitEsp8266), 8);
  LM_CHECK_EQ(lightmusicClampApMaxConnections(255, 8), 8);

  // Usable at compile time (C++11 constexpr).
  static_assert(lightmusicClampApMaxConnections(8, 10) == 8, "constexpr clamp");
  static_assert(lightmusicClampApMaxConnections(0, 10) == 1, "constexpr clamp lower bound");

  return lm_test_summary("ap_config_test");
}
