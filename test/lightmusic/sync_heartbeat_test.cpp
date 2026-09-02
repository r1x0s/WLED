#include "../../wled00/lightmusic_sync_heartbeat.h"
#include "test_common.h"

int main() {
  // Normalisation: 0 = off, below minimum raised to minimum, otherwise unchanged (capped to uint16).
  LM_CHECK_EQ(lightmusicNormalizeHeartbeatInterval(0), 0);
  LM_CHECK_EQ(lightmusicNormalizeHeartbeatInterval(1), 1000);
  LM_CHECK_EQ(lightmusicNormalizeHeartbeatInterval(999), 1000);
  LM_CHECK_EQ(lightmusicNormalizeHeartbeatInterval(1000), 1000);
  LM_CHECK_EQ(lightmusicNormalizeHeartbeatInterval(2500), 2500);
  LM_CHECK_EQ(lightmusicNormalizeHeartbeatInterval(70000), 65535);
  static_assert(lightmusicNormalizeHeartbeatInterval(500) == 1000, "constexpr normalise");

  // Disabled interval never fires.
  LM_CHECK(!lightmusicHeartbeatDue(5000, 0, 0));

  // Fires exactly at the interval boundary, not before.
  LM_CHECK(!lightmusicHeartbeatDue(1999, 1000, 1000));
  LM_CHECK(lightmusicHeartbeatDue(2000, 1000, 1000));

  // No burst after a long loop stall: one send, then the next only after a full interval.
  {
    uint32_t last = 0;
    uint32_t now = 10000; // loop was blocked for 10 s
    int sent = 0;
    for (int i = 0; i < 5; i++) {   // several consecutive loop iterations at (almost) the same time
      if (lightmusicHeartbeatDue(now + i, last, 1000)) { sent++; last = now + i; }
    }
    LM_CHECK_EQ(sent, 1);
    LM_CHECK(!lightmusicHeartbeatDue(last + 999, last, 1000));
    LM_CHECK(lightmusicHeartbeatDue(last + 1000, last, 1000));
  }

  // millis() rollover is handled by unsigned arithmetic.
  LM_CHECK(lightmusicHeartbeatDue(800, 0xFFFFFF00u, 1000));   // elapsed = 256 + 800 = 1056
  LM_CHECK(!lightmusicHeartbeatDue(500, 0xFFFFFF00u, 1000));  // elapsed = 256 + 500 = 756

  // A regular notification counts as "sent": passing max(lastHeartbeat, notificationSentTime) delays the beat.
  LM_CHECK(!lightmusicHeartbeatDue(1500, 1200 /* recent notify() */, 1000));

  return lm_test_summary("sync_heartbeat_test");
}
