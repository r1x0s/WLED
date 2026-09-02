# Протокол синхронизации master → node

Требования — `spec.md`, FR-4 и FR-5. Реализованы режим A (State Sync heartbeat) и базовый режим B (Audio Feature Sync поверх upstream UDP Sound Sync); расширения режима B отложены до полевого теста.

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

## Режим B: Audio Feature Sync (базовый режим реализован)

Основа — upstream UDP Sound Sync усермода `audioreactive`: пакет V2, 44 байта, заголовок `"00002"`, порт 11988, multicast 239.0.0.1, ~50 пакетов/с. Master делает FFT один раз и рассылает `sampleRaw`, `sampleSmth`, `samplePeak`, `fftResult[16]`, `FFT_Magnitude`, `FFT_MajorPeak`. Ноды принимают пакет и подставляют его в единую точку `getAudioData()` (`wled00/FX.cpp`), поэтому все аудиоэффекты (1D, 2D, Particle System) рендерятся на ноде локально без микрофона и без правок эффектов.

Что сделано в форке (Этап 2, минимальный):

- `usermods/audioreactive/audio_reactive.cpp`: build-time умолчание режима `LIGHTMUSIC_AUDIOSYNC_MODE` (0 = как в upstream, 1 = send, 2 = receive).
- Профили: master — `UM_AUDIOREACTIVE_ENABLE` + режим **send** (тип микрофона generic I²S для INMP441, пины настраиваются в UI); все ноды собирают `audioreactive` в режиме **receive**; ESP32-ноды дополнительно `SR_DMTYPE=254` («None - network receive only»), поэтому I²S-драйвер и FFT-задача на них не запускаются; на ESP8266 в receive-only сборке FFT/I²S вырезаны самим upstream.
- Режим можно изменить в UI: Config → Usermods → AudioReactive → `sync:mode`; сохраняется в `cfg.json` (`AudioReactive.sync.mode`).

Как проверить на железе:

1. Master: Info → «Audio» показывает уровень/GEQ при звуке с INMP441; в настройках usermod `sync:mode = Send`.
2. Нода: Info → «Sound Sync» показывает «receiving v2» при приёме пакетов (порог 2.5 с); эффект GEQ / Gravcenter / Ripplepeak реагирует на музыку у master.
3. При остановке музыки нода замирает на последнем кадре — ожидаемое поведение до реализации таймаута (см. ниже).

Что отложено до результатов полевого теста (совместимо с upstream, размер и заголовок пакета не меняются):

| Поле | Offset | Назначение |
|---|---|---|
| `reserved1[0]` | 6 | маска групп отправителя (фильтр `receiveGroups & mask`, 0 = все) |
| `reserved2` | 17 | счётчик кадров (sequence) для отбрасывания дубликатов и старых пакетов |
| `reserved3` | 34–35 | резерв (soundPressure / zeroCrossing из MoonModules — по потребности) |

Приём: обработка только последнего пакета из очереди (повторный `parsePacket()` сам отбрасывает предыдущий — одинаково на обоих ядрах), OR-накопление `samplePeak` по пропущенным пакетам, настраиваемый таймаут с затуханием `volumeSmth` и `fftResult[]`. Доставка: переключатель **multicast / broadcast** (broadcast через `lightmusicBroadcastAddress()`, обходит IGMP и надёжнее на SoftAP; на ESP8266 `beginMulticast` использует `WiFi.localIP()`, что не работает в AP-only режиме). Частота отправки: сейчас жёстко ~50 Гц (`> 20 мс`), таймер общий с приёмом — разделить перед тем, как делать настраиваемой.
