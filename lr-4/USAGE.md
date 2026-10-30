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

invariant_violations=0
max_active_readers=0
write-only exit=0
invariant_violations=0
max_active_readers=2
read-only exit=0
invariant_violations=0
max_active_readers=0
rounding exit=0

experiment reader_done writer_done max_active_readers violations timeouts status
---------- ----------- ----------- ------------------ ---------- -------- ------
only_write 0           10          0                  0          0        ok
only_read  10          0           2                  0          0        ok
rounding   0           1           0                  0          0        ok


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

PS D:\osasp\osaps-course> & $exe --operations 0 --read-percent 50 --read-ms 0 --seed 11864 --experiment invalid --results "$testDir/invalid.csv"
[ARGUMENT ERROR] operations must be 1..1000000, read-percent 0..100, read-ms 0..10000
PS D:\osasp\osaps-course> "invalid operations exit=$LASTEXITCODE"
>>
invalid operations exit=2
PS D:\osasp\osaps-course> & $exe --operations 10 --read-percent 101 --read-ms 0 --seed 11864 --experiment invalid --results "$testDir/invalid.csv"
>>
[ARGUMENT ERROR] operations must be 1..1000000, read-percent 0..100, read-ms 0..10000
PS D:\osasp\osaps-course> "invalid percent exit=$LASTEXITCODE"
>>
invalid percent exit=2
PS D:\osasp\osaps-course> & $exe --operations 10 --read-percent 50 --read-ms 0 --seed 11864 --experiment invalid --results "$testDir/missing-folder/result.csv" --quiet
>>
invariant_violations=0
max_active_readers=1
cannot open results file: D:\osasp\osaps-course\lr-4\test-results-20261113-194706/missing-folder/result.csv
PS D:\osasp\osaps-course> "invalid path exit=$LASTEXITCODE"
>>
invalid path exit=1

Первые две команды должны завершиться с кодом 2, третья — с кодом 1: каталог `missing-folder` не существует. При ошибке открытия CSV результаты в нём не сохраняются.

## Превышение времени эксперимента

```powershell
& $exe --operations 100 --read-percent 100 --read-ms 1000 --seed 11864 --experiment timeout --results "$testDir/timeout.csv" --quiet
"timeout exit=$LASTEXITCODE"
Get-Content "$testDir/timeout.csv"
```

PS D:\osasp\osaps-course> & $exe --operations 100 --read-percent 100 --read-ms 1000 --seed 11864 --experiment timeout --results "$testDir/timeout.csv" --quiet
experiment timed out
worker exited with code 1
worker exited with code 1
invariant_violations=0
max_active_readers=2
PS D:\osasp\osaps-course> "timeout exit=$LASTEXITCODE"
>>
timeout exit=1
PS D:\osasp\osaps-course> Get-Content "$testDir/timeout.csv"
>>
experiment,operations,read_percent,read_ms,seed,total_ms,reader_done,writer_done,reader_wait_sum_ms,reader_wait_avg_ms,reader_wait_max_ms,writer_wait_sum_ms,writer_wait_avg_ms,writer_wait_max_ms,max_active_readers,violations,timeouts,status
timeout,100,100,1000,11864,30025.171100,58,0,0.134900,0.002326,0.004400,0.000000,,0.000000,2,0,1,error

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

invariant_violations=0
max_active_readers=2
percent=20 run=1 exit=0
invariant_violations=0
max_active_readers=2
percent=20 run=2 exit=0
invariant_violations=0
max_active_readers=2
percent=20 run=3 exit=0
invariant_violations=0
max_active_readers=2
percent=30 run=1 exit=0
invariant_violations=0
max_active_readers=2
percent=30 run=2 exit=0
invariant_violations=0
max_active_readers=2
percent=30 run=3 exit=0
invariant_violations=0
max_active_readers=2
percent=40 run=1 exit=0
invariant_violations=0
max_active_readers=2
percent=40 run=2 exit=0
invariant_violations=0
max_active_readers=2
percent=40 run=3 exit=0
invariant_violations=0
max_active_readers=2
percent=50 run=1 exit=0
invariant_violations=0
max_active_readers=2
percent=50 run=2 exit=0
invariant_violations=0
max_active_readers=2
percent=50 run=3 exit=0
invariant_violations=0
max_active_readers=2
percent=75 run=1 exit=0
invariant_violations=0
max_active_readers=2
percent=75 run=2 exit=0
invariant_violations=0
max_active_readers=2
percent=75 run=3 exit=0
invariant_violations=0
max_active_readers=2
percent=90 run=1 exit=0
invariant_violations=0
max_active_readers=2
percent=90 run=2 exit=0
invariant_violations=0
max_active_readers=2
percent=90 run=3 exit=0

read_percent total_ms    reader_wait_avg_ms writer_wait_avg_ms violations timeouts status
------------ --------    ------------------ ------------------ ---------- -------- ------
20           1040.956000 0.075411           0.366868           0          0        ok
20           1077.399000 0.095490           0.424923           0          0        ok
20           1079.555600 0.262099           2.037247           0          0        ok
30           1608.026800 0.109213           0.206144           0          0        ok
30           1632.536000 0.330208           3.515595           0          0        ok
30           1624.858800 0.281376           3.477772           0          0        ok
40           2122.014500 0.028061           0.208190           0          0        ok
40           2114.071200 0.063531           1.504209           0          0        ok
40           2122.512700 0.153719           1.454294           0          0        ok
50           2662.534600 0.175970           7.999629           0          0        ok
50           2627.159000 0.020362           0.521276           0          0        ok
50           2640.123900 0.018207           0.145115           0          0        ok
75           3926.273200 0.013376           0.689145           0          0        ok
75           3968.765900 0.049720           1.081445           0          0        ok
75           3933.475700 0.007913           0.285547           0          0        ok
90           4719.534000 0.033960           0.715768           0          0        ok
90           4708.377000 0.011904           2.976161           0          0        ok
90           4589.188800 0.011716           4.782803           0          0        ok

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
invariant_violations=0
max_active_readers=2
read-ms=0 run=1 exit=0
invariant_violations=0
max_active_readers=1
read-ms=0 run=2 exit=0
invariant_violations=0
max_active_readers=1
read-ms=0 run=3 exit=0
invariant_violations=0
max_active_readers=2
read-ms=1 run=1 exit=0
invariant_violations=0
max_active_readers=2
read-ms=1 run=2 exit=0
invariant_violations=0
max_active_readers=2
read-ms=1 run=3 exit=0
invariant_violations=0
max_active_readers=2
read-ms=3 run=1 exit=0
invariant_violations=0
max_active_readers=2
read-ms=3 run=2 exit=0
invariant_violations=0
max_active_readers=2
read-ms=3 run=3 exit=0
invariant_violations=0
max_active_readers=2
read-ms=5 run=1 exit=0
invariant_violations=0
max_active_readers=2
read-ms=5 run=2 exit=0
invariant_violations=0
max_active_readers=2
read-ms=5 run=3 exit=0
invariant_violations=0
max_active_readers=2
read-ms=7 run=1 exit=0
invariant_violations=0
max_active_readers=2
read-ms=7 run=2 exit=0
invariant_violations=0
max_active_readers=2
read-ms=7 run=3 exit=0
invariant_violations=0
max_active_readers=2
read-ms=10 run=1 exit=0
invariant_violations=0
max_active_readers=2
read-ms=10 run=2 exit=0
invariant_violations=0
max_active_readers=2
read-ms=10 run=3 exit=0
PS D:\osasp\osaps-course> Import-Csv "$testDir/read-duration.csv" | Format-Table read_ms,total_ms,writer_wait_avg_ms,violations,timeouts,status
>>

read_ms total_ms    writer_wait_avg_ms violations timeouts status
------- --------    ------------------ ---------- -------- ------
0       24.981200   0.009608           0          0        ok
0       24.716200   0.000598           0          0        ok
0       24.072200   0.000642           0          0        ok
1       2593.582800 0.579853           0          0        ok
0       32.457000   0.015316           0          0        ok
0       23.061500   0.001339           0          0        ok
0       26.253000   0.000532           0          0        ok
1       2667.381800 0.906432           0          0        ok
1       2677.743700 7.988314           0          0        ok
1       2661.091200 0.874666           0          0        ok
3       2647.800200 0.309202           0          0        ok
3       2617.228200 0.143504           0          0        ok
3       2637.146000 1.087072           0          0        ok
5       2642.107400 0.986965           0          0        ok
5       2602.044400 0.890725           0          0        ok
5       2572.260000 0.425687           0          0        ok
7       2587.945300 0.666001           0          0        ok
7       2630.006000 3.886719           0          0        ok
7       2687.305500 7.914146           0          0        ok
10      2662.256000 0.810481           0          0        ok
10      2695.053100 0.046803           0          0        ok
10      2683.814900 7.964752           0          0        ok

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

foreach ($run in 1..3) {
    & $exe --operations 100 --read-percent 50 --read-ms 1 --seed 1000 --experiment standard --results "$testDir/standard.csv" --quiet
    "standard run=$run exit=$LASTEXITCODE"
}
Import-Csv "$testDir/standard.csv" | Format-Table total_ms,reader_done,writer_done,violations,timeouts,status


## Что сохранить и прислать

Сохраните полный вывод сборки и проверок, все CSV из `$testDir`, версию Windows, модель процессора, объём ОЗУ, версию компилятора и CMake. Вывести путь к результатам:

```powershell
$testDir
cmake --version
```

Ожидание в CSV измеряется от начала получения доступа до его получения, в миллисекундах; средние считаются по выполненным операциям соответствующей роли. `total_ms` включает запуск процессов, подключение, операции, ожидание их завершения и небольшие затраты управляющего процесса на проверку завершения. Подробный вывод при измерениях отключён. Разные процессы могут завершаться в разное время: заданный процент — доля операций за весь запуск.

Вывод `max_active_readers=2` и отсутствие нарушений полезны для проверки, но сами по себе не доказывают корректность во всех возможных расписаниях. Данные нужны реальные; строки с ошибкой не усредняем.
