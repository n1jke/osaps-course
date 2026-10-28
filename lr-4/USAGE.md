# Лабораторная 4: сборка и проверка

Команды рассчитаны на Windows и PowerShell, запущенный из корня репозитория. Нужны CMake и Visual Studio 2022 с компонентом «Разработка классических приложений на C++». Исходники лабораторной должны быть доступны на Windows. Запускать можно из обычного PowerShell.

## Сборка

```powershell
cmake -S lr-4 -B lr-4/build -G "Visual Studio 17 2022" -A x64
cmake --build lr-4/build --config Release
$exe = (Resolve-Path "lr-4/build/Release/lab4.exe").Path
$testDir = Join-Path (Get-Location) ("lr-4/test-results-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
New-Item -ItemType Directory -Path $testDir | Out-Null
```

Если сборка завершилась с ошибкой, остановитесь и сохраните её полный вывод. Каждая новая серия команд создаёт отдельный каталог результатов; файлы CSV внутри него дополняются, а не перезаписываются. Все команды ниже используют переменные `$exe` и `$testDir` из этого блока.

## Наглядная проверка

```powershell
& $exe --operations 20 --read-percent 80 --read-ms 50 --seed 11864 --experiment demo --results "$testDir/demo.csv"
"exit=$LASTEXITCODE"
Get-Content "$testDir/demo.csv"
```

Ожидается завершение с кодом 0, `invariant_violations=0`, `status=ok`, 16 чтений и 4 записи. Состояния `WAITING`, `READING`, `WRITING`, `FINISHED` сопровождаются номером участника и PID. Вывод разных процессов может перемешиваться. `max_active_readers=2` показывает, что в этом запуске чтение двух процессов пересеклось; если значение 1, повторите запуск: это зависит от планирования процессов.

## Граничные случаи

```powershell
& $exe --operations 10 --read-percent 0 --read-ms 0 --seed 11864 --experiment only_write --results "$testDir/boundary.csv" --quiet
"write-only exit=$LASTEXITCODE"
& $exe --operations 10 --read-percent 100 --read-ms 1 --seed 11864 --experiment only_read --results "$testDir/boundary.csv" --quiet
"read-only exit=$LASTEXITCODE"
& $exe --operations 1 --read-percent 50 --read-ms 0 --seed 11864 --experiment rounding --results "$testDir/boundary.csv" --quiet
"rounding exit=$LASTEXITCODE"
Import-Csv "$testDir/boundary.csv" | Format-Table experiment,reader_done,writer_done,max_active_readers,violations,timeouts,status
```

Ожидаются коды 0 и статусы `ok`. При 0% должно быть 0 чтений и 10 записей; при 100% — 10 чтений и 0 записей. Для одной операции и 50% округление вниз даёт 0 чтений и 1 запись. Среднее ожидание роли без операций в CSV пустое.

## Ошибки аргументов и файла

```powershell
& $exe --operations 0 --read-percent 50 --read-ms 0 --seed 11864 --experiment invalid --results "$testDir/invalid.csv"
"invalid operations exit=$LASTEXITCODE"
& $exe --operations 10 --read-percent 101 --read-ms 0 --seed 11864 --experiment invalid --results "$testDir/invalid.csv"
"invalid percent exit=$LASTEXITCODE"
& $exe --operations 10 --read-percent 50 --read-ms 0 --seed 11864 --experiment invalid --results "$testDir/missing-folder/result.csv" --quiet
"invalid path exit=$LASTEXITCODE"
```

Первые две команды должны завершиться с кодом 2, третья — с кодом 1: каталог `missing-folder` не существует. При ошибке открытия CSV результаты в нём не сохраняются.

## Превышение времени эксперимента

```powershell
& $exe --operations 100 --read-percent 100 --read-ms 1000 --seed 11864 --experiment timeout --results "$testDir/timeout.csv" --quiet
"timeout exit=$LASTEXITCODE"
Get-Content "$testDir/timeout.csv"
```

Два читателя получают по 50 операций с секундной задержкой; общий предел ожидания — 30 секунд после запуска процессов. Ожидаются сообщение о превышении времени, код 1 и строка со статусом `error` и ненулевым `timeouts`. Такой запуск не включается в графики. При аварийном завершении программа прерывает оставшихся работников; восстановление сессии не выполняется.

## Эксперимент 1: доля чтений

При одинаковом количестве операций и длительности чтения меняем долю чтения. Каждую конфигурацию запускаем три раза; индивидуальные параметры — 660 операций и seed 11864.

```powershell
foreach ($percent in @(20, 30, 40, 50, 75, 90)) {
    foreach ($run in 1..3) {
        & $exe --operations 660 --read-percent $percent --read-ms 1 --seed 11864 --experiment ratio --results "$testDir/ratio.csv" --quiet
        "percent=$percent run=$run exit=$LASTEXITCODE"
    }
}
Import-Csv "$testDir/ratio.csv" | Format-Table read_percent,total_ms,reader_wait_avg_ms,writer_wait_avg_ms,violations,timeouts,status
```

Для графика X — `read_percent`, Y — среднее ожидание читателей и писателей по трём успешным запускам. Если есть ненулевой код, нарушение или `status=error`, сохраните данные и сообщите об этом перед анализом.

## Эксперимент 2: длительность чтения

```powershell
foreach ($duration in @(0, 1, 3, 5, 7, 10)) {
    foreach ($run in 1..3) {
        & $exe --operations 660 --read-percent 50 --read-ms $duration --seed 11864 --experiment read_duration --results "$testDir/read-duration.csv" --quiet
        "read-ms=$duration run=$run exit=$LASTEXITCODE"
    }
}
Import-Csv "$testDir/read-duration.csv" | Format-Table read_ms,total_ms,writer_wait_avg_ms,violations,timeouts,status
```

Для графика X — `read_ms`, Y — среднее `total_ms` по трём успешным запускам. `Sleep` удерживает доступ к таблице на время задержки; фактическое время зависит от планировщика Windows.

## Стандартные параметры

Кроме индивидуальных параметров задание требует стандартный запуск. Ниже базовое число операций — 100; для сравнения повторяем его три раза.

```powershell
foreach ($run in 1..3) {
    & $exe --operations 100 --read-percent 50 --read-ms 1 --seed 1000 --experiment standard --results "$testDir/standard.csv" --quiet
    "standard run=$run exit=$LASTEXITCODE"
}
Import-Csv "$testDir/standard.csv" | Format-Table total_ms,reader_done,writer_done,violations,timeouts,status
```

## Что сохранить и прислать

Сохраните полный вывод сборки и проверок, все CSV из `$testDir`, версию Windows, модель процессора, объём ОЗУ, версию компилятора и CMake. Вывести путь к результатам:

```powershell
$testDir
cmake --version
```

Ожидание в CSV измеряется от начала получения доступа до его получения, в миллисекундах; средние считаются по выполненным операциям соответствующей роли. `total_ms` включает запуск процессов, подключение, операции, ожидание их завершения и небольшие затраты управляющего процесса на проверку завершения. Подробный вывод при измерениях отключён. Разные процессы могут завершаться в разное время: заданный процент — доля операций за весь запуск.

Вывод `max_active_readers=2` и отсутствие нарушений полезны для проверки, но сами по себе не доказывают корректность во всех возможных расписаниях. Данные нужны реальные; строки с ошибкой не усредняем.
