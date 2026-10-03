# Лабораторная №2: сборка и запуск

Вариант 26 — файловая база переменных строк. Нужны Windows x64, Visual Studio с **Desktop development with C++** и CMake. Из Developer Command Prompt for VS:

```bat
cd lr-2
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\main.exe --help
mkdir lr-2-data
```

Для другой версии Visual Studio выбрать установленный генератор. На macOS приложение не собирается и не запускается.

## Работа с базой

```bat
build\Release\main.exe create "lr-2-data\example.db"
build\Release\main.exe add "lr-2-data\example.db" "A"
build\Release\main.exe add "lr-2-data\example.db" "Hello"
build\Release\main.exe add "lr-2-data\example.db" ""
build\Release\main.exe list "lr-2-data\example.db"
```

Ожидаются три записи длиной 1, 5 и 0 байт, `count=3`, `file_bytes=78`, `verified=yes`. При добавлении программа выводит новый UUID. Подставить его в поиск:

```bat
build\Release\main.exe find "lr-2-data\example.db" <uuid>
echo %ERRORLEVEL%
```

UUID вводится без угловых скобок. Найденная запись выводится с ID, длиной и текстом; отсутствующая — `Not found`. В обоих случаях есть `elapsed_ms`. Коды: 0 — успех/не найдено, 1 — ошибка выполнения, 2 — неверные аргументы. Проверять код сразу после нужной команды.

Пути и текст — только ASCII, не-ASCII отклоняется. Максимум текста — 1 MiB; ввод также ограничен командной строкой Windows. Пробелы поддерживаются в кавычках. Кавычки, обратные слеши и управляющие байты при просмотре экранируются. `create`/`generate` не перезаписывают существующие файлы.

## Генерация и время

```bat
build\Release\main.exe generate "lr-2-data\small.db" 10 256
build\Release\main.exe generate "lr-2-data\medium.db" 100 256
build\Release\main.exe generate "lr-2-data\large.db" 1000 256
```

Каждая запись получает UUID и случайный ASCII-текст длиной 1..maxLen байт. Генератор выводит UUID первой, средней (индекс count/2 от нуля) и последней записи. Каждая запись после записи перечитывается и сравнивается побайтово; после сохранения выполняется полный обход с проверкой количества и размера. Хеширование не используется. Генерация случайная: разные запуски могут давать разные размеры; для сравнения использовать одну базу и её копии. При ошибке генерации файл может остаться неполным; успех не выводится.

`elapsed_ms`:

- `add`: проверка старой базы, запись и два сброса; генерация UUID, сериализация и финальное перечитывание исключены.
- `find`: полный обход с проверкой структуры, включая участок после найденной записи.
- `list`: обход без форматирования/печати каждой записи.
- `create`: запись заголовка и сброс; проверка исключена.
- `generate`: построение/запись данных, побайтовое перечитывание записей и сбросы; финальный обход исключён.

Открытие и закрытие файла исключены. `processed_bytes` для add — чтение старого файла плюс запись новой записи и заголовка; для find/list — размер прочитанного файла; для generate — записи/заголовки и прямое перечитывание записей, без финального обхода. Это логический объём, а не число физических байтов на диске. Таймер также включает обработку данных, поэтому его нельзя считать чистой скоростью диска.

В Release без отладчика выполнить минимум три поиска каждого выбранного UUID и неизвестного ID, например `00000000-0000-0000-0000-000000000000`. Для трёх добавлений каждый раз брать свежую копию одной базы и одинаковый текст. Подготовку копий не включать в замер. Таблица отчёта:

| Записей до операции | Размер до операции | maxLen генератора | Операция | Время 1, мс | Время 2, мс | Время 3, мс | Медиана, мс |
| --- | --- | --- | --- | --- | --- | --- | --- |

Указать параметры машины, Release x64 и влияние кэша. Программа не очищает кэш. Раннее совпадение поиска не устраняет полный обход для валидации.

Формат: заголовок 12 байт (`version:uint32`, `count:uint64`), запись — UUID 16 байт, длина uint32, ASCII. Числа little-endian; UUID в каноническом порядке байтов. Нет сигнатуры, индекса, сохранённого размера и проверки уникальности UUID. Проверки структуры не обнаруживают любое изменение допустимого текста.

UUID создаёт [UuidCreate](https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/nf-rpcdce-uuidcreate). Разбор и форматирование выполняют [UuidFromStringA](https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/nf-rpcdce-uuidfromstringa) и [UuidToStringA](https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/nf-rpcdce-uuidtostringa); выделенная RPC-строка освобождается через RpcStringFreeA. Собственные алгоритмы генерации/разбора UUID не используются.

## Тестирование (PowerShell, из каталога lr-2)

```powershell
$exe = ".\build\Release\main.exe"
$dir = Join-Path "lr-2-data" ("test-" + (Get-Date -Format "yyyyMMdd-HHmmss-fff"))
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$db = Join-Path $dir "example with spaces.db"

& $exe create $db
& $exe list $db
$added = & $exe add $db "A"
$added
$id = (($added | Where-Object { $_ -like "added id=*" }) -split "=")[1]
& $exe add $db "Hello"
cmd /c ('{0} add "{1}" ""' -f $exe, $db)
& $exe list $db
(Get-Item $db).Length
& $exe find $db $id
& $exe find $db "00000000-0000-0000-0000-000000000000"
$LASTEXITCODE

& $exe generate (Join-Path $dir "length-1.db") 100 1
(Get-Item (Join-Path $dir "length-1.db")).Length
& $exe generate (Join-Path $dir "long-text.db") 10 1048576

foreach ($count in 1000, 10000, 100000) {
    $base = Join-Path $dir "base-$count.db"
    $log = Join-Path $dir "results-$count.txt"
    $generated = & $exe generate $base $count 1024
    $generated | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { throw "generate failed for count=$count" }

    $ids = @($generated | Where-Object { $_ -match "^(first|middle|last)_uuid=" } | ForEach-Object { ($_ -split "=")[1] })
    $ids += "00000000-0000-0000-0000-000000000000"

    foreach ($uuid in $ids) {
        foreach ($repeat in 1..3) {
            "find count=$count uuid=$uuid repeat=$repeat" | Tee-Object -FilePath $log -Append
            & $exe find $base $uuid | Tee-Object -FilePath $log -Append
        }
    }

    foreach ($repeat in 1..3) {
        $copy = Join-Path $dir "add-$count-$repeat.db"
        Copy-Item $base $copy
        "add count=$count text_length=256 repeat=$repeat" | Tee-Object -FilePath $log -Append
        & $exe add $copy ("A" * 256) | Tee-Object -FilePath $log -Append
    }
}

& $exe create $db
$LASTEXITCODE
& $exe generate $db 10 256
$LASTEXITCODE
& $exe list (Join-Path $dir "missing.db")
$LASTEXITCODE
& $exe list $dir
$LASTEXITCODE
& $exe find $db "invalid-uuid"
$LASTEXITCODE
& $exe generate (Join-Path $dir "invalid.db") 0 256
$LASTEXITCODE
& $exe generate (Join-Path $dir "invalid.db") 10 0
$LASTEXITCODE
& $exe generate (Join-Path $dir "invalid.db") 10 1048577
$LASTEXITCODE
& $exe generate (Join-Path $dir "invalid.db") 18446744073709551615 1024
$LASTEXITCODE
& $exe add $db ([string][char]0x0410)
$LASTEXITCODE

$broken = Join-Path $dir "broken.db"
Copy-Item $db $broken
$bytes = [System.IO.File]::ReadAllBytes($broken)
$bytes[0] = 99
[System.IO.File]::WriteAllBytes($broken, $bytes)
& $exe list $broken
$LASTEXITCODE
Get-FileHash $broken
& $exe add $broken "Must not be written"
$LASTEXITCODE
Get-FileHash $broken

$locked = [System.IO.File]::Open($db, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
try { & $exe add $db "Locked"; $LASTEXITCODE } finally { $locked.Dispose() }
```
