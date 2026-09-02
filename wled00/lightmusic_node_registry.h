#ifndef WLED_LIGHTMUSIC_NODE_REGISTRY_H
#define WLED_LIGHTMUSIC_NODE_REGISTRY_H

// WLED-LightMusic: named registry of LED nodes kept by the master (spec FR-6).
// Header-only, C++11, no Arduino dependencies (also compiled by host tests in test/lightmusic).
//
// Core data model only. It is deliberately NOT wired to UDP or the web UI yet: the registry must
// never be mutated by arbitrary broadcasts, so network integration waits for the pairing flow (FR-7).

#include <stdint.h>
#include <stddef.h>
#include <string.h>

constexpr uint8_t kLightmusicMaxNodes     = 8;
constexpr size_t  kLightmusicNodeNameLen  = 32;   // characters, excluding terminator
constexpr size_t  kLightmusicMacLen       = 6;

struct LightmusicNodeRecord {
  uint8_t  mac[kLightmusicMacLen];
  char     name[kLightmusicNodeNameLen + 1];
  uint8_t  groupMask;        // sync groups the node listens to (bit 0 = group 1), never 0 for a used record
  uint8_t  capabilities;     // free-form capability bits (2D, audio receive, ...), 0 = unknown
  uint32_t lastSeenMs;
  uint16_t configRevision;   // revision of the config the node acknowledged, 0 = never
  bool     used;

  void clear() { memset(this, 0, sizeof(*this)); }
};

class LightmusicNodeRegistry {
 public:
  LightmusicNodeRegistry() { clear(); }

  void clear() {
    for (size_t i = 0; i < kLightmusicMaxNodes; i++) records_[i].clear();
  }

  size_t count() const {
    size_t n = 0;
    for (size_t i = 0; i < kLightmusicMaxNodes; i++) if (records_[i].used) n++;
    return n;
  }

  // Index of the record with this MAC, or -1.
  int find(const uint8_t* mac) const {
    for (size_t i = 0; i < kLightmusicMaxNodes; i++)
      if (records_[i].used && memcmp(records_[i].mac, mac, kLightmusicMacLen) == 0) return (int)i;
    return -1;
  }

  // Insert a node or refresh an existing one (same MAC -> same record, no duplicates).
  // New nodes get the fallback name "Node-XXYYZZ" (last three MAC bytes) and group 1.
  // Returns the record index, or -1 when the registry is full.
  int upsert(const uint8_t* mac, uint32_t nowMs) {
    int idx = find(mac);
    if (idx < 0) {
      idx = firstFree();
      if (idx < 0) return -1;
      LightmusicNodeRecord& r = records_[idx];
      r.clear();
      memcpy(r.mac, mac, kLightmusicMacLen);
      fallbackName(mac, r.name);
      r.groupMask = 0x01;
      r.used = true;
    }
    records_[idx].lastSeenMs = nowMs;
    return idx;
  }

  // Rename a node. Empty names are rejected; longer names are truncated to kLightmusicNodeNameLen.
  bool rename(int idx, const char* newName) {
    if (!valid(idx) || newName == nullptr || newName[0] == '\0') return false;
    strncpy(records_[idx].name, newName, kLightmusicNodeNameLen);
    records_[idx].name[kLightmusicNodeNameLen] = '\0';
    return true;
  }

  // Assign sync groups (bit mask). A node must belong to at least one group.
  bool setGroups(int idx, uint8_t mask) {
    if (!valid(idx) || mask == 0) return false;
    records_[idx].groupMask = mask;
    return true;
  }

  bool setCapabilities(int idx, uint8_t caps) {
    if (!valid(idx)) return false;
    records_[idx].capabilities = caps;
    return true;
  }

  bool setConfigRevision(int idx, uint16_t rev) {
    if (!valid(idx)) return false;
    records_[idx].configRevision = rev;
    return true;
  }

  bool remove(int idx) {
    if (!valid(idx)) return false;
    records_[idx].clear();
    return true;
  }

  const LightmusicNodeRecord* get(int idx) const { return valid(idx) ? &records_[idx] : nullptr; }

  // Writes "Node-XXYYZZ" (uppercase hex of the last 3 MAC bytes) into `out` (>= 12 chars).
  static void fallbackName(const uint8_t* mac, char* out) {
    static const char hex[] = "0123456789ABCDEF";
    const char prefix[] = "Node-";
    size_t p = 0;
    for (; prefix[p] != '\0'; p++) out[p] = prefix[p];
    for (size_t i = kLightmusicMacLen - 3; i < kLightmusicMacLen; i++) {
      out[p++] = hex[mac[i] >> 4];
      out[p++] = hex[mac[i] & 0x0F];
    }
    out[p] = '\0';
  }

 private:
  bool valid(int idx) const { return idx >= 0 && idx < (int)kLightmusicMaxNodes && records_[idx].used; }

  int firstFree() const {
    for (size_t i = 0; i < kLightmusicMaxNodes; i++) if (!records_[i].used) return (int)i;
    return -1;
  }

  LightmusicNodeRecord records_[kLightmusicMaxNodes];
};

#endif // WLED_LIGHTMUSIC_NODE_REGISTRY_H
