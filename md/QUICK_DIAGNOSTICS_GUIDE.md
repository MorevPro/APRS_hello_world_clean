# 🛠️ Быстрое применение диагностики APRS Transmission

Используйте этот файл как шпаргалку для применения максимального логирования.

## ⚡ Быстрая Инструкция

### Шаг 1️⃣: Подготовка
```powershell
# Закройте все приложения (direwolf, qFlipper, etc.)
# Иначе COM порт будет занят!

# Перейдите в директорию проекта
cd 'd:\projects\HAM\flipper zero\APRS_hello_world_clean'
```

### Шаг 2️⃣: Применение диагностики к коду

Откройте файл `aprs_hello_world_clean_app.c` и найдите функцию:

**Найдите:** `static bool aprs_refresh_frame(APRSHelloWorldCleanApp* app)`

**Замените блок логирования** на расширенный из файла `APRS_TRANSMISSION_DIAGNOSTICS.md`

**Найдите:** `static LevelDuration aprs_radio_tx_callback(void* context)`

**Добавьте логирование** после строк получения бита

**Найдите:** `static bool aprs_radio_start_tx(APRSHelloWorldCleanApp* app)`

**Добавьте диагностические FURI_LOG_I** перед `furi_hal_subghz_start_async_tx()`

### Шаг 3️⃣: Пересборка и Загрузка
```powershell
# Пересоберите приложение
ufbt build

# Проверьте, что сборка успешна (не должно быть ошибок)
# Загрузите на Flipper
ufbt launch
```

### Шаг 4️⃣: Запуск диагностики
```powershell
# Убедитесь, что qFlipper and direwolf закрыты!

# Откройте CLI сессию с Flipper
ufbt cli

# В консоли Flipper выполните:
> open_app APRS

# Несколько раз нажмите TX (кнопка UP на главном экране)

# Сохраните логи в файл
ufbt cli | Tee-Object -FilePath "aprs_diagnostic_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
```

### Шаг 5️⃣: Анализ логов

Откройте полученный файл логов и поищите:

#### ✅ Признаки НОРМАЛЬНОЙ работы:
```
=== FRAME GENERATION STARTED ===
Position is VALID, encoding position frame
Position frame size: 65 bytes
Frame buffer contents: ...HEX CODE...
NRZI stream generated: 987 bits total
First 100 NRZI bits: 0101010101...  ← много переходов, не сплошные 1-ы
=== FRAME GENERATION COMPLETE ===

=== TX STARTED ===
TX in progress - packets sent: 1
=== TX COMPLETE ===
TX finished successfully
  Bits sent: 987 / 987
  ✓ ALL BITS TRANSMITTED!
```

#### ❌ Признаки ОШИБОК:

```
ОШИБКА #1: Буфер переполнен
---
NRZI stream generated: 2048 bits total  ← ВСЕГДА 2048!
Expected frame bits: ~520
Total expected: ~800 bits
→ ПРОБЛЕМА: actual (2048) >> expected (~800)
→ РЕШЕНИЕ: увеличить APRS_HELLO_WORLD_TX_LEVEL_CAPACITY

ОШИБКА #2: NRZI кодирование сломано
---
First 100 NRZI bits: 1111111111111...  ← СПЛОШНЫЕ 1-ы! Потеря переходов
→ ПРОБЛЕМА: bit stuffing не работает или NRZI инвертирован
→ РЕШЕНИЕ: проверить функцию aprs_ax25_emit_byte_nrzi()

ОШИБКА #3: TX callback прерван
---
TX finished successfully
  Bits sent: 500 / 987  ← ТОЛЬКО ПОЛОВИНА!
  ⚠️ WARNING: Not all bits were transmitted!
  Missing bits: 487
→ ПРОБЛЕМА: TX остановлен раньше времени
→ РЕШЕНИЕ: проверить таймер или CC1101 модуль

ОШИБКА #4: furi_hal_subghz_start_async_tx() FAILED!
---
furi_hal_subghz_start_async_tx() FAILED!
Radio may not be properly initialized
→ ПРОБЛЕМА: Радиомодуль не инициализирован правильно
→ РЕШЕНИЕ: перезагрузить Flipper, проверить аппаратуру
```

---

## 🎯 Критические значения для проверки

```
ЧАСТОТА (FREQUENCY):
  ✓ ПРАВИЛЬНО: 432500000 Hz (432.500 MHz - точно!)
  ✗ НЕПРАВИЛЬНО: 432000000 или другие значения

РАЗМЕР ФРЕЙМА (FRAME SIZE):
  ✓ ПРАВИЛЬНО: 45-100 байт (зависит от данных)
  ✗ НЕПРАВИЛЬНО: 0 (фрейм не создан) или > 255 (слишком большой)

КОЛИЧЕСТВО БИТОВ (TX BITS):
  ✓ ПРАВИЛЬНО: 500-1500 бит (не 2048!)
  ✗ НЕПРАВИЛЬНО: 2048 (буфер переполнен)
  ✗ НЕПРАВИЛЬНО: 0 (NRZI кодирование не произошло)

ПЕРВЫЕ БИТЫ NRZI (first 100 bits):
  ✓ ПРАВИЛЬНО: 0101010101... (много чередующихся  0 и 1)
  ✗ НЕПРАВИЛЬНО: 11111111... или 00000000... (нет переходов)

ПЕРЕДАННЫЕ БИТЫ (BITS SENT):
  ✓ ПРАВИЛЬНО: 987 / 987 (совпадают!)
  ✗ НЕПРАВИЛЬНО: 500 / 987 (не все биты отправлены)
```

---

## 📋 Трубка отладки (Debug Pipeline)

Если хотите видеть ВЕСЬ поток данных:

```c
// 1. Данные входят → aprs_refresh_frame()
//    ↓ Кодируются в AX.25 frame (frame_buffer) 
{
    FURI_LOG_I(TAG, "frame_buffer HEX: %s", app->last_tx_hex);
}

// 2. AX.25 → aprs_ax25_build_nrzi_stream()
//    ↓ Преобразуются в NRZI биты (tx_levels)
{
    FURI_LOG_I(TAG, "NRZI bits (first 100): %s", bits_buffer);
}

// 3. NRZI биты → aprs_radio_start_tx() → furi_hal_subghz_start_async_tx()
//    ↓ Отправляются через callback
{
    FURI_LOG_I(TAG, "TX started: %lu bits to send", app->tx_bit_count);
}

// 4. TX callback вызывается для каждого бита
//    ↓ Создает LevelDuration для CC1101
{
    if(app->tx_level_index % 500 == 0) {
        FURI_LOG_D(TAG, "TX progress: %lu / %lu bits", 
                   app->tx_level_index, app->tx_level_count);
    }
}

// 5. TX завершается → aprs_radio_poll()
//    ↓ Проверяет, все ли биты отправлены
{
    FURI_LOG_I(TAG, "TX complete: %lu / %lu bits sent", 
               app->tx_level_index, app->tx_level_count);
}
```

---

## 🧪 Минимальное тестирование

Если хотите только БАЗОВУЮ диагностику, добавьте эти 3 логирования:

```c
// В aprs_refresh_frame(), перед return true:
FURI_LOG_I(TAG, "FRAME_DEBUG: size=%u bytes, bits=%lu, preamble=%u, postamble=%u",
           (unsigned int)app->frame_size, app->tx_bit_count, 
           APRS_PREAMBLE_FLAGS, APRS_POSTAMBLE_FLAGS);

// В aprs_radio_start_tx(), после furi_hal_subghz_start_async_tx():
FURI_LOG_I(TAG, "TX_DEBUG: freq=%lu Hz, bits=%lu, duration=%lu ms",
           (unsigned long)app->frequency_hz, app->tx_bit_count,
           (unsigned long)(app->tx_bit_count * APRS_GFSK_BIT_DURATION_US / 1000));

// В aprs_radio_poll(), после furi_hal_subghz_is_async_tx_complete():
FURI_LOG_I(TAG, "TX_DONE: sent=%lu of %lu bits, %s",
           (unsigned long)app->tx_level_index, (unsigned long)app->tx_level_count,
           (app->tx_level_index == app->tx_level_count) ? "OK" : "INCOMPLETE!");
```

---

## 📞 SOS: Если ничего не работает

1. **Проверьте COM порт:**
   ```powershell
   # Windows 10/11 Device Manager → COM Ports
   # Должен быть COM4 или похожий
   # Убедитесь, что он не используется другими приложениями
   ```

2. **Перезагрузите Flipper:**
   ```
   На Flipper: Main menu → Settings → Power → Power off → включите снова
   ```

3. **Переустановите ufbt SDK:**
   ```powershell
   ufbt update
   ```

4. **Проверьте версию прошивки:**
   ```
   На Flipper: Main menu → Settings → About
   Убедитесь, что версия совместима с ufbt
   ```

---

**Удачи в диагностике! Все проблемы транспортируются на свет с помощью логирования.** 🎯
