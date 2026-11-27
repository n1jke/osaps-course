# ЛР №6. Запуск на Windows

## Сборка

Нужны CMake, Visual Studio 2022 или Build Tools 2022 с MSVC и Windows SDK. В PowerShell из корня репозитория:

```powershell
cmake -S lr-6 -B lr-6/build -G "Visual Studio 17 2022" -A x64
cmake --build lr-6/build --config Release
Set-Location lr-6
```

Параметры запуска — `--hz 10|50|100`, `--seconds 1..300` и `--csv PATH`. По умолчанию используется 10 срабатываний/с, 30 секунд и файл `test_<run_id>.csv` в текущей папке. Для явного пути папка должна существовать, а имя файла быть свободным. Замер начинается по кнопке «Старт», CSV сохраняется автоматически.

## Проверка ошибок

```powershell
.\build\Release\lr6.exe --seconds 0
.\build\Release\lr6.exe --hz 25
.\build\Release\lr6.exe --seconds 2 --csv ".\$([guid]::NewGuid())\test.csv"
```

Первые две команды должны показать синтаксис запуска. После третьей нажми «Старт». При сохранении ожидается сообщение об ошибке пути. Для проверки закрытия во время замера запусти с `--seconds 30`, нажми «Старт» и закрой окно.

## Тестовые прогоны

```powershell
.\build\Release\lr6.exe --hz 10 --seconds 5 --csv ".\auto.csv"
.\build\Release\lr6.exe --hz 50 --seconds 10 --csv ".\manual.csv"
```

- В первом окне нажми «Старт» и дождись сводки. В `auto.csv` ожидаются `status=completed`, `observed_ms=5000` и пять секундных интервалов.
- Во втором окне нажми «Старт», дважды «Тест», подвигай окно и нажми «Стоп» до дедлайна. В `manual.csv` ожидается `status=stopped`. Суммы `count` и `study_timer_count` должны совпасть с итогами окна. Для `message_id=32769` сумма `count` должна быть 20.

Для отчёта на одном ноутбуке запусти каждую команду по три раза. В каждом окне нажми «Старт» и дождись конца замера без действий в окне.

```powershell
.\build\Release\lr6.exe --hz 10 --seconds 30
.\build\Release\lr6.exe --hz 50 --seconds 30
.\build\Release\lr6.exe --hz 100 --seconds 30
```

Для индивидуальной серии запусти ещё три прогона при 50 срабатываниях/с. Нажми «Тест» примерно на 5-й, 10-й и 15-й секундах, затем около пяти секунд перемещай окно. Отметь отзывчивость. Сохрани CSV обеих серий, `lr6.log`, конфигурацию ноутбука и версию Windows. Время выгрузки `elapsed_ms` сопоставляется с CSV по `run_id`.
