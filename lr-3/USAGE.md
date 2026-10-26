# ЛР №3: краткий ранбук

Windows, PowerShell, CMake 3.16+ и компилятор C++17 с Windows SDK. Во всех терминалах рабочий каталог — `lr-3`. В `src/config.h` установлен STUDENT_ID=112.

## Сборка

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Пути ниже — для Visual Studio; для одноконфигурационного генератора задайте `-DCMAKE_BUILD_TYPE=Release` и используйте `build\lr3.exe`.

## Запуск и тестирование

Каждая команда запуска — в отдельном терминале. Сначала сервер, после READY — три клиента:

```powershell
# Терминал 1
.\build\Release\lr3.exe server --run-id 381 --log run381.log --timeout-ms 60000
# Терминал 2
.\build\Release\lr3.exe client --run-id 381 --client-id 1 --size 64
# Терминал 3
.\build\Release\lr3.exe client --run-id 381 --client-id 2 --size 64
# Терминал 4
.\build\Release\lr3.exe client --run-id 381 --client-id 3 --size 64
```

Успех сервера: `status=OK`, `received=3336`, `rejected=0`, три `done=yes`, `missing=0`, `duplicates=0`. После завершения проверить:

```powershell
$LASTEXITCODE # сразу после программы: 0 — успех, 1 — ошибка работы, 2 — аргументы
(Get-Content .\run381.log | Measure-Object -Line).Lines # 3336
```

Для каждого нового прогона менять RUN_ID и имя журнала: существующий файл не перезаписывается.

### Сценарий с ошибкой: неизвестный тип сообщения

Повторить запуск в четырёх терминалах:

```powershell
# Терминал 1
.\build\Release\lr3.exe server --run-id 905 --log run905.log --timeout-ms 60000
# Терминал 2: один испорченный пакет, затем обычные LOG и DONE
.\build\Release\lr3.exe client --run-id 905 --client-id 1 --size 64 --fault type
# Терминал 3
.\build\Release\lr3.exe client --run-id 905 --client-id 2 --size 64
# Терминал 4
.\build\Release\lr3.exe client --run-id 905 --client-id 3 --size 64
```

Ожидается: `REJECT ... Unknown message type`, 3336 сохранённых логов, `rejected=1`, `status=INCOMPLETE_OR_INVALID`, `benchmark=invalid`, код сервера 1. Клиенты завершаются с кодом 0: это подтверждает только отправку. Для неверной длины повторить с новым RUN_ID/файлом и `--fault size` вместо `--fault type`.

### Дополнительные проверки

```powershell
# Без сервера 901: код 1, CreateFileA(mailslot), GetLastError=2
.\build\Release\lr3.exe client --run-id 901 --client-id 1
# Номер клиента вне диапазона: код 2
.\build\Release\lr3.exe client --run-id 901 --client-id 4
# Существующий журнал после основного прогона: код 1, GetLastError=80
.\build\Release\lr3.exe server --run-id 902 --log run381.log
# Без клиентов: через 2 с код 1, benchmark=invalid
.\build\Release\lr3.exe server --run-id 903 --log run903.log --timeout-ms 2000
```

Остановка участника: в новом прогоне с размером 4096 нажать Ctrl+C у клиента после SEND, до DONE. Остальные клиенты продолжают, сервер завершается по тайм-ауту с кодом 1. Если DONE уже отправлен, повторить.

## Замеры для отчёта

Повторить основной запуск по три раза для `--size 64` и `--size 4096` у всех клиентов. Сохранить вывод четырёх терминалов, журнал, `elapsed_s`, `messages_per_s` и коды завершения. Посчитать медианы отдельно для каждого размера; ошибочные прогоны исключить.

Паузы ручного запуска, консоль и запись журнала входят в замер. Для проверки одновременной работы нужно успеть запустить все три клиента до завершения первого. Формат и границы таймера — в [RFC](rfc.md); результаты перенести в [отчёт](report.md). Прогоны Windows пока не выполнены.
