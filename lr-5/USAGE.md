# ЛР №5. Сборка и проверка

Вариант 26. Клиент отправляет `GET` и `HEAD` по HTTP/1.1 поверх TCP через Winsock. Go-сервер выдаёт фиксированные ответы для проверки. Параметры и обработка ответа описаны в [RFC](rfc.md).

## Запуск на Windows

Нужны Go, CMake и инструменты Visual Studio с компонентами для разработки на C++ и Windows SDK. Открой два терминала PowerShell в корне репозитория.

В первом терминале запусти сервер. Дождись сообщения `server started` с адресом `127.0.0.1:20112`.

```powershell
Set-Location lr-5
go run ./server/main.go
```

Во втором терминале собери клиент. Эти команды рассчитаны на генератор Visual Studio по умолчанию.

```powershell
Set-Location lr-5
cmake -S . -B build
cmake --build build --config Release
$client = ".\build\Release\http_client.exe"
& $client GET /small
$LASTEXITCODE
```

На клиенте `--host`, `--port`, `--timeout-ms` и `--count` задают адрес, порт, общий тайм-аут одной попытки и число последовательных запросов. По умолчанию это `127.0.0.1`, `20112`, 3000 мс и один запрос. У сервера флаги `-host` и `-port`. Для локальной проверки оставь адрес по умолчанию.

## Ручная проверка

В PowerShell `& $client` запускает исполняемый файл по пути, записанному в переменной `$client`. Дальше идут два аргумента клиента: метод `GET` и HTTP-путь `/small` или `/large`. Это маршруты тестового сервера, которые возвращают 1 КиБ и 1 МиБ.

```powershell
& $client GET /small
& $client GET /large
& $client GET /empty
& $client HEAD /small
& $client GET /missing
& $client HEAD /missing
& $client GET /chunked
$LASTEXITCODE
& $client GET /slow --timeout-ms 200
$LASTEXITCODE
& $client POST /small
$LASTEXITCODE
```

```txt
PS D:\osasp\osaps-course\lr-5> & $client GET /small
18:13:46.188 CLIENT pid=18452 run_id=18452-7901358830974 peer=127.0.0.1:20112 attempt=1 GET /small
18:13:46.190 CLIENT pid=18452 run_id=18452-7901358830974 peer=127.0.0.1:20112 attempt=1 response complete status=200
HTTP/1.1 200 OK
Content-Length: 1024
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:13:46 GMT
Connection: close

body_bytes=1024
body_preview=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
attempts=1 requests=1 responses=1 successful=1 errors=0 timeouts=0 sent_http_bytes=65 received_http_bytes=1162 body_bytes=1024 mean_attempt_ms=1.549 successful_get_body_bytes_per_second=661114.339
PS D:\osasp\osaps-course\lr-5> & $client GET /large
18:13:46.245 CLIENT pid=5376 run_id=5376-7901359412132 peer=127.0.0.1:20112 attempt=1 GET /large
18:13:46.249 CLIENT pid=5376 run_id=5376-7901359412132 peer=127.0.0.1:20112 attempt=1 response complete status=200
HTTP/1.1 200 OK
Content-Length: 1048576
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:13:46 GMT
Connection: close

body_bytes=1048576
body_preview=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
attempts=1 requests=1 responses=1 successful=1 errors=0 timeouts=0 sent_http_bytes=65 received_http_bytes=1048717 body_bytes=1048576 mean_attempt_ms=2.856 successful_get_body_bytes_per_second=367199887.939
PS D:\osasp\osaps-course\lr-5> & $client GET /empty
18:13:46.288 CLIENT pid=18396 run_id=18396-7901359837022 peer=127.0.0.1:20112 attempt=1 GET /empty
18:13:46.300 CLIENT pid=18396 run_id=18396-7901359837022 peer=127.0.0.1:20112 attempt=1 response complete status=200
HTTP/1.1 200 OK
Content-Length: 0
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:13:46 GMT
Connection: close

body_bytes=0
attempts=1 requests=1 responses=1 successful=1 errors=0 timeouts=0 sent_http_bytes=65 received_http_bytes=135 body_bytes=0 mean_attempt_ms=11.115 successful_get_body_bytes_per_second=0.000
PS D:\osasp\osaps-course\lr-5> & $client HEAD /small
18:13:46.337 CLIENT pid=20716 run_id=20716-7901360329743 peer=127.0.0.1:20112 attempt=1 HEAD /small
18:13:46.340 CLIENT pid=20716 run_id=20716-7901360329743 peer=127.0.0.1:20112 attempt=1 response complete status=200
HTTP/1.1 200 OK
Content-Length: 1024
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:13:46 GMT
Connection: close

body_bytes=0
attempts=1 requests=1 responses=1 successful=1 errors=0 timeouts=0 sent_http_bytes=66 received_http_bytes=138 body_bytes=0 mean_attempt_ms=1.693
PS D:\osasp\osaps-course\lr-5> & $client GET /missing
18:13:46.371 CLIENT pid=13724 run_id=13724-7901360685184 peer=127.0.0.1:20112 attempt=1 GET /missing
18:13:46.401 CLIENT pid=13724 run_id=13724-7901360685184 peer=127.0.0.1:20112 attempt=1 response complete status=404
HTTP/1.1 404 Not Found
Content-Length: 10
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:13:46 GMT
Connection: close

body_bytes=10
body_preview=not found\x0a
attempts=1 requests=1 responses=1 successful=1 errors=0 timeouts=0 sent_http_bytes=67 received_http_bytes=153 body_bytes=10 mean_attempt_ms=27.271 successful_get_body_bytes_per_second=366.686
PS D:\osasp\osaps-course\lr-5> & $client HEAD /missing
18:13:46.442 CLIENT pid=18912 run_id=18912-7901361384643 peer=127.0.0.1:20112 attempt=1 HEAD /missing
18:13:46.468 CLIENT pid=18912 run_id=18912-7901361384643 peer=127.0.0.1:20112 attempt=1 response complete status=404
HTTP/1.1 404 Not Found
Content-Length: 10
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:13:46 GMT
Connection: close

body_bytes=0
attempts=1 requests=1 responses=1 successful=1 errors=0 timeouts=0 sent_http_bytes=68 received_http_bytes=143 body_bytes=0 mean_attempt_ms=24.212
PS D:\osasp\osaps-course\lr-5> & $client GET /chunked
18:13:46.507 CLIENT pid=912 run_id=912-7901362034131 peer=127.0.0.1:20112 attempt=1 GET /chunked
18:13:46.548 CLIENT pid=912 run_id=912-7901362034131 peer=127.0.0.1:20112 attempt=1 error http: unsupported Transfer-Encoding: chunked
attempts=1 requests=1 responses=0 successful=0 errors=1 timeouts=0 sent_http_bytes=67 received_http_bytes=1180 body_bytes=0 mean_attempt_ms=39.667
PS D:\osasp\osaps-course\lr-5> $LASTEXITCODE
1
PS D:\osasp\osaps-course\lr-5> & $client GET /slow --timeout-ms 200
18:13:46.655 CLIENT pid=20536 run_id=20536-7901363516048 peer=127.0.0.1:20112 attempt=1 GET /slow
18:13:46.860 CLIENT pid=20536 run_id=20536-7901363516048 peer=127.0.0.1:20112 attempt=1 timeout receive: recv timed out
attempts=1 requests=1 responses=0 successful=0 errors=0 timeouts=1 sent_http_bytes=64 received_http_bytes=0 body_bytes=0 mean_attempt_ms=203.497
PS D:\osasp\osaps-course\lr-5> $LASTEXITCODE
1
PS D:\osasp\osaps-course\lr-5> & $client POST /small
argument error: method must be GET or HEAD
usage: http_client GET|HEAD <path> [--host <ipv4>] [--port <port>] [--timeout-ms <ms>] [--count <n>] [--csv <path>]
PS D:\osasp\osaps-course\lr-5> $LASTEXITCODE
2

PS D:\osasp\osaps-course\lr-5> go run ./server/main.go  
2026/11/23 18:12:11 INFO server started pid=19320 address=127.0.0.1:20112
2026/11/23 18:13:04 INFO request completed requestID=1 peer=127.0.0.1:61893 method=GET path=/small status=200
2026/11/23 18:13:46 INFO request completed requestID=2 peer=127.0.0.1:50714 method=GET path=/small status=200
2026/11/23 18:13:46 INFO request completed requestID=3 peer=127.0.0.1:50715 method=GET path=/large status=200
2026/11/23 18:13:46 INFO request completed requestID=4 peer=127.0.0.1:50716 method=GET path=/empty status=200
2026/11/23 18:13:46 INFO request completed requestID=5 peer=127.0.0.1:50717 method=HEAD path=/small status=200
2026/11/23 18:13:46 INFO request completed requestID=6 peer=127.0.0.1:50718 method=GET path=/missing status=404
2026/11/23 18:13:46 INFO request completed requestID=7 peer=127.0.0.1:50719 method=HEAD path=/missing status=404
2026/11/23 18:13:46 INFO request completed requestID=8 peer=127.0.0.1:50720 method=GET path=/chunked status=200
2026/11/23 18:13:46 INFO request cancelled requestID=9 peer=127.0.0.1:50721 method=GET path=/slow
2026/11/23 18:15:13 INFO request completed requestID=10 peer=127.0.0.1:50529 method=GET path=/small status=200
2026/11/23 18:15:13 INFO request completed requestID=11 peer=127.0.0.1:50530 method=GET path=/small status=200
2026/11/23 18:15:13 INFO request completed requestID=12 peer=127.0.0.1:50531 method=GET path=/small status=200
2026/11/23 18:15:24 INFO server stopped

PS D:\osasp\osaps-course\lr-5> & $client GET /small --count 3 
18:15:13.344 CLIENT pid=17636 run_id=17636-7902230397100 peer=127.0.0.1:20112 attempt=1 GET /small
18:15:13.346 CLIENT pid=17636 run_id=17636-7902230397100 peer=127.0.0.1:20112 attempt=1 response complete status=200
HTTP/1.1 200 OK
Content-Length: 1024
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:15:13 GMT
Connection: close

body_bytes=1024
18:15:13.347 CLIENT pid=17636 run_id=17636-7902230397100 peer=127.0.0.1:20112 attempt=2 GET /small
18:15:13.359 CLIENT pid=17636 run_id=17636-7902230397100 peer=127.0.0.1:20112 attempt=2 response complete status=200
HTTP/1.1 200 OK
Content-Length: 1024
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:15:13 GMT
Connection: close

body_bytes=1024
18:15:13.360 CLIENT pid=17636 run_id=17636-7902230397100 peer=127.0.0.1:20112 attempt=3 GET /small
18:15:13.376 CLIENT pid=17636 run_id=17636-7902230397100 peer=127.0.0.1:20112 attempt=3 response complete status=200
HTTP/1.1 200 OK
Content-Length: 1024
Content-Type: text/plain; charset=utf-8
Date: Mon, 23 Nov 2026 15:15:13 GMT
Connection: close

body_bytes=1024
attempts=3 requests=3 responses=3 successful=3 errors=0 timeouts=0 sent_http_bytes=195 received_http_bytes=3486 body_bytes=3072 mean_attempt_ms=10.129 successful_get_body_bytes_per_second=101092.869

PS D:\osasp\osaps-course\lr-5> & $client GET /small --count 1
18:16:23.155 CLIENT pid=18828 run_id=18828-7902928504849 peer=127.0.0.1:20112 attempt=1 GET /small
18:16:25.185 CLIENT pid=18828 run_id=18828-7902928504849 peer=127.0.0.1:20112 attempt=1 error connect: connect (10061): Подключение не установлено, т.к. конечный компьютер отверг запрос на подключение.
attempts=1 requests=0 responses=0 successful=0 errors=1 timeouts=0 sent_http_bytes=0 received_http_bytes=0 body_bytes=0 mean_attempt_ms=2029.881
```

| Команда | Ожидаемый результат |
|---|---|
| `GET /small` | `200`, 1024 байта тела и превью до 256 байтов |
| `GET /large` | `200`, 1048576 байтов тела за несколько чтений |
| `GET /empty` | `200`, `Content-Length: 0` и ноль байтов тела |
| `HEAD /small` | `200`, `Content-Length: 1024` и ноль байтов тела |
| `GET /missing`, `HEAD /missing` | `404`, соответственно 10 и 0 байтов тела, код выхода клиента 0 |
| `GET /chunked` | Ошибка неподдерживаемого `Transfer-Encoding: chunked`, код выхода 1 |
| `GET /slow --timeout-ms 200` | Тайм-аут на чтении, код выхода 1 |
| `POST /small` | Ошибка аргументов, код выхода 2 |

Для проверки повторов запусти `& $client GET /small --count 3`. В итоге должны быть три завершённых запроса и три ответа. Для проверки параллельной работы запусти по одному `GET /slow` без дополнительных флагов в трёх терминалах примерно одновременно. Все три запроса должны завершиться с `200`.

Останови сервер через Ctrl+C. Повтори `& $client GET /small`. Клиент должен сообщить об отказе подключения и завершиться с кодом 1.

## Замеры

Оставь сервер запущенным. Все команды ниже выполняются во втором терминале. Скрипт делает по три отдельных запуска для каждой пары метода и пути. В одном запуске 100 последовательных запросов. Это даёт больше наблюдений внутри запуска, а три отдельных запуска показывают разброс между сериями. Для всей серии получится 24 CSV по 100 строк, если все команды дойдут до конца.

```powershell
$series = "measurements/lr5"
if (Test-Path $series) {
    throw "Move the existing measurements/lr5 directory before recording a new series."
}
New-Item -ItemType Directory -Path $series | Out-Null
$count = 100

foreach ($method in @("GET", "HEAD")) {
    foreach ($resource in @("small", "large", "empty", "missing")) {
        foreach ($repeat in 1..3) {
            $file = Join-Path $series "$method-$resource-$repeat.csv"
            & $client $method "/$resource" --count $count --csv $file
            if ($LASTEXITCODE -ne 0) {
                throw "Client failed for $method /$resource. See $file"
            }
        }
    }
}
```

`--csv` перезаписывает одноимённый файл. CSV содержит одну строку на попытку. Поля `request_complete` и `response_complete` имеют значения 0 или 1. `outcome=ok` означает, что HTTP-ответ полностью разобран, в том числе при ожидаемом `404`.

- `connect_ms` измеряет время от вызова `connect` до определения его результата. Если до вызова не дошли, поле пустое.
- `attempt_ms` измеряет время от начала создания сокета до конца разбора ответа или фиксации ошибки. В него входит работа сервера. Закрытие сокета, вывод и запись CSV за пределами этого интервала.
- `sent_http_bytes` и `received_http_bytes` учитывают байты, переданные и полученные приложением, включая HTTP-заголовки. `body_bytes` учитывает только байты HTTP-тела, которые клиент принял как часть ответа.

Цель эксперимента — проверить, как размер ресурса и выбор метода влияют на время клиентской операции и число переданных HTTP-байтов. `connect_ms` помогает оценить вклад подключения. `attempt_ms` включает работу сервера и приём ответа, поэтому это время операции на loopback, а не сетевой RTT.

Для графиков используй размер ресурса в байтах по оси X и две серии `GET` и `HEAD`. Для `HEAD` размер ресурса по оси X берётся из маршрута, фактически полученное тело равно нулю. Для одного графика считай среднее `attempt_ms` по запускам, для второго — среднее `received_http_bytes`. Ответы `404` сведи отдельно. Сохрани CSV, сведения о машине, конфигурацию сборки и версию исходников.
