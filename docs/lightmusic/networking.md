# Сеть: приоритеты Wi‑Fi, SoftAP, профили сборки

Документ описывает сетевую часть форка WLED-LightMusic (база upstream `v16.0.1`). Требования — в `spec.md` (FR-1, FR-2, FR-3).

## Приоритет Wi‑Fi сетей (FR-1)

Каждая запись в списке сетей (Settings → WiFi) получила поле **Priority (0-255, higher connects first)**.

Алгоритм выбора (`findWiFi()` в `wled00/network.cpp`, чистая логика в `wled00/lightmusic_wifi_priority.h`):

1. После асинхронного скана рассматриваются только видимые в эфире SSID из списка.
2. Если у записи задан BSSID, она считается видимой только когда в скане есть точка именно с этим BSSID.
3. Побеждает наибольший `priority`; при равенстве — лучший RSSI; при полном равенстве — запись, стоящая выше в настройках.
4. Если ни одна из сетей не видна, сохраняется текущий выбор, и работает штатный round-robin WLED по списку (нужен для скрытых SSID).
5. Функция вызывается только при старте и переподключении: **активного роуминга нет**, появление сети с бóльшим приоритетом не рвёт стабильное соединение.

Хранение: `cfg.json` → `nw.ins[].prio` (0…255). Старый `cfg.json` без ключа загружается, приоритет считается `0`. Изменение приоритета в форме вызывает переподключение (`forceReconnect`).

Отличие от upstream: там выбирался сильнейший RSSI с гистерезисом 10 dB в пользу более ранних записей, а совпадение BSSID побеждало безусловно.

## SoftAP на 8 станций (FR-2)

`WiFi.softAP()` теперь получает пятый аргумент `max_connection` = `LIGHTMUSIC_AP_MAX_CONNECTIONS` (по умолчанию **8**, см. `wled00/const.h`), ограниченный лимитом платформы (`wled00/lightmusic_ap_config.h`: ESP32 — 10, ESP8266 — 8).

Почему 8: DHCP-сервер ESP-IDF 4.4 выдаёт максимум 8 адресов (`CONFIG_LWIP_DHCPS_MAX_STATION_NUM=8`), поэтому большее число станций без пересборки IDF смысла не имеет. Режим точки доступа — штатный WLED **AP opens: Disconnected**.

## Broadcast-адрес UDP-sync в режиме SoftAP

В upstream адрес широковещания для UDP-notifier считается как `~subnetMask | gatewayIP` интерфейса STA. Когда master работает **только как точка доступа**, STA-интерфейс не имеет адреса, `gatewayIP()` возвращает 0.0.0.0 (ESP32), и пакеты уходят на 0.0.0.255 — ноды на SoftAP ничего не получают.

Исправление: `lightmusicBroadcastAddress()` в `wled00/lightmusic_net_utils.h`:

| Состояние | Адрес |
|---|---|
| STA подключён | `~mask \| gateway` (как upstream) |
| Только SoftAP | broadcast подсети AP, т.е. `4.3.2.255` |
| Ни того, ни другого | `255.255.255.255` |

Используется в `notify()` (`wled00/udp.cpp`); в следующем этапе — и для broadcast-варианта аудио-пакетов.

## Профили сборки (FR-3)

Файл `platformio_lightmusic.ini` подключён через `extra_configs` в `platformio.ini`.

| Профиль | База | Особенности |
|---|---|---|
| `lightmusic_master_esp32` | `esp32dev` | `audioreactive` (INMP441), SoftAP 8, heartbeat 1000 мс, 2D/GIF/WS/OTA |
| `lightmusic_node_esp8266` | `nodemcuv2` (4 MB) | без аудио, без Particle System 2D (как upstream для 4 MB), 1D ленты |
| `lightmusic_node_esp32` | `esp32dev` | без аудио, 2D/GIF/PS |
| `lightmusic_node_esp32c3` | `esp32c3dev` | без аудио, одно ядро — проверять FPS |
| `lightmusic_node_esp32s3` | `esp32s3_4M_qspi` | без аудио, PSRAM |
| `lightmusic_node_esp32s2` | `lolin_s2_mini` | без аудио |

Во всех профилях отключены: Alexa, Hue Sync, IR, MQTT, Adalight, Loxone, ESP-NOW. Общие defines: `LIGHTMUSIC_FORK`, `LIGHTMUSIC_ROLE_MASTER`/`LIGHTMUSIC_ROLE_NODE`, `LIGHTMUSIC_AP_MAX_CONNECTIONS`, `LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL`.

Подводные камни PlatformIO, учтённые в файле: при `extends` список `build_flags` **заменяется**, а скрипт `pio-scripts/output_bins.py` берёт первое `WLED_RELEASE_NAME` — поэтому флаги в каждом env перечислены полностью; `custom_usermods` наследуется, поэтому node-профили явно обнуляют его.

Сборка:

```bash
~/.venvs/platformio/bin/pio run -e lightmusic_master_esp32
~/.venvs/platformio/bin/pio run -e lightmusic_node_esp8266
```
