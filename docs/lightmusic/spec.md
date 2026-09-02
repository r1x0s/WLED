# Техническое задание: форк WLED для распределённой светомузыки

**Основа:** WLED `v16.0.1` (стабильный тег upstream)  
**Целевая архитектура:** ESP32 NodeMCU как master/контроллер/точка доступа/аудиоанализатор; до 8 ведомых LED-нод (ESP8266 для 1D-лент; ESP32/ESP32-C3/S2/S3 — преимущественно для 2D-матриц).  
**Рабочая ветка:** `lightmusic/main`  
**Исходный upstream:** `https://github.com/wled/WLED.git`  
**Статус документа:** актуализирован по реализованным изменениям (Этап 1 «Фундамент», 2026-09-02, репозиторий `r1x0s/WLED`).

---

## 1. Цель проекта

Создать лёгкий и поддерживаемый форк WLED для домашней и выездной светомузыки с центральным ESP32 и распределёнными контроллерами светодиодных лент.

Решение должно:

1. Работать в домашней Wi‑Fi сети, минимально нагружая ESP32 в роли точки доступа.
2. При отсутствии известных сетей автоматически превращать ESP32-master в Wi‑Fi роутер для ведомых устройств.
3. Поддерживать не менее 8 одновременных клиентов-нод в SoftAP режиме master (по умолчанию — 8).
4. Позволять группировать светильники и управлять группами раздельно.
5. Автоматически возвращать актуальное состояние света узлам после переподключения.
6. Сохранять Web UI, WebSockets, OTA, 1D/2D LED-поддержку и возможность GIF для 2D.
7. Использовать ESP32 для анализа аудио с INMP441; ведомые ноды должны оставаться простыми контроллерами лент/матриц без собственного аудиоанализа.
8. В дальнейшем поддерживать локальное управление через OLED 128×64 и энкодер.

---

## 2. Аппаратная архитектура

### 2.1 Master

- MCU: классический ESP32 NodeMCU.
- Роли:
  - центральное управление;
  - Wi‑Fi STA-клиент при доступной домашней/внешней сети;
  - SoftAP fallback при отсутствии доступных сетей;
  - обработка аудио INMP441;
  - рассылка групповых состояний;
  - хранение реестра ведомых устройств;
  - Web UI и WebSocket API;
  - в будущем: OLED + encoder.

### 2.2 Ведомые ноды

- MCU: ESP8266 (простые 1D-ленты) **или** ESP32 / ESP32-C3 / ESP32-S2 / ESP32-S3 (преимущественно 2D-матрицы, GIF, тяжёлые эффекты).
- Количество: до 8 одновременно подключённых нод.
- Роли:
  - подключение к наиболее приоритетной доступной Wi‑Fi сети;
  - приём WLED UDP state snapshot;
  - применение состояния только для назначенных групп;
  - управление своей LED-лентой или 2D-матрицей.
- Ни одна нода не выполняет аудиоанализ — источником аудиопризнаков всегда является master.

### 2.3 Пины

Не фиксировать аппаратные пины в исходном коде форка.

Требования:

- сохранить дефолтные значения WLED;
- использовать штатные настройки WLED через Web UI;
- GPIO INMP441, OLED, encoder, AUX и LED выходов должны конфигурироваться пользователем;
- не допускать конфликтов пинов между I²S, I²C, LED-выходами и пользовательскими модулями.

> INMP441 передаёт аудиоданные по I²S; I²C остаётся доступной шиной для OLED и других периферийных устройств.

---

## 3. Базовая политика форка

1. Базироваться на стабильных релизах WLED, начиная с `v16.0.1`.
2. Не переносить MoonModules целиком: он существенно разошёлся с upstream и создаёт высокую стоимость обновлений.
3. Заимствовать из MoonModules только отдельные, изолированные и проверенные функции после сравнения, тестирования и сборки на актуальном WLED.
4. Не удалять подсистемы WLED физически без необходимости. Для облегчённых прошивок применять профили PlatformIO и `WLED_DISABLE_*` flags.
5. Не редактировать автоматически сгенерированные `wled00/html_*.h` и `wled00/js_*.h` вручную.
6. Каждая новая логика должна иметь тест до реализации, где это возможно, и обязательную проверку сборкой firmware.

---

## 4. Функциональные требования

### FR-1. Несколько Wi‑Fi сетей с приоритетом

Каждое WLED-устройство (master или node) должно хранить несколько Wi‑Fi сетей, включая SSID, пароль, BSSID, статические сетевые параметры и числовой приоритет `0…255`.

Алгоритм выбора сети:

1. При старте и после потери связи выполнить асинхронный scan доступных SSID.
2. Выбрать известную видимую сеть с наибольшим числовым priority.
3. При равном priority выбрать сеть с лучшим RSSI.
4. При полном равенстве выбирать сеть, расположенную выше в настройках WLED.
5. Если для профиля указан BSSID, использовать его как привязку к конкретной точке доступа внутри соответствующего SSID.
6. Не рвать уже стабильное подключение только потому, что в эфире появилась сеть с большим priority. Приоритет используется во время подключения и переподключения.

Совместимость:

- старый `cfg.json`, где поле priority отсутствует, должен загружаться без ошибки;
- отсутствующее поле `prio` трактуется как `0`.

### FR-2. Fallback SoftAP на master

Master должен использовать штатный режим WLED **AP opens: Disconnected**:

1. Сначала попытаться подключиться к известным Wi‑Fi согласно FR-1.
2. Если доступных сетей нет — создать SoftAP.
3. Если Wi‑Fi STA подключился — выключить fallback SoftAP, кроме режима «Always».
4. При потере STA-сети снова пробовать известные SSID, затем возвращаться к SoftAP.

Для профиля master SoftAP должен разрешать **8 одновременных станций** (значение по умолчанию).

> Ограничение платформы: ESP-IDF допускает до 10 станций на SoftAP classic ESP32; значение 8 находится в допустимых пределах, но требует нагрузочного теста (см. §7.3).

### FR-3. Профили сборки

Должны быть отдельные профили (master + два типа нод):

#### `lightmusic_master_esp32`

Содержит:

- Web UI и WebSockets;
- OTA;
- UDP sync;
- 1D/2D функции WLED;
- возможность GIF/2D без отключения соответствующих функций;
- audioreactive для ESP32;
- SoftAP на 8 клиентов;
- heartbeat синхронизации 1 раз в секунду.
- Improv Wi‑Fi scan.

Отключает в baseline:

- Alexa;
- Hue Sync;
- IR;
- MQTT;
- Adalight;
- Loxone;
- ESP-NOW;

#### `lightmusic_node_esp8266`

Содержит:

- Web UI и WebSockets;
- OTA;
- Wi‑Fi priority;
- UDP sync receive;
- штатное управление LED.
- Improv Wi‑Fi scan;
- Particle System 1D (2D-вариант PS на ESP8266 невозможен: upstream запрещает 1D и 2D PS одновременно на этой платформе через `#error` в `FX.cpp`; 2D-ноды — на ESP32-семействе).

Отключает:

- аудиоанализ;
- Alexa;
- Hue Sync;
- IR;
- MQTT;
- Adalight;
- Loxone;
- ESP-NOW;
- Particle System 2D.

#### `lightmusic_node_esp32` (и варианты `_c3`/`_s2`/`_s3`)

Профиль ведомой ноды для 2D-матриц на ESP32-семействе.

Содержит:

- всё из `lightmusic_node_esp8266`;
- полную 2D-поддержку (matrix mapping, segment bounds/options);
- GIF/Pixel Art;
- Particle System 1D/2D;
- приём Audio Feature Sync с локальной отрисовкой audio-reactive effects (после реализации FR-4 режим B).

Отключает то же, что node-профиль ESP8266, включая локальный аудиоанализ (микрофонный вход на нодах не используется).

> Для ESP32-C3 учитывать: одно ядро RISC-V и меньший запас CPU — профиль должен собираться без audioreactive и без тяжёлых опций; пригодность для конкретных матриц подтверждается FPS-тестом.

### FR-4. Heartbeat синхронизации состояния

Система должна поддерживать **два независимых режима распространения данных master → node**. Режим выбирается в конфигурации master и в дальнейшем — для каждой целевой группы.

#### Режим A: State Sync (полное состояние WLED)

Master должен рассылать полное состояние WLED:

- сразу при обычном изменении состояния;
- дополнительно раз в 1 секунду.

Периодический пакет должен содержать нормальный WLED UDP state snapshot:

- яркость;
- цвета;
- эффект;
- палитру;
- скорость/интенсивность;
- timing;
- параметры активных сегментов;
- параметры, необходимые для 2D;
- битовую маску групп отправителя.

Ограничения:

- частота heartbeat по умолчанию: 1000 мс;
- запрещать конфигурации с ненулевым interval меньше 1000 мс;
- heartbeat не должен блокироваться обычными флагами «уведомлять при UI/кнопке»;
- полученный UDP snapshot нельзя отправлять повторно, чтобы исключить sync-loop;
- preset ID не является единственным источником истины: одинаковый номер preset на разных нодах может иметь различное содержимое.

Назначение режима: обычные неаудиореактивные эффекты, точное восстановление визуального состояния после reconnect, воспроизведение одинакового preset/state на простых нодах.

#### Режим B: Audio Feature Sync (аудиопризнаки, отрисовка на node)

Master получает звук от INMP441/AUX, выполняет захват, фильтрацию, AGC и FFT. Он **не передаёт raw audio и не передаёт готовые значения каждого LED**. Вместо этого он передаёт компактный декодированный набор аудиопризнаков, а каждая node самостоятельно запускает выбранный audio-reactive effect и формирует пиксели своей ленты/матрицы.

Пакет Audio Feature Sync должен содержать как минимум совместимый с upstream audioreactive V2 набор:

- `sampleRaw` — мгновенный уровень;
- `sampleSmth` — сглаженный уровень;
- `samplePeak` — событие пика/удара;
- `fftResult[16]` — 16 частотных полос FFT с амплитудой 0…255;
- `FFT_Magnitude` — амплитуда доминирующей частоты;
- `FFT_MajorPeak` — доминирующая частота;
- protocol version, sequence/timestamp и group mask.

Требования режима Audio Feature Sync:

1. Master выполняет FFT только один раз и публикует результаты для назначенных групп.
2. Node не запускает FFT и не требует подключённого микрофона; она принимает audio feature packet и использует его как источник данных для стандартных audio-reactive effects.
3. Node отрисовывает эффект локально, поэтому лента, матрица, число LED, 1D/2D mapping, segment bounds и параметры эффекта могут быть разными на разных устройствах.
4. Пакеты должны передаваться с частотой, достаточной для плавной реакции на музыку; целевой начальный диапазон — 20…50 Гц. Точная частота должна быть настраиваемой и подтверждённой нагрузочными тестами с восемью nodes.
5. При потере Audio Feature Sync node должна иметь configurable timeout: остановка/затухание эффекта, последнее корректное состояние либо переход к State Sync.
6. Пакеты должны фильтроваться по `receiveGroups & syncGroups != 0` и не должны ретранслироваться.
7. По возможности следует использовать/расширять существующий upstream UDP Sound Sync (`audioreactive`, V2 payload 44 bytes, стандартный порт 11988), а не изобретать несовместимый протокол. Group mask и sequence/version допускается добавить как расширение с явной проверкой совместимости.

Назначение режима: светомузыка. Он заметно экономичнее полноценной передачи пикселей и позволяет каждой ноде по-своему визуализировать одинаковый музыкальный материал.

> Важное уточнение: текущий реализованный heartbeat относится к режиму **State Sync**. Audio Feature Sync зафиксирован в ТЗ как обязательный следующий функциональный этап, но ещё не реализован в коде.

### FR-5. Группы синхронизации

Использовать стандартную 8-битную модель WLED:

- `syncGroups` — группы назначения пакета;
- `receiveGroups` — группы, которые слушает node;
- нода применяет пакет, если:

```text
receiveGroups & syncGroups != 0
```

Требования к будущему UI:

- выбор «все группы»;
- выбор группы 1…8;
- назначение ноды в одну или несколько групп;
- отображение групп понятными именами.

### FR-6. Именованный реестр ведомых устройств

Master должен хранить до 8 records:

```text
MAC → human-readable name → group mask → capabilities → last seen → config revision
```

Примеры имён:

```text
Lamp1
Lamp2
Wall
Background
Table
```

Минимальные правила:

- MAC — стабильный идентификатор;
- нельзя создать дубликат одной MAC;
- имя не может быть пустым;
- group mask должна быть ненулевой;
- нода может находиться в нескольких группах;
- для новой ноды формируется понятное fallback-имя `Node-<MAC>`.

### FR-7. Безопасный pairing и provisioning

Реестр master не должен меняться от любого UDP broadcast.

Будущий flow:

1. Нода отправляет announce со стабильным identity.
2. Master показывает новое устройство как unpaired.
3. Пользователь явно включает pairing mode и подтверждает устройство.
4. Пользователь назначает имя и группы.
5. Master отправляет revisioned конфигурацию ноде.
6. Нода сохраняет назначение, подтверждает revision и применяет группы.

До реализации pairing локальный `receiveGroups` на ноде остаётся источником истины.

### FR-8. Аудио

Master должен использовать upstream usermod `audioreactive` для INMP441 в режиме generic I²S.

Требования:

- не включать аудиоанализ ни в один node build (ESP8266 и ESP32-семейство);
- предоставить выбор аудиопинов через штатный интерфейс WLED;
- использовать доступные upstream параметры gain, AGC, squelch, источник звука;
- будущий AUX не реализовывать через жёстко заданные пины;
- для качественного AUX предпочесть I²S ADC/codec, например ES7243/ES8388;
- прямой ADC classic ESP32 допустим только как отдельная экспериментальная опция с документированными ограничениями.

### FR-9. OLED 128×64 и энкодер (будущее)

Взять за основу существующие WLED usermods:

- `usermod_v2_four_line_display_ALT`;
- `usermod_v2_rotary_encoder_ui_ALT`.

Создать единое local UI для master:

- home screen: preset, effect, brightness, текущая target group, Wi‑Fi/AP status, число online nodes;
- encoder rotate: навигация;
- encoder click: выбор/применение;
- long press: back/home;
- presets: применить к выбранной группе или ко всем;
- audio: gain, AGC, squelch, текущий source/status;
- groups: выбор target group;
- nodes: список имён и online/offline;
- network: текущий SSID, IP, SoftAP status.

### FR-10. 2D, GIF и DMX

- 2D не отключать в master.
- GIF/Pixel Art не запрещать на ESP32/2D профилях.
- 2D-ноды строятся на ESP32/ESP32-C3/S2/S3 (профиль `lightmusic_node_esp32*`); ESP8266 используется только для 1D-лент из-за ограничений flash/RAM.
- DMX/E1.31/Art-Net сейчас не включать в baseline.
- При будущей интеграции с Rekordbox сначала подтвердить его фактический протокол/оборудование, затем выбрать DMX input/output, sACN/E1.31 или Art-Net.

---

## 5. Реализовано на момент составления ТЗ

### 5.1 Wi‑Fi priority

Реализовано:

- `WiFiConfig.priority`;
- конфигурационное поле JSON `nw.ins[].prio`;
- Web UI input `Priority (0-255, higher connects first)`;
- выбор сети: priority → RSSI → порядок конфигурации;
- обратная совместимость конфигурации без `prio`;
- reconnect после изменения priority.

Основные затронутые файлы:

```text
wled00/fcn_declare.h
wled00/cfg.cpp
wled00/set.cpp
wled00/xml.cpp
wled00/network.cpp
wled00/data/settings_wifi.htm
wled00/lightmusic_wifi_priority.h
test/lightmusic/wifi_priority_test.cpp
```

### 5.2 Master/node PlatformIO profiles и SoftAP capacity

Реализовано:

- профили `lightmusic_master_esp32`, `lightmusic_node_esp8266`, `lightmusic_node_esp32`, `lightmusic_node_esp32c3`, `lightmusic_node_esp32s3`, `lightmusic_node_esp32s2` в `platformio_lightmusic.ini` (подключён через `extra_configs`);
- compile-time значение `LIGHTMUSIC_AP_MAX_CONNECTIONS` (default **8**), клампер по лимиту платформы в `lightmusic_ap_config.h`;
- обе платформы передают лимит в штатный `WiFi.softAP(..., max_connection)`;
- обоснование потолка 8: DHCP-сервер ESP-IDF 4.4 выдаёт максимум 8 адресов.

Основные файлы:

```text
platformio.ini
platformio_lightmusic.ini
wled00/lightmusic_ap_config.h
wled00/const.h
wled00/wled.h
wled00/wled.cpp
test/lightmusic/ap_config_test.cpp
```

### 5.3 Heartbeat state synchronization

Реализовано:

- `CALL_MODE_LIGHTMUSIC_HEARTBEAT` (13);
- scheduler без burst после задержки loop; обычное уведомление засчитывается как последний heartbeat;
- master profile с `LIGHTMUSIC_SYNC_HEARTBEAT_INTERVAL=1000`, ноды — 0; runtime-настройка в Settings → Sync (`HB`) и `cfg.json` `if.sync.send.hb`, значения 1…999 поднимаются до 1000;
- normal WLED UDP snapshot каждую секунду;
- heartbeat работает вне runtime notify toggle и флагов «on direct change / on button», молчит при активном realtime-потоке;
- heartbeat не взводит ретрансмиты и не открывает окно подавления входящих пакетов;
- **исправлен дефект upstream**: в режиме «только SoftAP» broadcast-адрес UDP-sync считался как 0.0.0.255 на ESP32 (шлюз STA отсутствует); теперь используется broadcast подсети AP (`lightmusic_net_utils.h`).

Основные файлы:

```text
wled00/const.h
wled00/udp.cpp
wled00/wled.cpp
wled00/wled.h
wled00/lightmusic_sync_heartbeat.h
wled00/lightmusic_net_utils.h
wled00/data/settings_sync.htm
test/lightmusic/sync_heartbeat_test.cpp
test/lightmusic/net_utils_test.cpp
```

### 5.4 Core named node registry

Реализовано и покрыто host-тестом:

- ограничение `kLightmusicMaxNodes = 8`;
- upsert по MAC;
- недопущение дубликатов;
- fallback node name;
- изменение name;
- изменение group mask;
- валидация пустого имени и нулевой group mask.

Файлы:

```text
wled00/lightmusic_node_registry.h
test/lightmusic/node_registry_test.cpp
```

> Реестр пока не подключён к сети и UI намеренно: pairing/provisioning должен быть спроектирован безопасно до подключения сетевых side effects.

### 5.5 Audio Feature Sync

**Не реализовано.** Текущая master-прошивка включает `audioreactive` для локального анализа INMP441, но `lightmusic_node_esp8266` пока не получает FFT/audio features и не выполняет локальную отрисовку по удалённому аудиосигналу. Это следующий основной этап после pairing/provisioning.

---

## 6. Принятые технические решения

### 6.1 Почему heartbeat передаёт state, а не только номер preset

Preset ID является локальным для устройства. Preset `5` на master и preset `5` на node могут содержать разные эффект, цвета, сегменты или палитру. Поэтому для восстановления ноды canonical data — полный state snapshot, а номер preset может быть только дополнительной меткой.

### 6.2 Почему master не должен постоянно переключаться на более приоритетный SSID

Постоянный active roaming может разрушать стабильную работу: ESP32 будет разрывать действующее соединение, когда в эфире появится SSID с большим priority. Для первого релиза строгость priority применяется при boot/reconnect. Active roaming можно добавить в будущем как отдельную явную опцию.

### 6.3 Почему MoonModules нельзя использовать как базу

MoonModules полезен как источник отдельных идей для аудио и эффектов, но его ветка существенно diverged от актуального WLED. Базой остаётся поддерживаемый upstream WLED; перенос допустим только небольшими, проверенными commits/features.

---

## 7. Проверка и критерии приёмки

### 7.1 Автоматические проверки

Для каждого изменения обязательно выполнить:

```bash
npm ci
npm run build
npm test

# Host tests (g++, -std=c++11 и -std=c++17, -Werror)
bash test/lightmusic/run.sh

# Firmware builds (PlatformIO в venv ~/.venvs/platformio)
for e in lightmusic_master_esp32 lightmusic_node_esp8266 lightmusic_node_esp32 \
         lightmusic_node_esp32c3 lightmusic_node_esp32s3 lightmusic_node_esp32s2; do
  ~/.venvs/platformio/bin/pio run -e $e
done
```

Также:

```bash
git diff --check
```

### 7.2 Результаты текущей проверки

Последние успешно проверенные результаты:

```text
npm test: 16 / 16 passed
host tests: 5 модулей × 2 стандарта, все зелёные

lightmusic_master_esp32:      RAM 79,720 / 327,680 (24.3%)  Flash 1,226,685 / 1,572,864 (78.0%)
lightmusic_node_esp8266:      RAM 44,932 /  81,920 (54.8%)  Flash   855,023 / 1,044,464 (81.9%)
lightmusic_node_esp32:        RAM 79,184 / 327,680 (24.2%)  Flash 1,186,881 / 1,572,864 (75.5%)
lightmusic_node_esp32c3:      RAM 69,656 / 327,680 (21.3%)  Flash 1,150,718 / 1,572,864 (73.2%)
lightmusic_node_esp32s3:      RAM 42,980 / 327,680 (13.1%)  Flash 1,122,893 / 1,572,864 (71.4%)
lightmusic_node_esp32s2:      RAM 50,128 / 327,680 (15.3%)  Flash 1,134,106 / 1,572,864 (72.1%)

Эксперимент: ESP8266 с Particle System 2D не собирается (upstream #error: 1D и 2D PS
одновременно на ESP8266 не поддерживаются) — нода ESP8266 остаётся с PS 1D.
```

### 7.3 Обязательный hardware test matrix

Перед выпуском проверить на реальном оборудовании:

1. Master подключается к сети с наибольшим priority.
2. При недоступности сети с самым высоким priority master подключается к следующей доступной.
3. При недоступности всех известных сетей master поднимает SoftAP.
4. К SoftAP подключаются 8 нод (смешанный состав: ESP8266 + ESP32/ESP32-C3).
5. После reconnect нода получает актуальное состояние не позже следующего heartbeat.
6. Нода из группы 1 не применяет state snapshot, отправленный только в группу 2.
7. Нода, состоящая в нескольких группах, принимает состояния каждой назначенной группы.
8. INMP441 корректно отдаёт аудиоданные; Wi‑Fi и LED rendering не деградируют недопустимо.
9. OTA обновляет master и node profile.
10. 2D ESP32 node корректно принимает segment state.

---

## 8. Артефакты сборки

Текущие бинарники:

```text
build_output/release/WLED_16.0.1_Lightmusic_Master_ESP32.bin
build_output/release/WLED_16.0.1_Lightmusic_Node_ESP8266.bin  (+ .bin.gz)
build_output/release/WLED_16.0.1_Lightmusic_Node_ESP32.bin
build_output/release/WLED_16.0.1_Lightmusic_Node_ESP32-C3.bin
build_output/release/WLED_16.0.1_Lightmusic_Node_ESP32-S3_4M.bin
build_output/release/WLED_16.0.1_Lightmusic_Node_ESP32-S2.bin
```

Проверенные SHA-256 сборки Этапа 1 (2026-09-02):

```text
59c57a2a5aad1a9c13377aa2da303323e7bd6276cea2ff6672926cfce403e336  WLED_16.0.1_Lightmusic_Master_ESP32.bin
199e0d6e7ae44b1770e84e5e37925bd7413e6d03cad11b92552d91f42d1a76e3  WLED_16.0.1_Lightmusic_Node_ESP8266.bin
33c81354ee8a2012daf2fd86e9e32bd5638f239a7b4fb4b55831883911fd8800  WLED_16.0.1_Lightmusic_Node_ESP32.bin
4241a27ce1f06bda41e1f77d8c4f47b9b7d5b5608798348350e784a01432b732  WLED_16.0.1_Lightmusic_Node_ESP32-C3.bin
ad12f39b6d3cce49a6e81a671cbaf8071cce13f103d531824b159a6115d48352  WLED_16.0.1_Lightmusic_Node_ESP32-S3_4M.bin
2d23b850db4670956c5cf6fa791d71ca1da042945668cd20b0d8a1c20bf31316  WLED_16.0.1_Lightmusic_Node_ESP32-S2.bin
```

> После последующих изменений firmware нужно всегда пересчитать checksums и выдать новые бинарники.

---

## 9. Roadmap / идеи на будущее

### P1. Pairing + provisioning

- подписанные или одноразовые pairing tokens;
- UI для approve/reject неизвестных MAC;
- сохранение реестра в `cfg.json`;
- `lastSeen`, RSSI, firmware version, capabilities;
- revisioned configuration ACK.

### P2. Независимое управление зонами

Текущая WLED state модель глобальна: один state не может одновременно хранить разные эффекты для разных групп. Для полноценного раздельного управления нужно:

- master-side per-zone state snapshots; или
- explicit per-target preset/state dispatch;
- API/UI target selector: All / Group 1…8 / named set;
- отдельное хранение текущего preset/state на зону.

### P3. Master UI: Nodes & Groups

- отображение нод по имени;
- online/offline + lastSeen;
- назначение группы checkbox-ами;
- rename;
- setup wizard для первой установки.

### P4. OLED + Encoder control surface

- объединить four-line OLED и rotary UI;
- быстрый выбор group target;
- применение preset к группе;
- audio controls;
- статус нод и сети.

### P5. Audio enhancements

- профили для INMP441;
- selectable audio source;
- I²S codec input для AUX;
- Audio Feature Sync: master FFT → group-targeted UDP audio features → локальная отрисовка эффекта на node;
- совместимость с upstream audioreactive UDP Sound Sync V2, расширенная group mask и защита от stale packets;
- таймаут, fade и fallback-политика node при потере аудиопакетов;
- оценка и точечный перенос подходящих MoonModules audio effects (см. `moonmodules-analysis.md`: профили INMP441, UDP sequence/purge, broadcast-опция, DC-blocker);
- benchmark CPU, FPS, packet loss, audio FFT latency.

### P6. 2D/GIF node profile (приоритет повышен)

- профиль `lightmusic_node_esp32` + варианты для C3/S2/S3;
- тест segment bounds/options в group sync;
- измерение RAM/flash для GIF и сложных эффектов;
- FPS-бенчмарк 2D-матриц на C3 (одно ядро) для подтверждения пригодности.

### P7. DMX / DJ integration

- исследовать конкретный DMX workflow Rekordbox;
- выбрать подходящий input: DMX, Art-Net или E1.31/sACN;
- внедрить как опциональный master profile, не утяжеляя базовую прошивку.

### P8. Обновление upstream

- периодически получать изменения upstream WLED;
- обновлять fork малыми merge/rebase шагами;
- прогонять master/node builds и host tests после каждого обновления;
- не принимать крупные MoonModules port без отдельного review и benchmark.

---

## 10. Документация проекта

Дополнительные документы:

```text
docs/lightmusic/moonmodules-analysis.md   (аудит WLED-MM и план cherry-pick)
docs/lightmusic/networking.md
docs/lightmusic/sync-protocol.md
docs/lightmusic/nodes-and-groups.md
.hermes/plans/2026-08-30_232343-lightmusic-master-node.md
```
