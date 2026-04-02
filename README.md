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



### Логи и проверка

- Сборка и запуск: `ufbt launch` (нужен Flipper по USB; порт можно задать через `FLIP_PORT`, например `set FLIP_PORT=COM4` в Windows).
- Просмотр логов: `ufbt cli` — интерактивная сессия с Flipper по USB; дальше зависит от версии прошивки (например, команды потока логов в CLI). В приложении используется тег `APRS432`: при успешных шагах жеста — `Settings: Up Short step 1/3`, при открытии меню — `Open settings (triple Up Short)`.

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

### Why the menu did not open (fixed in code)

- The main event loop only forwarded `InputTypePress` to `aprs_handle_input`, so **`InputTypeShort` never reached** the handler and the triple-Up gesture did not run. The main loop now also forwards `InputTypeShort` for **Up** on the main screen.
- Triple-tap detection uses **Short** (complete short click), not raw `Press` edges.

### Logs

- Build and run: `ufbt launch` (Flipper on USB; optional `FLIP_PORT` such as `COM4` on Windows).
- Logs: `ufbt cli` for an interactive USB session; log commands depend on firmware. This app logs with tag `APRS432`, e.g. `Settings: Up Short step 1/3` and `Open settings (triple Up Short)`.

### Debugging

- VS Code debugging files are generated by `ufbt vscode_dist`
- Debug configurations are stored in `.vscode/launch.json`
- Full low-level debugging usually requires an SWD probe such as ST-Link, CMSIS-DAP, J-Link, or Black Magic Probe
