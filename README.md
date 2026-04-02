# APRS Hello World Clean for Flipper Zero

External application for Flipper Zero.

## Русский

### Статус

- Проект успешно собирается через `ufbt`
- Приложение успешно загружается и запускается на Flipper Zero через `ufbt launch`
- В текущей проверке устройство было доступно как `COM3`

### Состав проекта

- `application.fam` - манифест приложения Flipper Zero
- `aprs_hello_world_clean_app.c` - основной исходный код
- `aprs_hello_world_clean_app.h` - заголовочный файл
- `dist/` - результат сборки
- `md/` - журнал выполненных работ

### Быстрый старт для Windows

1. Установите `git`.
2. Установите Python 3.
3. Подключите Flipper Zero по USB.
4. Установите `ufbt`:

```powershell
py -m pip install --upgrade ufbt
```

5. Перейдите в папку проекта:

```powershell
cd "D:\projects\HAM\flipper zero\APRS_hello_world_clean"
```

6. Создайте локальное окружение проекта:

```powershell
ufbt dotenv_create
```

7. Сгенерируйте файлы для VS Code:

```powershell
ufbt vscode_dist
```

8. Соберите приложение:

```powershell
ufbt
```

9. Загрузите и запустите приложение на Flipper:

```powershell
ufbt launch
```

### Быстрый старт для Linux

1. Установите `git`.
2. Установите Python 3 и `pip`.
3. Подключите Flipper Zero по USB.
4. Установите `ufbt`:

```bash
python3 -m pip install --upgrade ufbt
```

5. Перейдите в папку проекта:

```bash
cd /path/to/APRS_hello_world_clean
```

6. Создайте локальное окружение проекта:

```bash
ufbt dotenv_create
```

7. Сгенерируйте файлы для VS Code:

```bash
ufbt vscode_dist
```

8. Соберите приложение:

```bash
ufbt
```

9. Загрузите и запустите приложение на Flipper:

```bash
ufbt launch
```

### Полезные команды

- Сборка: `ufbt`
- Очистка: `ufbt -c`
- Запуск на Flipper: `ufbt launch`
- CLI Flipper: `ufbt cli`
- Обновление SDK: `ufbt update`
- Обновление VS Code-конфигов: `ufbt vscode_dist`

### Экран настроек APRS

- Открытие: **три коротких нажатия «Вверх»** подряд на главном экране приложения. Имеется в виду полный короткий клик (нажали и отпустили), а не удержание
- Подсказка на главном экране: строка `3xUp Short=cfg ...` внизу дисплея.
- Внутри настроек: **Back** — назад (из подменю в список, из списка на главный экран).

### Передача APRS и конфигурация (Задача 3)

**Задача 3: TX из конфигурации** — Приложение теперь передаёт APRS кадры, используя параметры конфигурации, загруженные из `/ext/aprs/config.cfg`, вместо зашитых лабораторных значений:

- **Поля конфигурации**: позывной источника (с SSID), позывной адресата, path1 и path2 (с флагами включения), широта, долгота, азимут, скорость, текст статуса
- **Кадры статуса**: Стандартный APRS формат статуса AX.25 с пользовательским текстом (до 62 символов)
- **Кадры позиции**: Формат APRS Position Report (`!lat/lon/status`) отправляется при наличии валидных координат
- **Валидация кадра**: Размер AX.25 кадра не превышает 255 байт; кадры отклоняются при превышении лимита
- **Динамический выбор**: Кодировщик автоматически выбирает между Position Report (если координаты валидны) и Status кадром (fallback)

#### Изменения

1. **Удалено**: Зашитый текст "Flipper Zero lab beacon ..."; функция `aprs_ax25_make_mystatus()`
2. **Расширенные структуры**:
   - `AprsAx25AddressConfig` теперь включает `path2`, `status_text`, `position_lat`, `position_lon`, `bearing_deg` и `speed`
3. **Новые функции**:
   - `aprs_ax25_encode_position_frame()` — кодирует APRS position report
   - `aprs_ax25_position_is_valid()` — проверяет валидность координат для позиционного сообщения
4. **Обновленная логика кодировщика**:
   - `aprs_ax25_encode_status_frame()` теперь читает статус и позицию из конфигурации, поддерживает path2, валидирует размер кадра

### Почему меню не открывалось (исправлено в коде)

- Главный цикл событий раньше пересылал только `InputTypePress` в обработчик, поэтому **`InputTypeShort` никогда не доходил** до обработчика и жест triple-Up не срабатывал. Теперь main loop также пересылает `InputTypeShort` для **Up** на главном экране.
- Детектор triple-tap использует **Short** (полный короткий клик), а не сырые edges `Press`.

### Логи и проверка APRS TX (Задача 3)

**Просмотр логов передачи:**

1. Запусти приложение: `ufbt launch`
2. Откройте CLI в новом терминале: `ufbt cli`
3. Включите логирование: `log info`
4. На Flipper переведите в режим LAB TX (Right) и включите TX (Left)
5. Нажимайте OK (Center) для передачи

**Пример логов при передаче Status frame (полный формат):**
```
[APRS432] === Frame config ===
[APRS432] Source: KYCALL-1
[APRS432] Dest: APRS-0
[APRS432] Path1: WIDE2-2 (use=1)
[APRS432] Path2: WIDE1-1 (use=0)
[APRS432] Status: 'Flipper APRS cfg'
[APRS432] Lat: '00.0000N', Lon: '000.0000E'
[APRS432] Position invalid, encoding status frame
[APRS432] Status frame size: 42 bytes
[APRS432] AX25 Status: Flipper APRS cfg | frame=42 bytes | full_hex=82A0A4A64040609620F67F029E544C59434B414C20F32D31AC3EEC42
[APRS432] TX started: freq=432495971 frame=42 bits=617 full_hex=82A0A4A64040609620F67F029E544C59434B414C20F32D31AC3EEC42
[APRS432] TX complete: packets=7 last_rssi=-138 last_lqi=127
```

**Интерпретация логов:**
- `Source: KYCALL-1` — позывной источника из конфигурации (можно менять в настройках)
- `Dest: APRS-0` — стандартный адресат APRS
- `Path1: WIDE2-2 (use=1)` — используется path повторитель WIDE2-2
- `Status: 'Flipper APRS cfg'` — текст статуса из конфигурации
- `frame=42 bytes` — размер AX.25 кадра без флагов и NRZI-кодирования
- `bits=617` — количество бит в NRZI потоке (включая флаги и bit stuffing)

**Анализ полного hex-дампа кадра:**

`full_hex=82A0A4A64040609620F67F029E544C59434B414C20F32D31AC3EEC42`

Разбор по байтам:
```
82 A0 A4 A6 40 40 60        — AX.25 адреса (7+7 байт)
96                          — Control byte (0x03 = UI frame, нижние 2 бита)
20                          — PID (0xF0 = No L3 protocol)
F6 7F 02 9E                 — Control + PID (NRZI decoded)
54 4C 59 43 4B 41 4C 20 F3  — "KYCALL " + SSID битом
2D 31                       — "-1" в ASCII
AC 3E                       — Info ID и статус данные
EC 42                       — FCS (CRC) little-endian
```

**Структура адреса (7 байт на адрес):**

Для `APRS-0` (Destination):
- Буквы кодируются: ASCII << 1, затем пробелы
- APRS = 0x41 0x50 0x52 0x53 = 0x82 0xA0 0xA4 0xA6 (сдвинуто на 1)
- Пробелы для выравнивания
- Последний байт: 0x60 = 0x30 | SSID | (last ? 1 : 0)

**Проверка структуры кадра:**

- Байты 1-7: destination адрес, last=0
- Байты 8-14: source адрес, last=1 (если нет path)
- Байты 15-21: path1 адрес (если use_path1=1), last зависит от path2
- Байт 15: Control (0x03 = UI frame, иногда может быть другое значение)
- Байт 16: PID (0xF0 = No L3)
- Байт 17: Info ID ('>' = 0x3E для status, '!' = 0x21 для position)
- Байты 18+: payload (status text или position data)
- Последние 2 байта: FCS (CRC16 little-endian)

**Сохранение логов:**

Логи выводятся в CLI в реальном времени. Для сохранения скопируйте вывод из терминала (Ctrl+A, Ctrl+C) и сохраните в файл для последующего анализа.

### Отладка

- Файлы VS Code для отладки генерируются командой `ufbt vscode_dist`
- Конфигурации находятся в `.vscode/launch.json`


## English

### Status

- The project builds successfully with `ufbt`
- The application uploads and starts successfully on Flipper Zero with `ufbt launch`
- During the latest verification, the device was detected on `COM3`

### Project layout

- `application.fam` - Flipper Zero application manifest
- `aprs_hello_world_clean_app.c` - main source file
- `aprs_hello_world_clean_app.h` - header file
- `dist/` - build output
- `md/` - work log and setup notes

### Quick start for Windows

1. Install `git`.
2. Install Python 3.
3. Connect Flipper Zero over USB.
4. Install `ufbt`:

```powershell
py -m pip install --upgrade ufbt
```

5. Open the project directory:

```powershell
cd "D:\projects\HAM\flipper zero\APRS_hello_world_clean"
```

6. Create local project environment:

```powershell
ufbt dotenv_create
```

7. Generate VS Code project files:

```powershell
ufbt vscode_dist
```

8. Build the application:

```powershell
ufbt
```

9. Upload and launch the app on Flipper:

```powershell
ufbt launch
```

### Quick start for Linux

1. Install `git`.
2. Install Python 3 and `pip`.
3. Connect Flipper Zero over USB.
4. Install `ufbt`:

```bash
python3 -m pip install --upgrade ufbt
```

5. Open the project directory:

```bash
cd /path/to/APRS_hello_world_clean
```

6. Create local project environment:

```bash
ufbt dotenv_create
```

7. Generate VS Code project files:

```bash
ufbt vscode_dist
```

8. Build the application:

```bash
ufbt
```

9. Upload and launch the app on Flipper:

```bash
ufbt launch
```

### Useful commands

- Build: `ufbt`
- Clean: `ufbt -c`
- Launch on Flipper: `ufbt launch`
- Open Flipper CLI: `ufbt cli`
- Update SDK: `ufbt update`
- Refresh VS Code config: `ufbt vscode_dist`

### APRS settings screen

- Open the settings with **three short Up** presses on the main app screen (a full short click: press and release). On the Flipper firmware this is `InputTypeShort` for `InputKeyUp`.
- The maximum gap between two consecutive short clicks is **900 ms** (`APRS_TRIPLE_UP_WINDOW_MS` in `aprs_hello_world_clean_app.h`). Longer pauses reset the counter.
- Bottom hint on the main screen: `3xUp Short=cfg ...`.
- **Back** navigates back (submenu → list → main).

### APRS Transmission and Configuration

**Task 3: TX from Config** — The application now transmits APRS frames using configuration parameters loaded from `/ext/aprs/config.cfg` instead of hardcoded lab values:

- **Configuration fields**: source call (with SSID), destination call, path1 and path2 (with optional enable flags), latitude, longitude, bearing, speed, status text
- **Status frames**: Standard AX.25 status format with user-defined status text (up to 62 characters)
- **Position frames**: APRS Position Report format (`!lat/lon/status`) sent when valid coordinates are available
- **Frame validation**: AX.25 frame size must not exceed 255 bytes; frames are rejected if oversized
- **Dynamic selection**: The encoder automatically chooses between Position Report (if coordinates are valid) and Status frame (fallback)

#### Key Changes

1. **Removed**: Hardcoded "Flipper Zero lab beacon ..." status text; `aprs_ax25_make_mystatus()` function
2. **Enhanced structures**:
   - `AprsAx25AddressConfig` now includes `path2`, `status_text`, `position_lat`, `position_lon`, `bearing_deg`, and `speed`
3. **New functions**:
   - `aprs_ax25_encode_position_frame()` — encodes APRS position report
   - `aprs_ax25_position_is_valid()` — validates coordinate fields for position transmission
4. **Updated encoder logic**:
   - `aprs_ax25_encode_status_frame()` now reads status and position from config, supports path2, validates frame size

### Why the menu did not open (fixed in code)

- The main event loop only forwarded `InputTypePress` to `aprs_handle_input`, so **`InputTypeShort` never reached** the handler and the triple-Up gesture did not run. The main loop now also forwards `InputTypeShort` for **Up** on the main screen.
- Triple-tap detection uses **Short** (complete short click), not raw `Press` edges.

### Logs and APRS TX Verification (Task 3)

**How to view transmission logs:**

1. Launch the app: `ufbt launch`
2. Open CLI in new terminal: `ufbt cli`
3. Enable logging: `log info`
4. On Flipper switch to LAB TX mode (Right) and arm TX (Left)
5. Press OK (Center) to transmit

**Example logs for Status frame transmission (full format):**
```
[APRS432] === Frame config ===
[APRS432] Source: KYCALL-1
[APRS432] Dest: APRS-0
[APRS432] Path1: WIDE2-2 (use=1)
[APRS432] Path2: WIDE1-1 (use=0)
[APRS432] Status: 'Flipper APRS cfg'
[APRS432] Lat: '00.0000N', Lon: '000.0000E'
[APRS432] Position invalid, encoding status frame
[APRS432] Status frame size: 42 bytes
[APRS432] AX25 Status: Flipper APRS cfg | frame=42 bytes | full_hex=82A0A4A64040609620F67F029E544C59434B414C20F32D31AC3EEC42
[APRS432] TX started: freq=432495971 frame=42 bits=617 full_hex=82A0A4A64040609620F67F029E544C59434B414C20F32D31AC3EEC42
[APRS432] TX complete: packets=7 last_rssi=-138 last_lqi=127
```

**Log interpretation:**
- `Source: KYCALL-1` — source callsign from configuration (can be changed in settings)
- `Dest: APRS-0` — standard APRS destination callsign
- `Path1: WIDE2-2 (use=1)` — using path repeater WIDE2-2
- `Status: 'Flipper APRS cfg'` — status text from configuration
- `frame=42 bytes` — AX.25 frame size without flags and NRZI encoding
- `bits=617` — number of bits in NRZI stream (including flags and bit stuffing)

**Full hex-dump frame analysis:**

`full_hex=82A0A4A64040609620F67F029E544C59434B414C20F32D31AC3EEC42`

Byte-by-byte breakdown:
```
82 A0 A4 A6 40 40 60        — AX.25 addresses (7+7 bytes)
96                          — Control byte (0x03 = UI frame, lower 2 bits)
20                          — PID (0xF0 = No L3 protocol)
F6 7F 02 9E                 — Control + PID (NRZI decoded)
54 4C 59 43 4B 41 4C 20 F3  — "KYCALL " + SSID byte
2D 31                       — "-1" in ASCII
AC 3E                       — Info ID and status data
EC 42                       — FCS (CRC) little-endian
```

**Address structure (7 bytes per address):**

For `APRS-0` (Destination):
- Letters encoded as: ASCII << 1, then padding spaces
- APRS = 0x41 0x50 0x52 0x53 = 0x82 0xA0 0xA4 0xA6 (shifted by 1)
- Space padding for alignment
- Last byte: 0x60 = 0x30 | SSID | (last ? 1 : 0)

**Frame structure verification:**

- Bytes 1-7: destination address, last=0
- Bytes 8-14: source address, last=1 (if no path)
- Bytes 15-21: path1 address (if use_path1=1), last depends on path2
- Byte 15: Control (0x03 = UI frame, may vary)
- Byte 16: PID (0xF0 = No L3)
- Byte 17: Info ID ('>' = 0x3E for status, '!' = 0x21 for position)
- Bytes 18+: payload (status text or position data)
- Last 2 bytes: FCS (CRC16 little-endian)

**Saving logs:**

Logs appear in CLI in real-time. To save, copy terminal output (Ctrl+A, Ctrl+C) and paste into a file for later analysis.

### Debugging

- VS Code debugging files are generated by `ufbt vscode_dist`
- Debug configurations are stored in `.vscode/launch.json`
- Full low-level debugging usually requires an SWD probe such as ST-Link, CMSIS-DAP, J-Link, or Black Magic Probe
