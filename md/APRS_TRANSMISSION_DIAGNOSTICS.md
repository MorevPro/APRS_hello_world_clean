# 🔍 Диагностика проблемы передачи APRS на Flipper Zero

## 📋 Постановка проблемы

**Симптомы:**
- ✅ Direwolf успешно получает сигналы от iPhone (PulseModem A)
- ✅ Аудиовыход рации передает правильный звук (слышны пищащие тоны AFSK)
- ✅ Несколько нажатий на Flipper показывают звуки разной тональности
- ❌ Direwolf НЕ декодирует сообщения от Flipper
- ❌ **Вероятно: передача прерывается или режется на пакеты**

---

## 🔬 Гипотезы проблемы

### **Гипотеза 1: Передача прерывается между пакетами**
```
Что может быть неправильно:
- Каждое нажатие "TX" отправляет ОДИН пакет, но пакет может быть разбит
- Между флагами (синхронизирующими последовательностями) биты теряются
- Таймер TX callback может вызываться с непредусмотренными задержками
```

**Признак:** Слышны отдельные звуки (разная тональность), но не единое сообщение

---

### **Гипотеза 2: NRZI кодирование неправильно**
```
NRZI (Non-Return-to-Zero Inverted) кодирование:
- 0 бит → переход (уровень меняется)
- 1 бит → нет перехода (уровень не меняется)

С bit stuffing:
- После 5 подряд идущих 1-ов добавляется принудительный 0
```

**Что может быть неправильно:**
1. Bit stuffing работает некорректно
2. Уровни (true/false) в неправильном порядке
3. Пребабула (32 флага 0x7E) может быть неправильной длины
4. Постабула (3 флага 0x7E) может быть неправильной длины

---

### **Гипотеза 3: Buffer overflow или трусение данных**
```
#define APRS_HELLO_WORLD_TX_LEVEL_CAPACITY 2048U
#define APRS_GFSK_BIT_DURATION_US  104U

2048 битов × 104мкс = 213ms на один пакет
При частоте дискретизации 48000 Hz: это примерно 42ms
```

**Признак:** Если данные не вмещаются в буфер, они молча обрезаются!

---

## 🔧 Код для диагностики

Добавьте следующее логирование в файл `aprs_hello_world_clean_app.c`:

### **1. Расширенное логирование в aprs_refresh_frame()**

```c
static bool aprs_refresh_frame(APRSHelloWorldCleanApp* app) {
    aprs_sync_address_config(app);

    // === ДИАГНОСТИКА: Логирование конфигурации ===
    FURI_LOG_I(TAG, "=== FRAME GENERATION STARTED ===");
    FURI_LOG_I(TAG, "Source: %s-%u", app->address_config.source_call, app->address_config.source_ssid);
    FURI_LOG_I(TAG, "Dest: %s-%u", app->address_config.destination_call, app->address_config.destination_ssid);
    FURI_LOG_I(TAG, "Path1: %s-%u (use=%d)", app->address_config.path1_call, app->address_config.path1_ssid, app->address_config.use_path1);
    FURI_LOG_I(TAG, "Path2: %s-%u (use=%d)", app->address_config.path2_call, app->address_config.path2_ssid, app->address_config.use_path2);
    FURI_LOG_I(TAG, "Status: '%s'", app->address_config.status_text ? app->address_config.status_text : "(null)");
    FURI_LOG_I(TAG, "Lat: '%s', Lon: '%s'", app->address_config.position_lat, app->address_config.position_lon);

    // Try to encode position frame if coordinates are valid
    if(aprs_ax25_position_is_valid(&app->address_config)) {
        FURI_LOG_I(TAG, "Position is VALID, encoding position frame");
        app->frame_size = aprs_ax25_encode_position_frame(
            &app->address_config, app->frame_buffer, sizeof(app->frame_buffer));
        
        if(app->frame_size > 0) {
            snprintf(app->status_text, sizeof(app->status_text), "Position");
            FURI_LOG_I(TAG, "Position frame size: %u bytes", (unsigned int)app->frame_size);
        } else {
            aprs_set_status(app, "Position frame too large");
            FURI_LOG_E(TAG, "Position frame encoding FAILED - size = 0");
            return false;
        }
    } else {
        FURI_LOG_I(TAG, "Position is INVALID, encoding status frame");
        app->frame_size = aprs_ax25_encode_status_frame(
            &app->address_config, app->frame_buffer, sizeof(app->frame_buffer));
        
        if(app->frame_size == 0U) {
            aprs_set_status(app, "Status frame failed");
            FURI_LOG_E(TAG, "Status frame encoding FAILED");
            return false;
        }
        
        FURI_LOG_I(TAG, "Status frame size: %u bytes", (unsigned int)app->frame_size);
        snprintf(app->status_text, sizeof(app->status_text), "Status");
    }

    // === ДИАГНОСТИКА: Содержимое фрейма ===
    FURI_LOG_I(TAG, "Frame buffer contents (first 50 bytes as hex):");
    char hex_buffer[256] = {0};
    for(size_t i = 0; i < app->frame_size && i < 50; i++) {
        snprintf(hex_buffer + (i * 2), sizeof(hex_buffer) - (i * 2), "%02X", app->frame_buffer[i]);
    }
    FURI_LOG_I(TAG, "HEX: %s", hex_buffer);

    if(app->frame_size > 255U) {
        aprs_set_status(app, "Frame too large (>255)");
        FURI_LOG_E(TAG, "Frame size exceeds AX.25 maximum: %u > 255", (unsigned int)app->frame_size);
        return false;
    }

    // === ДИАГНОСТИКА: NRZI кодирование ===
    FURI_LOG_I(TAG, "Building NRZI stream...");
    FURI_LOG_I(TAG, "  Preamble flags: %u", APRS_PREAMBLE_FLAGS);
    FURI_LOG_I(TAG, "  Postamble flags: %u", APRS_POSTAMBLE_FLAGS);
    FURI_LOG_I(TAG, "  TX level capacity: %u", APRS_HELLO_WORLD_TX_LEVEL_CAPACITY);
    
    if(!aprs_ax25_build_nrzi_stream(
           app->frame_buffer,
           app->frame_size,
           APRS_PREAMBLE_FLAGS,
           APRS_POSTAMBLE_FLAGS,
           app->tx_levels,
           APRS_HELLO_WORLD_TX_LEVEL_CAPACITY,
           &app->tx_level_count)) {
        app->tx_level_count = 0U;
        aprs_set_status(app, "NRZI stream failed");
        FURI_LOG_E(TAG, "NRZI encoding FAILED");
        return false;
    }

    // === ДИАГНОСТИКА: Результат NRZI ===
    app->tx_bit_count = (uint32_t)app->tx_level_count;
    FURI_LOG_I(TAG, "NRZI stream generated: %lu bits total", app->tx_bit_count);
    FURI_LOG_I(TAG, "  Expected preamble bits: %u", APRS_PREAMBLE_FLAGS * 8);
    FURI_LOG_I(TAG, "  Expected frame bits: ~%u", (unsigned int)(app->frame_size * 8));
    FURI_LOG_I(TAG, "  Expected postamble bits: %u", APRS_POSTAMBLE_FLAGS * 8);
    FURI_LOG_I(TAG, "  Total expected: ~%lu bits", 
               (unsigned long)(APRS_PREAMBLE_FLAGS * 8 + app->frame_size * 8 + APRS_POSTAMBLE_FLAGS * 8));
    
    // === ДИАГНОСТИКА: Первые 100 битов NRZI ===
    if(app->tx_level_count > 0) {
        FURI_LOG_I(TAG, "First 100 NRZI bits (True=1, False=0):");
        char bits_buffer[101] = {0};
        for(size_t i = 0; i < app->tx_level_count && i < 100; i++) {
            bits_buffer[i] = app->tx_levels[i] ? '1' : '0';
        }
        bits_buffer[100] = '\0';
        FURI_LOG_I(TAG, "%s", bits_buffer);
    }

    aprs_hex_full_dump(
        app->frame_buffer, app->frame_size, app->last_tx_hex, sizeof(app->last_tx_hex));

    FURI_LOG_I(
        TAG,
        "AX25 %s: %s | frame=%u bytes | bits=%u | full_hex=%s",
        app->status_text,
        app->address_config.status_text ? app->address_config.status_text : "(empty)",
        (unsigned int)app->frame_size,
        (unsigned int)app->tx_bit_count,
        app->last_tx_hex);
    FURI_LOG_I(TAG, "=== FRAME GENERATION COMPLETE ===\n");

    aprs_set_status(app, "frame ready");
    return true;
}
```

---

### **2. Добавить логирование TX callback**

В файле `aprs_hello_world_clean_app.c` найдите функцию `aprs_radio_tx_callback` и добавьте логирование:

```c
static LevelDuration aprs_radio_tx_callback(void* context) {
    APRSHelloWorldCleanApp* app = context;

    if(app->tx_level_index >= app->tx_level_count) {
        // === ДИАГНОСТИКА: Конец передачи ===
        if(app->tx_level_index == app->tx_level_count) {
            FURI_LOG_I(TAG, "TX callback: End of transmission reached");
            FURI_LOG_I(TAG, "  Total bits transmitted: %lu", (unsigned long)app->tx_level_index);
            FURI_LOG_I(TAG, "  Expected bits: %lu", (unsigned long)app->tx_level_count);
        }
        return level_duration_reset();
    }

    const bool level = app->tx_levels[app->tx_level_index++];
    
    // === ДИАГНОСТИКА: Логирование каждого бита (осторожно - много логов!) ===
    // Раскомментируйте ТОЛЬКО если нужно видеть ВСЕ биты (замедлит систему!)
    // if(app->tx_level_index % 100 == 0) {  // Логировать каждый 100-й бит
    //     FURI_LOG_D(TAG, "TX bit %lu/%lu: level=%u", 
    //                (unsigned long)app->tx_level_index, 
    //                (unsigned long)app->tx_level_count, 
    //                level);
    // }
    
    return level_duration_make(level, APRS_GFSK_BIT_DURATION_US);
}
```

---

### **3. Добавить логирование в aprs_radio_start_tx()**

```c
static bool aprs_radio_start_tx(APRSHelloWorldCleanApp* app) {
    // ... существующий код ...
    
    FURI_LOG_I(TAG, "=== TX START REQUEST ===");
    FURI_LOG_I(TAG, "TX Configuration:");
    FURI_LOG_I(TAG, "  Frequency: %lu Hz (432.500 MHz)", (unsigned long)app->frequency_hz);
    FURI_LOG_I(TAG, "  Frame size: %u bytes", (unsigned int)app->frame_size);
    FURI_LOG_I(TAG, "  TX bits ready: %lu", (unsigned long)app->tx_bit_count);
    FURI_LOG_I(TAG, "  TX level capacity: %u", APRS_HELLO_WORLD_TX_LEVEL_CAPACITY);
    FURI_LOG_I(TAG, "  Bit duration: %u µs", APRS_GFSK_BIT_DURATION_US);
    FURI_LOG_I(TAG, "  Expected TX time: %lu ms", 
               (unsigned long)(app->tx_bit_count * APRS_GFSK_BIT_DURATION_US / 1000));
    
    // ... остальной код ...
    
    app->tx_level_index = 0U;
    app->tx_last_start_ok = furi_hal_subghz_start_async_tx(aprs_radio_tx_callback, app);

    if(!app->tx_last_start_ok) {
        furi_hal_power_suppress_charge_exit();
        aprs_set_status(app, "TX start failed");
        FURI_LOG_E(TAG, "furi_hal_subghz_start_async_tx() FAILED!");
        FURI_LOG_E(TAG, "  Radio may not be properly initialized");
        notification_message(app->notifications, &sequence_tx_error);
        aprs_radio_start_rx(app);
        return false;
    }

    notification_message(app->notifications, &sequence_tx_start);
    app->tx_running = true;
    app->tx_packets++;
    aprs_set_status(app, "TX in progress");
    FURI_LOG_I(TAG, "=== TX STARTED ===");
    FURI_LOG_I(TAG, "TX in progress - packets sent: %lu", (unsigned long)app->tx_packets);
    return true;
}
```

---

### **4. Улучшенное логирование в aprs_radio_poll()**

```c
static void aprs_radio_poll(APRSHelloWorldCleanApp* app) {
    if(!app->radio_ready) {
        return;
    }

    app->last_rssi = furi_hal_subghz_get_rssi();
    app->last_lqi = furi_hal_subghz_get_lqi();

    if(app->tx_running && furi_hal_subghz_is_async_tx_complete()) {
        FURI_LOG_I(TAG, "=== TX COMPLETE ===");
        FURI_LOG_I(TAG, "TX finished successfully");
        FURI_LOG_I(TAG, "  Bits sent: %lu / %lu", 
                   (unsigned long)app->tx_level_index, 
                   (unsigned long)app->tx_level_count);
        
        if(app->tx_level_index < app->tx_level_count) {
            FURI_LOG_W(TAG, "  ⚠️ WARNING: Not all bits were transmitted!");
            FURI_LOG_W(TAG, "     Missing bits: %lu", 
                      (unsigned long)(app->tx_level_count - app->tx_level_index));
        }
        
        furi_hal_subghz_stop_async_tx();
        furi_hal_power_suppress_charge_exit();
        furi_hal_subghz_idle();
        notification_message(app->notifications, &sequence_tx_stop);
        notification_message(app->notifications, &sequence_rx_activity);
        app->tx_running = false;
        aprs_set_status(app, "TX complete");
        FURI_LOG_I(
            TAG,
            "TX complete: packets=%lu last_rssi=%d last_lqi=%u",
            (unsigned long)app->tx_packets,
            (int)app->last_rssi,
            app->last_lqi);
        aprs_radio_start_rx(app);
    }
}
```

---

## 📊 Как использовать эту диагностику

### **Шаг 1: Отключите direwolf и закройте COM порт**
```powershell
# Закройте все приложения, использующие COM4
# Перезагрузите Flipper Zero
```

### **Шаг 2: Примените изменения кода**
- Добавьте приведенное выше логирование в `aprs_hello_world_clean_app.c`
- Пересоберите приложение: `ufbt build`
- Загрузите на Flipper: `ufbt launch`

### **Шаг 3: Запустите ufbt cli в режиме логирования**
```powershell
cd 'd:\projects\HAM\flipper zero\APRS_hello_world_clean'
ufbt cli
```

### **Шаг 4: В консоли Flipper запустите приложение**
```
> open_app APRS
```

### **Шаг 5: Нажмите TX несколько раз и соберите логи**
```
> log
```

### **Шаг 6: Сохраните логи в файл (в PowerShell)**
```powershell
ufbt cli | Tee-Object -FilePath aprs_diagnostics.log
```

---

## 🎯 Что искать в логах

### **Проблема 1: Буфер переполнен**
```
Ищите строку:
  Total bits transmitted: 2048 / 2048
  
Если ВСЕ 2048 бит отправляются, это означает, что:
- Фрейм слишком большой
- ИЛИ bit stuffing создает слишком много битов
- Решение: увеличить APRS_HELLO_WORLD_TX_LEVEL_CAPACITY
```

### **Проблема 2: Слишком мало битов**
```
Ищите строку:
  First 100 NRZI bits (True=1, False=0):
  0101010101... (слишком много одинаковых битов)
  
Флаги AX.25 должны быть: 0x7E = 01111110 (в NRZI: 01010101)
Если видите сплошные 1-ы (нет переходов) - проблема с NRZI кодированием!
```

### **Проблема 3: Пакет режется**
```
Ищите строку:
  Not all bits were transmitted!
  Missing bits: XXX
  
Это означает, что TX callback был преждевременно остановлен
Возможные причины:
  - Таймер истек раньше, чем нужно
  - CC1101 модуль сбросился
```

---

## 🔋 Размеры и вычисления

```
AX.25 фрейм:
  Адрес источника:    7 байт
  Адрес получателя:   7 байт
  Пути (опционально): 7-14 байт
  Контроль:           1 байт
  PID:                1 байт
  Данные:             ~20-60 байт
  CRC:                2 байт
  ────────────────────────────
  Итого:              ~45-92 байт

С NRZI кодированием (bit-stuffing добавляет ~5-10%):
  45 байт  × 8 = 360 бит → ~396-450 бит с stuffing
  92 байта × 8 = 736 бит → ~800-900 бит с stuffing

С преамбулой (32 флага) и постамбулой (3 флага):
  Пребабула: 32 × 8 = 256 бит
  Фрейм:     ~360-900 бит
  Постамбула: 3 × 8 = 24 бита
  ────────────────────────────
  Total: ~640-1180 бит (легко умещается в 2048!)

Время передачи при 104 µs на бит:
  1000 бит × 104 µs = 104 ms (~0.1 сек)
  2000 бит × 104 µs = 208 ms (~0.2 сек)
```

---

## 🚨 Критические проверки

```
✓ ОБЯЗАТЕЛЬНО проверите логи на сообщение об ошибке:
  - "furi_hal_subghz_start_async_tx() FAILED!"
  - "NRZI encoding FAILED"
  - "Position frame encoding FAILED"
  - "Frame size exceeds AX.25 maximum"

✓ Убедитесь, что видно сообщение:
  - "TX STARTED"
  - "TX complete"

✓ Проверьте значения:
  - Frequency: 432500000 Hz (точно!)
  - Bit duration: 104 µs (стандарт AFSK 1200 бит/с)
  - Total bits: NOT 2048 (иначе переполнение)
```

---

## 📝 Примеры вывода при правильной работе

```
=== FRAME GENERATION STARTED ===
Source: KV2TST-0
Dest: APRS-0
Path1: WIDE1-1 (use=1)
Status: 'Test beacon'
Position is VALID, encoding position frame
Position frame size: 65 bytes
Frame buffer contents (first 50 bytes as hex):
HEX: 829A84A6A460E0829A84A6A488604F60E0A844...
Building NRZI stream...
  Preamble flags: 32
  Postamble flags: 3
  TX level capacity: 2048
NRZI stream generated: 987 bits total
  Expected preamble bits: 256
  Expected frame bits: ~520
  Expected postamble bits: 24
  Total expected: ~800 bits
First 100 NRZI bits (True=1, False=0):
0101010101010101110110110110...  ← видны флаги (0101) и данные (сложнее)
AX25 Position: Test beacon | frame=65 bytes | bits=987
=== FRAME GENERATION COMPLETE ===

=== TX START REQUEST ===
TX Configuration:
  Frequency: 432500000 Hz (432.500 MHz)
  Frame size: 65 bytes
  TX bits ready: 987
  Expected TX time: 102 ms
=== TX STARTED ===
TX in progress - packets sent: 1
=== TX COMPLETE ===
TX finished successfully
  Bits sent: 987 / 987
  ✓ All bits transmitted successfully!
```

---

## 🎓 Заключение

Благодаря этой диагностике вы сможете:
1. **Видеть точно, что формируется** - каждый байт фрейма
2. **Видеть точно, что кодируется** - каждый бит NRZI
3. **Видеть точно, что передается** - статус TX callback
4. **Обнаружить потери** - если биты обрезаны или потеряны

Самые частые проблемы:
- ❌ Буфер слишком маленький (APRS_HELLO_WORLD_TX_LEVEL_CAPACITY = 2048)
- ❌ NRZI кодирование содержит ошибки
- ❌ Bit stuffing считается неправильно
- ❌ TX callback прерывается до конца

После применения этой диагностики точно найдется проблема! 🎯
