# WLED-LightMusic host tests

Plain C++ tests for the header-only fork modules in `wled00/lightmusic_*.h`.
They need only a host `g++` (no Arduino / PlatformIO) and are compiled with both
`-std=c++11` (the ESP32 firmware standard) and `-std=c++17`, with `-Werror`.

```bash
bash test/lightmusic/run.sh
```

| Test | Module | Covers |
|---|---|---|
| `ap_config_test` | `lightmusic_ap_config.h` | SoftAP station-count clamping |
| `wifi_priority_test` | `lightmusic_wifi_priority.h` | priority → RSSI → order ranking, BSSID pinning |
| `net_utils_test` | `lightmusic_net_utils.h` | AP-aware UDP broadcast address |
| `sync_heartbeat_test` | `lightmusic_sync_heartbeat.h` | interval normalisation, no-burst scheduling, rollover |
| `node_registry_test` | `lightmusic_node_registry.h` | upsert/dedupe, validation, capacity |

Binaries go to `$LIGHTMUSIC_TEST_OUT` (default `$TMPDIR/lightmusic-host-tests`).
PlatformIO's `pio test` ignores this directory (`test_ignore = lightmusic` in the lightmusic envs).
