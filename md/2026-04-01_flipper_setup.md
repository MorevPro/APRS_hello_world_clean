# Настройка среды Flipper Zero

Дата: 2026-04-01

## Что было сделано

1. Проверен проект: это external app для Flipper Zero с манифестом `application.fam`.
2. Установлен официальный `ufbt` для Windows:
   - команда: `py -m pip install --upgrade ufbt`
   - версия: `0.2.6`
3. Создан локальный файл окружения `.env`:
   - `UFBT_HOME=.ufbt`
4. Скачаны SDK и toolchain Flipper Zero в локальное окружение проекта.
5. Сгенерированы файлы VS Code через `ufbt vscode_dist`:
   - `.vscode/tasks.json`
   - `.vscode/launch.json`
   - `.vscode/settings.json`
   - `.vscode/extensions.json`
   - `.vscode/c_cpp_properties.json`
   - `.vscode/compile_commands.json`
6. Исправлены несовместимости исходника с текущим SDK:
   - `taskENTER_CRITICAL/taskEXIT_CRITICAL` заменены на `FURI_CRITICAL_ENTER/FURI_CRITICAL_EXIT`
   - сигнатура `timer_callback` приведена к `void* context`
7. Выполнена успешная сборка приложения.

## Результат сборки

- Основной бинарник: `dist/aprs_hello_world_clean.fap`
- Debug ELF: `dist/debug/aprs_hello_world_clean_d.elf`

## Что уже работает в VS Code

- Сборка: задача `Build`
- Загрузка VS Code-конфигурации: задача `Update VSCode config`
- Подготовка к отладке через сгенерированный `launch.json`

## Что пока не готово

Автозапуск приложения на устройство через `ufbt launch` пока не проходит, потому что Windows в текущий момент не видит Flipper как доступное устройство для `ufbt`.

Проверка показала:

- в системе виден только `COM1`
- `ufbt launch` завершился ошибкой `Failed to find connected Flipper`

## Что сделать дальше

1. Проверить, что Flipper разблокирован и включен.
2. Установить/запустить Flipper Desktop, чтобы Windows подтянула корректные USB-драйверы.
3. Переподключить устройство другим USB-C кабелем, если текущий только для зарядки.
4. После появления устройства повторить в терминале проекта:
   - `ufbt launch`
5. Если нужна именно пошаговая отладка через debugger:
   - потребуется совместимая прошивка из SDK
   - для полноценного low-level debug обычно нужен SWD-пробник

## Полезные команды

- Сборка: `ufbt`
- Запуск на Flipper: `ufbt launch`
- CLI-сессия с устройством: `ufbt cli`
- Обновление SDK: `ufbt update`
- Перегенерация VS Code-конфигов: `ufbt vscode_dist`

## Обновление статуса

Повторная проверка после настройки драйвера выполнена успешно.

- Windows видит Flipper как `COM3`
- `ufbt launch` успешно:
  - обнаружил устройство
  - загрузил `.fap` на Flipper
  - запустил приложение

Подтверждено, что среда разработки готова для:

- сборки приложения
- загрузки приложения на устройство
- запуска из VS Code задач `Build` и `Launch App on Flipper`

Также обновлен `README.md`:

- добавлены инструкции на русском и английском языках
- добавлены пошаговые сценарии для Windows и Linux
- отражен успешный статус запуска приложения на устройстве
