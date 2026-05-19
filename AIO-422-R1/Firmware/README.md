# AIO-422-R1 — руководство разработчика прошивки

Документ описывает, как на **новом компьютере** установить среду, открыть исходный код, доработать прошивку, собрать и загрузить её в модуль **AIO-422-R1** (MCU **RP2350**).

---

## 1. Что входит в проект

| Путь | Назначение |
|------|------------|
| `default_aio_422_r1/default_aio_422_r1.ino` | Основная прошивка (Arduino) |
| `ConfigToolPage.html` | Web-конфигуратор по USB (Web Serial) |
| `default_aio_422_r1_plc/*.yaml` | Профили ESPHome для MiniPLC (не требуют Arduino IDE) |

**Аппаратура модуля:** 4× AI (ADS1115), 2× AO (MCP4725), 2× RTD (MAX31865), 4 кнопки, 4 LED, Modbus RTU по RS-485, USB Type-C.

---

## 2. Требования

### 2.1. Оборудование

- Модуль **AIO-422-R1** с MCU **RP2350A**
- Кабель **USB Type-C** (данные, не только зарядка)
- ПК: Windows 10/11, Linux (в т.ч. Fedora/Ubuntu) или macOS
- Для полевой работы по Modbus: питание **24 V DC**, линия RS-485 (не обязательно для сборки/прошивки по USB)

### 2.2. Программное обеспечение

- **Arduino IDE 2.3+** (рекомендуется): https://www.arduino.cc/en/software  
- Доступ в интернет (установка ядра плат и библиотек)
- **Git** (чтобы клонировать репозиторий): https://git-scm.com/

---

## 3. Получение исходного кода

```bash
git clone https://github.com/isystemsautomation/homemaster-dev.git
cd homemaster-dev/AIO-422-R1/Firmware
```

Либо скачайте ZIP архива репозитория с GitHub и распакуйте.

Скетч для Arduino IDE:

```text
homemaster-dev/AIO-422-R1/Firmware/default_aio_422_r1/default_aio_422_r1.ino
```

Открывайте именно файл **`.ino`** — Arduino IDE подхватит папку скетча целиком.

---

## 4. Установка Arduino IDE

### Windows

1. Скачайте установщик с https://www.arduino.cc/en/software  
2. Установите с правами администратора при необходимости  
3. При первом подключении модуля Windows может установить драйвер USB (CDC) автоматически  

### Linux (Fedora и аналоги)

**Рекомендуется AppImage** с сайта Arduino (полный доступ к USB):

1. Скачайте AppImage с https://www.arduino.cc/en/software  
2. Сделайте исполняемым: `chmod +x arduino-ide_*.AppImage`  
3. Добавьте пользователя в группу serial:

   ```bash
   sudo usermod -aG dialout $USER
   ```

4. Выйдите из сессии и войдите снова  

**Flatpak** (`cc.arduino.IDE2`): часто **не видит COM-порт**. Если используете Flatpak:

```bash
flatpak override --user cc.arduino.IDE2 --device=all
```

### macOS

Установите Arduino IDE с официального сайта; при запросе разрешите доступ к USB.

---

## 5. Поддержка платы RP2350 (пакет плат)

Прошивка собирается ядром **Earle Philhower** (arduino-pico), а не «голым» Mbed без RP2350.

### 5.1. URL менеджера плат

1. **File → Preferences** (Файл → Настройки)  
2. Поле **Additional boards manager URLs** — добавьте (если пусто — вставьте одной строкой):

   ```text
   https://arduino.earlephilhower.com/version/stable/package_earlephilhower_index.json
   ```

3. **OK**

### 5.2. Установка ядра

1. **Tools → Board → Boards Manager** (Инструменты → Плата → Менеджер плат)  
2. В поиске: `pico` или `rp2350`  
3. Установите пакет:

   **Raspberry Pi Pico/RP2040/RP2350**  
   автор: **Earle Philhower**

Дождитесь окончания загрузки (несколько сотен МБ).

### 5.3. Выбор платы и параметров

**Tools → Board** → группа **Raspberry Pi Pico/RP2040/RP2350**:

- **Raspberry Pi Pico 2**, или  
- **Generic RP2350** / вариант с **rp2350** в имени  

(точное название зависит от версии ядра; главное — **RP2350**, не только RP2040/Pico 1).

| Параметр (Tools) | Рекомендация |
|------------------|--------------|
| **Port** | После подключения USB: `COMx` (Windows), `/dev/ttyACM0` (Linux) |
| Остальные пункты | По умолчанию для выбранной платы |

**LittleFS** и **watchdog** входят в ядро Philhower — отдельно не устанавливаются.

---

## 6. Библиотеки Arduino

**Sketch → Include Library → Manage Libraries…** (Эскиз → Подключить библиотеку → Управлять библиотеками…)

Установите по очереди:

| № | Искать в Library Manager | Заголовок в коде | Назначение |
|---|--------------------------|------------------|------------|
| 1 | **ADS1X15** (Rob Tillaart) | `ADS1X15.h` | Аналоговые входы, ADS1115 |
| 2 | **Adafruit MCP4725** | `Adafruit_MCP4725.h` | Аналоговые выходы, DAC |
| 3 | **Adafruit MAX31865** | `Adafruit_MAX31865.h` | RTD, MAX31865 |
| 4 | **Adafruit BusIO** | — | Зависимость Adafruit (часто ставится автоматически) |
| 5 | **Modbus Serial** (epsilonrt) | `ModbusSerial.h` | Modbus RTU slave |
| 6 | **SimpleWebSerial** | `SimpleWebSerial.h` | USB WebConfig |
| 7 | **Arduino_JSON** | `Arduino_JSON.h` | JSON для WebSerial |

### 6.1. Modbus — важно

Используется библиотека **epsilonrt** с файлом **`ModbusSerial.h`**.

**Не устанавливайте** библиотеку **`Modbus`** от автора **UL DARA** — другой API, на RP2040/RP2350 часто **ошибка сборки** (`Modbus.h not found`, конфликт типа `byte`).

Если в Library Manager нет Modbus Serial, установите вручную:

```bash
mkdir -p ~/Arduino/libraries
cd ~/Arduino/libraries
git clone https://github.com/epsilonrt/modbus-arduino.git Modbus-Arduino
```

Перезапустите Arduino IDE.

### 6.2. Порядок подключения заголовков

В `default_aio_422_r1.ino` уже задан верный порядок:

```cpp
#include <Arduino.h>
#include <ModbusSerial.h>   // обязательно ДО Adafruit
#include <Wire.h>
// ...
```

Не добавляйте `#include <utility>` — на RP2350 возможен конфликт `byte` с Modbus.

---

## 7. Открытие проекта и компиляция

1. **File → Open** → выберите  
   `default_aio_422_r1/default_aio_422_r1.ino`
2. Проверьте **Tools → Board** (RP2350) и **Tools → Port** (если модуль подключён)
3. Нажмите **Verify** (галочка) или **Sketch → Verify/Compile**

Ожидаемый результат: **компиляция без ошибок**, размер прошивки порядка **150–165 KB** (зависит от версии ядра).

### Типичные ошибки сборки

| Сообщение | Решение |
|-----------|---------|
| `Modbus.h: No such file` | Установить **epsilonrt** Modbus; удалить Modbus UL DARA из `~/Arduino/libraries` |
| `byte` ambiguous | `ModbusSerial.h` перед Adafruit; убрать `<utility>` |
| `PersistConfig was not declared` | Не переносить `struct PersistConfig` в конец файла — должна быть в начале `.ino` |
| Плата не RP2350 | Установить ядро Philhower (раздел 5) |

---

## 8. Загрузка прошивки (Upload)

### 8.1. Через USB (обычный способ)

1. Подключите модуль по USB-C к ПК  
2. **Tools → Port** → выберите порт (`COM…` / `ttyACM0`)  
3. Нажмите **Upload** (стрелка вправо)  
4. При необходимости на модуле кратко появится режим загрузчика — не отключайте кабель  

После загрузки в Serial Monitor (115200 baud) или WebConfig должно появиться сообщение о успешной загрузке (Boot OK).

### 8.2. Через UF2 (если порта нет)

1. Отключите USB  
2. Зажмите кнопку **BOOTSEL** на плате MCU, подключите USB, отпустите BOOTSEL  
3. В системе появится диск **RPI-RP2**  
4. Скопируйте файл **`.uf2`** из папки сборки Arduino (после Compile) на диск RPI-RP2  
5. Модуль перезагрузится с новой прошивкой  

Путь к `.uf2` после сборки (пример Linux):

```text
/tmp/arduino_build_*/default_aio_422_r1.ino.uf2
```

или смотрите вывод компиляции / папку `build` рядом со скетчем, если включена детальная сборка.

### 8.3. Сброс конфигурации на модуле

Прошивка хранит настройки во flash (**LittleFS**, файл `/cfg.bin`). При смене **версии формата** конфигурации выполняется сброс на заводские значения. После первой загрузки новой версии может потребоваться заново задать Modbus-адрес и привязки кнопок/LED через WebConfig.

---

## 9. WebConfig (настройка без пересборки)

1. Откройте в браузере Chrome или Edge файл  
   `ConfigToolPage.html` (двойной клик или drag-and-drop в браузер)  
2. Подключите модуль по USB  
3. Нажмите **Connect** → выберите COM-порт модуля  
4. Доступны: Modbus address/baud, AI/AO/RTD (диагностика), кнопки, LED, RTD-конфиг  

Modbus по RS-485 работает параллельно (по умолчанию адрес **3**, скорость **19200**).

---

## 10. ESPHome / MiniPLC (отдельно от Arduino)

Файлы в `default_aio_422_r1_plc/` — для интеграции через **ESPHome**, не для Arduino IDE:

| Файл | Описание |
|------|----------|
| `default_aio_422_r1_plc.yaml` | Базовый набор: AI, AO, RTD |
| `default_aio_422_r1_plc_full.yaml` | + кнопки и LED (Modbus discrete) |

Проверка YAML (на ПК с установленным ESPHome):

```bash
esphome config default_aio_422_r1_plc.yaml
```

---

## 11. Чеклист «новый компьютер»

```
[ ] Git: клонирован репозиторий homemaster-dev
[ ] Установлен Arduino IDE 2.x
[ ] Linux: пользователь в группе dialout; при Flatpak — device=all
[ ] Preferences: URL earlephilhower package_earlephilhower_index.json
[ ] Boards Manager: Raspberry Pi Pico/RP2040/RP2350 (Earle Philhower)
[ ] Board: RP2350 / Pico 2
[ ] Библиотеки: ADS1X15, Adafruit MCP4725, Adafruit MAX31865, Adafruit BusIO,
                 Modbus Serial (epsilonrt), SimpleWebSerial, Arduino_JSON
[ ] НЕТ библиотеки Modbus (UL DARA)
[ ] Открыт default_aio_422_r1.ino
[ ] Verify — без ошибок
[ ] Upload или UF2 — успешно
[ ] ConfigToolPage.html — Connect по USB работает
```

---

## 12. Структура Modbus (справка)

| Регистры | Адреса | Описание |
|----------|--------|----------|
| Кнопки | ISTS 1–4 | Discrete inputs |
| LED | ISTS 20–23 | Discrete inputs |
| RTD | HREG 120–121 | °C×10, S_WORD |
| AI mV | HREG 140–143 | U_WORD |
| AO raw | HREG 200–201 | U_WORD, 0–4095 |

Подробнее — комментарии в начале `default_aio_422_r1.ino` и YAML для ESPHome.

---

## 13. Поддержка

- Репозиторий: https://github.com/isystemsautomation/homemaster-dev  
- Производитель: ISYSTEMS AUTOMATION S.R.L. (HomeMaster®)  
- Сайт: https://www.home-master.eu  

При ошибках сборки приложите **полный текст из окна Output** Arduino IDE и версии: IDE, ядра Philhower, ОС.
