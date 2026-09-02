# Протокол синхронизации master → node

Требования — `spec.md`, FR-4 и FR-5. Реализован режим A (State Sync heartbeat); режим B (Audio Feature Sync) — следующий этап, здесь зафиксирован задел.

## Режим A: State Sync heartbeat (реализовано)

Master периодически повторяет **штатный WLED UDP notifier-пакет** (порт 21324, протокол 0, версия 12): яркость, цвета, эффект, палитра, скорость/интенсивность, transition, timebase, все активные сегменты с 2D-параметрами, байт `syncGroups` (offset 36). Ноды применяют его тем же кодом, что и обычные уведомления, и фильтруют по `receiveGroups & syncGroups != 0`.

Особенности реализации (`wled00/udp.cpp`, `wled00/lightmusic_sync_heartbeat.h`):

- Новый callMode `CALL_MODE_LIGHTMUSIC_HEARTBEAT` (13). Приёмники upstream принимают любой callMode ≤ 199, так что пакет полностью совместим.
- Heartbeat **не зависит** от runtime-переключателя «Send notifications» и от флагов «on direct change / on button»: он гейтится только собственным интервалом, `syncGroups`, наличием UDP-сокета и связью (`WLED_CONNECTED || apActive`).
- Не отправляется, пока активен realtime-поток (E1.31/DDP/UDP realtime), чтобы не сбивать ноды устаревшим snapshot.
- Отправка heartbeat **не обновляет** `notificationSentTime`/`notificationCount`: иначе каждый пакет взводил бы ретрансмиты `udpNumRetries` и держал бы постоянно открытым окно подавления входящих (1 с после своей отправки).
- Планировщик без «догонки»: после долгой задержки loop уходит один пакет, следующий — через полный интервал. Обычное уведомление засчитывается как отправленное состояние, так что heartbeat не дублирует его раньше, чем через интервал.
- Полученный по UDP пакет не ретранслируется (штатный anti-loop WLED: `CALL_MODE_NOTIFICATION` не рассылается).

Настройка: Settings → Sync → **Full-state heartbeat interval (ms, 0 = off, min 1000)**; `cfg.json` → `if.sync.send.hb`. Значения 1…999 поднимаются до 1000. Значение по умолчанию задаётся при сборке (`LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL`: master — 1000, ноды — 0).

## Группы (FR-5)

Штатная 8-битная модель WLED: `syncGroups` (отправитель) и `receiveGroups` (приёмник), пакет применяется при `receiveGroups & syncGroups != 0`. Нода может состоять в нескольких группах.

## Режим B: Audio Feature Sync (план)

Основа — upstream UDP Sound Sync V2 усермода `audioreactive`: 44 байта, заголовок `"00002"`, порт 11988, multicast 239.0.0.1, ~50 пакетов/с. Master уже умеет передавать (`sync.mode` = send), ноды ESP8266/ESP32 — принимать; эффекты берут данные из единой точки `getAudioData()` и не требуют микрофона.

Что добавляется в следующем этапе (совместимо с upstream, размер и заголовок не меняются):

| Поле | Offset | Назначение |
|---|---|---|
| `reserved1[0]` | 6 | маска групп отправителя |
| `reserved2` | 17 | счётчик кадров (sequence) для отбрасывания дубликатов и старых пакетов |
| `reserved3` | 34–35 | резерв (soundPressure / zeroCrossing из MoonModules — по потребности) |

Приём: фильтр по группам, sequence-check, обработка только последнего пакета из очереди (purge), настраиваемый таймаут с затуханием / fallback. Доставка: переключатель **multicast / broadcast** (broadcast — через `lightmusicBroadcastAddress()`, надёжнее на SoftAP).
