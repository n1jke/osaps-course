### 7.2 Проверка основных операций

Проверка выполнена на Windows с использованием программы из каталога build\Release. В листингах сокращён путь приглашения PowerShell и удалены служебные строки продолжения ввода; значения результатов сохранены.

#### Создание, добавление, просмотр и поиск

Создана база с именем, содержащим пробелы. Пустой файл занимает 12 байт. После добавления строк A, Hello и пустой строки количество записей стало равным 3, а размер — 78 байтам. Команда list вывела все три записи с правильными длинами. Поиск по полученному UUID вернул исходную запись; поиск нулевого UUID завершился сообщением Not found и кодом 0.

При первоначальном вызове add с пустой строкой непосредственно из PowerShell программа сообщила Incorrect number of arguments. В последующем прогоне пустой аргумент был передан через cmd /c, и запись длиной 0 успешно добавилась. Следовательно, пустой текст поддерживается программой, но в использованном способе запуска из PowerShell пустой аргумент не был передан приложению.

Листинг 7.1 — Проверка основных операций и генерации записей граничной длины.

```text
PS> $exe = ".\build\Release\main.exe"
PS> $dir = Join-Path "lr-2-data" ("test-" + (Get-Date -Format "yyyyMMdd-HHmmss-fff"))
PS> New-Item -ItemType Directory -Path $dir -Force | Out-Null
PS> $db = Join-Path $dir "example with spaces.db"
PS> & $exe create $db
count=0 file_bytes=12 processed_bytes=12 elapsed_ms=2.813600 verified=yes
PS> & $exe list $db
Database is empty
count=0 file_bytes=12 processed_bytes=12 elapsed_ms=0.033600 verified=yes
PS> $added = & $exe add $db "A"
PS> $added
added id=35954023-db5d-43ca-92e2-b08e3c6da4fc
count=1 file_bytes=33 processed_bytes=45 elapsed_ms=2.116800 verified=yes
PS> $id = (($added | Where-Object { $_ -like "added id=*" }) -split "=")[1]
PS> & $exe add $db "Hello"
added id=78ba6c5a-8dba-463d-9716-e5b740dafd73
count=2 file_bytes=58 processed_bytes=70 elapsed_ms=2.241200 verified=yes
PS> cmd /c ('{0} add "{1}" ""' -f $exe, $db)
added id=6f229a5e-e705-485e-9e8b-efb6c7656343
count=3 file_bytes=78 processed_bytes=90 elapsed_ms=1.976700 verified=yes
PS> & $exe list $db
id=35954023-db5d-43ca-92e2-b08e3c6da4fc text_length=1 text="A"
id=78ba6c5a-8dba-463d-9716-e5b740dafd73 text_length=5 text="Hello"
id=6f229a5e-e705-485e-9e8b-efb6c7656343 text_length=0 text=""
count=3 file_bytes=78 processed_bytes=78 elapsed_ms=0.045800 verified=yes
PS> (Get-Item $db).Length
78
PS> & $exe find $db $id
id=35954023-db5d-43ca-92e2-b08e3c6da4fc text_length=1 text="A"
count=3 file_bytes=78 processed_bytes=78 elapsed_ms=0.035100 verified=yes
PS> & $exe find $db "00000000-0000-0000-0000-000000000000"
Not found
count=3 file_bytes=78 processed_bytes=78 elapsed_ms=0.034300 verified=yes
PS> $LASTEXITCODE
0
PS> & $exe generate (Join-Path $dir "length-1.db") 100 1
first_uuid=04af0162-d0ce-4bd9-a60b-c2fb2bb2a2ad
middle_uuid=10533fbf-fbb7-493f-aa81-a8f63922f497
last_uuid=e8f0071a-601b-43c8-a841-b1f441474eab
maxLen=1
count=100 file_bytes=2112 processed_bytes=4224 elapsed_ms=2.035100 verified=yes
PS> (Get-Item (Join-Path $dir "length-1.db")).Length
2112
PS> & $exe generate (Join-Path $dir "long-text.db") 10 1048576
first_uuid=c65a1178-fab7-49aa-8c58-ebf25c9ec79c
middle_uuid=1814f2dc-22c4-4398-835f-adcfab84d5cf
last_uuid=34e0ff26-63c2-4078-a376-5fd5a4d6e1b3
maxLen=1048576
count=10 file_bytes=5083250 processed_bytes=10166500 elapsed_ms=25.787100 verified=yes
```

Генерация 100 записей длиной 1 байт дала файл размером 2 112 байт, что соответствует заголовку и 100 записям по 21 байту. При maxLen=1 048 576 успешно создана база из 10 случайных записей размером 5 083 250 байт. Этот прогон проверяет допустимость верхней границы параметра maxLen и работу с длинными текстами; он не гарантирует наличие записи длиной ровно 1 MiB.

#### Генерация баз большого размера

Для maxLen=256 созданы базы из 10 000, 100 000 и 1 000 000 записей. Во всех трёх случаях программа завершила внутреннюю проверку и вывела verified=yes. Повторные попытки создать файлы с теми же именами были отклонены с кодом Windows 80 («Файл существует»).

Листинг 7.2 — Генерация баз и отказ от повторного создания существующих файлов.

```text
PS> build\Release\main.exe generate "lr-2-data\small.db" 10000 256
first_uuid=7ec96d8a-6af5-46c5-8c57-2708391e3be3
middle_uuid=e12db664-96f5-4ff5-aff3-9873e3412295
last_uuid=03d82d86-977d-43da-8a0c-2046ca9be47a
maxLen=256
count=10000 file_bytes=1486227 processed_bytes=2972454 elapsed_ms=59.039300 verified=yes
PS> build\Release\main.exe generate "lr-2-data\medium.db" 100000 256
first_uuid=59c03263-1f8b-4836-ad0d-73c3934e6916
middle_uuid=52fb69bf-caaa-4cec-9375-86fa2803b344
last_uuid=6f2337be-ee8f-41f4-98b8-c52d1532bd8d
maxLen=256
count=100000 file_bytes=14819397 processed_bytes=29638794 elapsed_ms=324.763100 verified=yes
PS> build\Release\main.exe generate "lr-2-data\large.db" 1000000 256
first_uuid=ae820885-1cda-427e-a4d7-486174f18308
middle_uuid=4ef38605-f84c-4cbf-ad8a-6c8f5ba7225d
last_uuid=3cd9eaaa-3bff-4d98-916f-ee673e9a39c2
maxLen=256
count=1000000 file_bytes=148378668 processed_bytes=296757336 elapsed_ms=3136.114100 verified=yes
PS> build\Release\main.exe generate "lr-2-data\small.db" 10000 256
[ERROR] CreateFileA: The file exists.: 80 : Файл существует.
PS> build\Release\main.exe generate "lr-2-data\medium.db" 100000 256
[ERROR] CreateFileA: The file exists.: 80 : Файл существует.
PS> build\Release\main.exe generate "lr-2-data\large.db" 1000000 256
[ERROR] CreateFileA: The file exists.: 80 : Файл существует.
```

Таблица 7.1 — Результаты генерации при maxLen=256. Каждый размер измерен одним прогоном; это отдельная серия от повторных замеров поиска и добавления.

| Записей | Размер файла, байт | Размер, MiB | Время генерации, мс |
| --- | --- | --- | --- |
| 10 000 | 1 486 227 | 1.42 | 59.0393 |
| 100 000 | 14 819 397 | 14.13 | 324.7631 |
| 1 000 000 | 148 378 668 | 141.50 | 3136.1141 |

Один MiB равен 1 048 576 байтам. Время generate включает построение данных, запись, непосредственное перечитывание записей для сравнения и сбросы буферов. Поэтому его нельзя напрямую сравнивать со временем find или считать чистым временем записи на диск.

#### Проверка ошибочных входных данных

Проверены существующий файл, отсутствующий файл, путь к каталогу, неверный UUID, нулевое количество записей, недопустимая максимальная длина, переполнение возможного размера базы и текст вне ASCII. Ошибки файловых операций завершались кодом 1; ошибки аргументов — кодом 2. Сообщения Windows API содержат название операции, числовой код и описание ошибки.

Листинг 7.3 — Диагностика ошибок файловых операций и аргументов.

```text
PS> & $exe create $db
[ERROR] CreateFileA: The file exists.: 80 : Файл существует.
PS> $LASTEXITCODE
1
PS> & $exe generate $db 10 256
[ERROR] CreateFileA: The file exists.: 80 : Файл существует.
PS> $LASTEXITCODE
1
PS> & $exe list (Join-Path $dir "missing.db")
[ERROR] CreateFileA: The system cannot find the file specified.: 2 : Не удается найти указанный файл.
PS> $LASTEXITCODE
1
PS> & $exe list $dir
[ERROR] CreateFileA: Access is denied.: 5 : Отказано в доступе.
PS> $LASTEXITCODE
1
PS> & $exe find $db "invalid-uuid"
[ARGUMENT ERROR] UUID must contain 36 ASCII characters
Use --help
PS> $LASTEXITCODE
2
PS> & $exe generate (Join-Path $dir "invalid.db") 0 256
[ARGUMENT ERROR] Value must be positive
Use --help
PS> $LASTEXITCODE
2
PS> & $exe generate (Join-Path $dir "invalid.db") 10 0
[ARGUMENT ERROR] Value must be positive
Use --help
PS> $LASTEXITCODE
2
PS> & $exe generate (Join-Path $dir "invalid.db") 10 1048577
[ARGUMENT ERROR] maxLen must be in 1..1048576 bytes
Use --help
PS> $LASTEXITCODE
2
PS> & $exe generate (Join-Path $dir "invalid.db") 18446744073709551615 1024
[ARGUMENT ERROR] Generated file size would overflow a Windows file offset
Use --help
PS> $LASTEXITCODE
2
PS> & $exe add $db ([string][char]0x0410)
[ARGUMENT ERROR] Text must be ASCII, at most 1 MiB
Use --help
PS> $LASTEXITCODE
2
```

### 7.3 Измерение времени поиска и добавления

Эти замеры получены для исходной версии с полным обходом при поиске. Текущая версия останавливается при первом совпадении: время поиска первой и средней записей требует повторного измерения. Ниже сохранены фактические результаты исходной версии.

Для этой серии сформированы три базы с maxLen=1024. Выполнено по три поиска первой, средней, последней и отсутствующей записи. Добавление строки из 256 символов A также повторено трижды, каждый раз на отдельной копии исходной базы. Всего получено 45 замеров. Для сравнения использована медиана трёх значений elapsed_ms.

Таблица 7.2 — Размеры баз и медианное время операций. Время указано в миллисекундах, медианы вычислены по трём прогонам. Для всех баз maxLen=1024; добавляется текст длиной 256 байт.

| Записей до операции | Размер файла, байт | Размер, MiB | Поиск первой, мс | Поиск средней, мс | Поиск последней, мс | Поиск отсутствующей, мс | Добавление, мс |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 000 | 527 603 | 0.50 | 2.0560 | 2.2786 | 2.6036 | 2.0383 | 4.8370 |
| 10 000 | 5 309 327 | 5.06 | 28.7928 | 28.5514 | 21.8642 | 21.5400 | 27.9096 |
| 100 000 | 53 277 199 | 50.81 | 231.1791 | 213.6776 | 237.9305 | 237.5462 | 278.9017 |

Таблица 7.3 — Время поиска и добавления, мс. Количество записей указано до операции.

| Записей | Операция | Прогон 1 | Прогон 2 | Прогон 3 | Медиана |
| --- | --- | --- | --- | --- | --- |
| 1 000 | find: первая | 2.0560 | 2.0164 | 2.8514 | 2.0560 |
| 1 000 | find: средняя | 2.0205 | 2.2886 | 2.2786 | 2.2786 |
| 1 000 | find: последняя | 2.6036 | 2.4456 | 3.0649 | 2.6036 |
| 1 000 | find: отсутствующая | 2.0383 | 2.1625 | 2.0365 | 2.0383 |
| 1 000 | add: 256 байт | 4.8370 | 5.1300 | 4.4152 | 4.8370 |
| 10 000 | find: первая | 28.7928 | 24.8311 | 29.2392 | 28.7928 |
| 10 000 | find: средняя | 28.1454 | 32.6478 | 28.5514 | 28.5514 |
| 10 000 | find: последняя | 21.8160 | 21.8642 | 22.1794 | 21.8642 |
| 10 000 | find: отсутствующая | 21.5400 | 21.0715 | 22.7299 | 21.5400 |
| 10 000 | add: 256 байт | 49.0415 | 26.5832 | 27.9096 | 27.9096 |
| 100 000 | find: первая | 231.1791 | 245.5214 | 225.4458 | 231.1791 |
| 100 000 | find: средняя | 213.0139 | 213.6776 | 236.8393 | 213.6776 |
| 100 000 | find: последняя | 220.5283 | 239.8074 | 237.9305 | 237.9305 |
| 100 000 | find: отсутствующая | 236.3401 | 237.5462 | 238.4014 | 237.5462 |
| 100 000 | add: 256 байт | 298.7619 | 278.9017 | 274.1367 | 278.9017 |

После каждого добавления количество записей увеличилось на 1, размер — на 276 байт: 20 байт служебных полей и 256 байт текста. Все операции завершились с verified=yes.

Листинг 7.4 содержит сокращённый вывод серии замеров. Строки самих найденных записей и UUID новых записей опущены; сохранены идентификаторы сценариев, номера повторов и полные строки статистики.

Листинг 7.4 — Повторные замеры поиска и добавления.

```text
first_uuid=294792fb-b9d6-433f-bf5d-72b6d9332824
middle_uuid=e175b1d2-1a03-4182-a65f-cf3d61d147c4
last_uuid=1b2c1b53-4ab5-4ec2-84aa-92c27af6cdc7
maxLen=1024
count=1000 file_bytes=527603 processed_bytes=1055206 elapsed_ms=7.604700 verified=yes
find count=1000 uuid=294792fb-b9d6-433f-bf5d-72b6d9332824 repeat=1
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.056000 verified=yes
find count=1000 uuid=294792fb-b9d6-433f-bf5d-72b6d9332824 repeat=2
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.016400 verified=yes
find count=1000 uuid=294792fb-b9d6-433f-bf5d-72b6d9332824 repeat=3
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.851400 verified=yes
find count=1000 uuid=e175b1d2-1a03-4182-a65f-cf3d61d147c4 repeat=1
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.020500 verified=yes
find count=1000 uuid=e175b1d2-1a03-4182-a65f-cf3d61d147c4 repeat=2
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.288600 verified=yes
find count=1000 uuid=e175b1d2-1a03-4182-a65f-cf3d61d147c4 repeat=3
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.278600 verified=yes
find count=1000 uuid=1b2c1b53-4ab5-4ec2-84aa-92c27af6cdc7 repeat=1
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.603600 verified=yes
find count=1000 uuid=1b2c1b53-4ab5-4ec2-84aa-92c27af6cdc7 repeat=2
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.445600 verified=yes
find count=1000 uuid=1b2c1b53-4ab5-4ec2-84aa-92c27af6cdc7 repeat=3
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=3.064900 verified=yes
find count=1000 uuid=00000000-0000-0000-0000-000000000000 repeat=1
Not found
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.038300 verified=yes
find count=1000 uuid=00000000-0000-0000-0000-000000000000 repeat=2
Not found
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.162500 verified=yes
find count=1000 uuid=00000000-0000-0000-0000-000000000000 repeat=3
Not found
count=1000 file_bytes=527603 processed_bytes=527603 elapsed_ms=2.036500 verified=yes
add count=1000 text_length=256 repeat=1
count=1001 file_bytes=527879 processed_bytes=527891 elapsed_ms=4.837000 verified=yes
add count=1000 text_length=256 repeat=2
count=1001 file_bytes=527879 processed_bytes=527891 elapsed_ms=5.130000 verified=yes
add count=1000 text_length=256 repeat=3
count=1001 file_bytes=527879 processed_bytes=527891 elapsed_ms=4.415200 verified=yes
first_uuid=670c59e3-6268-4086-b01f-94ae658db7bc
middle_uuid=776f7ead-d278-4e54-b192-44d8b5a0bc9f
last_uuid=53cf6e83-ba98-4442-8d51-a2e1dc0adf9f
maxLen=1024
count=10000 file_bytes=5309327 processed_bytes=10618654 elapsed_ms=58.331000 verified=yes
find count=10000 uuid=670c59e3-6268-4086-b01f-94ae658db7bc repeat=1
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=28.792800 verified=yes
find count=10000 uuid=670c59e3-6268-4086-b01f-94ae658db7bc repeat=2
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=24.831100 verified=yes
find count=10000 uuid=670c59e3-6268-4086-b01f-94ae658db7bc repeat=3
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=29.239200 verified=yes
find count=10000 uuid=776f7ead-d278-4e54-b192-44d8b5a0bc9f repeat=1
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=28.145400 verified=yes
find count=10000 uuid=776f7ead-d278-4e54-b192-44d8b5a0bc9f repeat=2
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=32.647800 verified=yes
find count=10000 uuid=776f7ead-d278-4e54-b192-44d8b5a0bc9f repeat=3
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=28.551400 verified=yes
find count=10000 uuid=53cf6e83-ba98-4442-8d51-a2e1dc0adf9f repeat=1
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=21.816000 verified=yes
find count=10000 uuid=53cf6e83-ba98-4442-8d51-a2e1dc0adf9f repeat=2
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=21.864200 verified=yes
find count=10000 uuid=53cf6e83-ba98-4442-8d51-a2e1dc0adf9f repeat=3
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=22.179400 verified=yes
find count=10000 uuid=00000000-0000-0000-0000-000000000000 repeat=1
Not found
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=21.540000 verified=yes
find count=10000 uuid=00000000-0000-0000-0000-000000000000 repeat=2
Not found
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=21.071500 verified=yes
find count=10000 uuid=00000000-0000-0000-0000-000000000000 repeat=3
Not found
count=10000 file_bytes=5309327 processed_bytes=5309327 elapsed_ms=22.729900 verified=yes
add count=10000 text_length=256 repeat=1
count=10001 file_bytes=5309603 processed_bytes=5309615 elapsed_ms=49.041500 verified=yes
add count=10000 text_length=256 repeat=2
count=10001 file_bytes=5309603 processed_bytes=5309615 elapsed_ms=26.583200 verified=yes
add count=10000 text_length=256 repeat=3
count=10001 file_bytes=5309603 processed_bytes=5309615 elapsed_ms=27.909600 verified=yes
first_uuid=f8517441-6acb-4c3f-8e41-88164ed669c1
middle_uuid=f585b302-b391-435a-a082-e7fc22f7066d
last_uuid=9260fc36-df86-4347-b166-ec929fa4ced2
maxLen=1024
count=100000 file_bytes=53277199 processed_bytes=106554398 elapsed_ms=526.440600 verified=yes
find count=100000 uuid=f8517441-6acb-4c3f-8e41-88164ed669c1 repeat=1
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=231.179100 verified=yes
find count=100000 uuid=f8517441-6acb-4c3f-8e41-88164ed669c1 repeat=2
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=245.521400 verified=yes
find count=100000 uuid=f8517441-6acb-4c3f-8e41-88164ed669c1 repeat=3
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=225.445800 verified=yes
find count=100000 uuid=f585b302-b391-435a-a082-e7fc22f7066d repeat=1
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=213.013900 verified=yes
find count=100000 uuid=f585b302-b391-435a-a082-e7fc22f7066d repeat=2
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=213.677600 verified=yes
find count=100000 uuid=f585b302-b391-435a-a082-e7fc22f7066d repeat=3
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=236.839300 verified=yes
find count=100000 uuid=9260fc36-df86-4347-b166-ec929fa4ced2 repeat=1
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=220.528300 verified=yes
find count=100000 uuid=9260fc36-df86-4347-b166-ec929fa4ced2 repeat=2
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=239.807400 verified=yes
find count=100000 uuid=9260fc36-df86-4347-b166-ec929fa4ced2 repeat=3
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=237.930500 verified=yes
find count=100000 uuid=00000000-0000-0000-0000-000000000000 repeat=1
Not found
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=236.340100 verified=yes
find count=100000 uuid=00000000-0000-0000-0000-000000000000 repeat=2
Not found
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=237.546200 verified=yes
find count=100000 uuid=00000000-0000-0000-0000-000000000000 repeat=3
Not found
count=100000 file_bytes=53277199 processed_bytes=53277199 elapsed_ms=238.401400 verified=yes
add count=100000 text_length=256 repeat=1
count=100001 file_bytes=53277475 processed_bytes=53277487 elapsed_ms=298.761900 verified=yes
add count=100000 text_length=256 repeat=2
count=100001 file_bytes=53277475 processed_bytes=53277487 elapsed_ms=278.901700 verified=yes
add count=100000 text_length=256 repeat=3
count=100001 file_bytes=53277475 processed_bytes=53277487 elapsed_ms=274.136700 verified=yes
```

### 7.4 Анализ полученных результатов

При увеличении базы с 1 000 до 100 000 записей её размер вырос с 527 603 до 53 277 199 байт, приблизительно в 101 раз. Медиана поиска отсутствующего UUID увеличилась с 2,0383 до 237,5462 мс, приблизительно в 116,5 раза. Медиана добавления увеличилась с 4,8370 до 278,9017 мс, приблизительно в 57,7 раза. Таким образом, в данной серии время обеих операций существенно возрастает с размером базы; результаты согласуются с необходимостью полного обхода существующих записей.

Расположение найденной записи не сокращает объём чтения: processed_bytes во всех поисках равен размеру всей базы. Для 100 000 записей медианы поиска первой, средней и последней записей составили 231,1791; 213,6776 и 237,9305 мс соответственно, а отсутствующей — 237,5462 мс. Последняя запись на базе из 10 000 записей искалась быстрее первой, что не свидетельствует о более быстром доступе к концу файла: алгоритм во всех случаях выполняет одинаковый полный обход.

Добавление включает предварительный обход, запись новой записи, обновление заголовка и два сброса файловых буферов. Поэтому оно занимает больше времени, чем только чтение базы. Для 10 000 записей получены значения 49,0415; 26,5832 и 27,9096 мс. Медиана 27,9096 мс уменьшает влияние единичного повышенного значения.

Разброс времени может быть связан с файловым кэшем, фоновой нагрузкой и задержками сохранения. Кэш в этой серии не очищался; причины отдельных отклонений отдельно не измерялись. Три повтора позволяют сравнить результаты данной серии, но не доказывают точную пропорциональность времени размеру файла. Измеряемое время включает обработку записей и проверки, поэтому характеризует работу программы, а не только скорость накопителя.

Алгоритмическая сложность поиска составляет Θ(B), добавления — Θ(B + L), где B — размер исходной базы, L — длина нового текста. Чтобы ускорить доступ, можно добавить индекс UUID и смещений, а также пересмотреть необходимость полного обхода перед каждым добавлением.

## 8 Вывод

Разработана и проверена на Windows файловая база записей переменной длины с использованием синхронных функций Windows API. Прогоны подтвердили создание базы, добавление и просмотр записей, поиск существующего и отсутствующего UUID, хранение пустого текста и обработку путей с пробелами. Размер базы из трёх записей составил 78 байт, а генерация 100 однобайтовых записей дала ожидаемые 2 112 байт. Продемонстрированы отказы при неверных аргументах и ошибках открытия файла.

Сгенерирована база из 1 000 000 записей размером 148 378 668 байт с успешной внутренней проверкой; время генерации составило 3 136,1141 мс. В серии из 45 замеров медиана поиска отсутствующего UUID выросла с 2,0383 до 237,5462 мс, а добавления — с 4,8370 до 278,9017 мс при увеличении количества записей с 1 000 до 100 000.

Полученные результаты показывают рост затрат на поиск и добавление с увеличением базы. Причина — полный последовательный обход для проверки данных. Положение искомой записи не уменьшает объём чтения. Прямой доступ к переменным записям требует знания размеров предыдущих записей либо хранения индекса смещений. Для улучшения производительности целесообразно использовать индекс; при оценке измерений необходимо учитывать файловый кэш и затраты на сохранение данных.
