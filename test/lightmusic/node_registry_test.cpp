#include "../../wled00/lightmusic_node_registry.h"
#include "test_common.h"

static const uint8_t kMacA[6] = {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF};
static const uint8_t kMacB[6] = {0x24, 0x6F, 0x28, 0x01, 0x02, 0x03};

int main() {
  LightmusicNodeRegistry reg;
  LM_CHECK_EQ(reg.count(), 0u);
  LM_CHECK_EQ(reg.find(kMacA), -1);

  // Upsert creates a record with fallback name, group 1 and lastSeen.
  int a = reg.upsert(kMacA, 1000);
  LM_CHECK(a >= 0);
  LM_CHECK_EQ(reg.count(), 1u);
  LM_CHECK(strcmp(reg.get(a)->name, "Node-ABCDEF") == 0);
  LM_CHECK_EQ(reg.get(a)->groupMask, 0x01);
  LM_CHECK_EQ(reg.get(a)->lastSeenMs, 1000u);
  LM_CHECK_EQ(reg.get(a)->configRevision, 0);

  // Same MAC again -> same record, no duplicate, lastSeen refreshed, name kept.
  LM_CHECK(reg.rename(a, "Lamp1"));
  int a2 = reg.upsert(kMacA, 2000);
  LM_CHECK_EQ(a2, a);
  LM_CHECK_EQ(reg.count(), 1u);
  LM_CHECK_EQ(reg.get(a)->lastSeenMs, 2000u);
  LM_CHECK(strcmp(reg.get(a)->name, "Lamp1") == 0);

  // Validation: empty name and zero group mask are rejected and leave the record untouched.
  LM_CHECK(!reg.rename(a, ""));
  LM_CHECK(!reg.rename(a, nullptr));
  LM_CHECK(strcmp(reg.get(a)->name, "Lamp1") == 0);
  LM_CHECK(!reg.setGroups(a, 0));
  LM_CHECK_EQ(reg.get(a)->groupMask, 0x01);

  // Multiple groups and other fields.
  LM_CHECK(reg.setGroups(a, 0x05));
  LM_CHECK_EQ(reg.get(a)->groupMask, 0x05);
  LM_CHECK(reg.setCapabilities(a, 0x02));
  LM_CHECK(reg.setConfigRevision(a, 7));
  LM_CHECK_EQ(reg.get(a)->capabilities, 0x02);
  LM_CHECK_EQ(reg.get(a)->configRevision, 7);

  // Long names are truncated to 32 characters.
  LM_CHECK(reg.rename(a, "0123456789012345678901234567890123456789"));
  LM_CHECK_EQ(strlen(reg.get(a)->name), kLightmusicNodeNameLen);

  // Invalid indices.
  LM_CHECK(!reg.rename(-1, "x"));
  LM_CHECK(!reg.rename(kLightmusicMaxNodes, "x"));
  LM_CHECK(!reg.setGroups(3, 1));           // unused slot
  LM_CHECK(reg.get(3) == nullptr);

  // Capacity: 8 nodes total, the 9th is refused, existing ones still update.
  int b = reg.upsert(kMacB, 3000);
  LM_CHECK(b >= 0 && b != a);
  for (uint8_t i = 0; i < 6; i++) {
    uint8_t mac[6] = {0x10, 0x20, 0x30, 0x40, 0x50, i};
    LM_CHECK(reg.upsert(mac, 4000 + i) >= 0);
  }
  LM_CHECK_EQ(reg.count(), (size_t)kLightmusicMaxNodes);
  {
    uint8_t extra[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    LM_CHECK_EQ(reg.upsert(extra, 9000), -1);
    LM_CHECK_EQ(reg.count(), (size_t)kLightmusicMaxNodes);
  }
  LM_CHECK_EQ(reg.upsert(kMacB, 9500), b);

  // Removal frees the slot for a new node.
  LM_CHECK(reg.remove(b));
  LM_CHECK_EQ(reg.find(kMacB), -1);
  LM_CHECK_EQ(reg.count(), (size_t)(kLightmusicMaxNodes - 1));
  {
    uint8_t extra[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    int e = reg.upsert(extra, 9600);
    LM_CHECK(e >= 0);
    LM_CHECK(strcmp(reg.get(e)->name, "Node-EF0001") == 0);
  }

  // Fallback name helper directly.
  char buf[16];
  LightmusicNodeRegistry::fallbackName(kMacB, buf);
  LM_CHECK(strcmp(buf, "Node-010203") == 0);

  return lm_test_summary("node_registry_test");
}
