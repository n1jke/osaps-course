# Лабораторная работа № 1: процессы, потоки и Fiber в Windows

Вариант 26 — **Process против Thread**. Этот конспект составлен по требованиям [task.md](task.md); конкретный алгоритм и способ обмена результатами он намеренно не предписывает.

## 1. База, которую нужно знать независимо от варианта

### Process, thread, fiber

**Process** — provides the resources needed to execute a program. A process has a virtual address space, executable code, open handles to system objects, a security context, a unique process identifier, environment variables, a priority class, minimum and maximum working set sizes, and at least one thread of execution. Each process is started with a single thread, often called the primary thread, but can create additional threads from any of its threads.

**Thread** — is the entity within a process that can be scheduled for execution. All threads of a process share its virtual address space and system resources. In addition, each thread maintains exception handlers, a scheduling priority, thread local storage, a unique thread identifier, and a set of structures the system will use to save the thread context until it is scheduled. The thread context includes the thread's set of machine registers, the kernel stack, a thread environment block, and a user stack in the address space of the thread's process. Threads can also have their own security context, which can be used for impersonating clients.

**Fiber** - is a unit of execution that must be manually scheduled by the application. Fibers run in the context of the threads that schedule them. Each thread can schedule multiple fibers. In general, fibers do not provide advantages over a well-designed multithreaded application. However, using fibers can make it easier to port applications that were designed to schedule their own threads.

`PID` и `TID` — process/thread identifier. Это не дескрипторы и сами по себе не предоставляют доступа к объекту. 
**Дескриптор** (`HANDLE`) — значение в таблице дескрипторов процесса, через которое выполняются операции с объектом ядра; после использования его закрывают `CloseHandle`.

[Microsoft Learn: About Processes and Threads](https://learn.microsoft.com/en-us/windows/win32/procthread/about-processes-and-threads).

[Microsoft Learn: Fibers](https://learn.microsoft.com/en-us/windows/win32/procthread/fibers)

### Минимальный Windows API

| Задача | Функции |
|---|---|
| Создать процесс и получить PID/дескрипторы | [`CreateProcess`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw), `PROCESS_INFORMATION` |
| Создать поток и получить TID/дескриптор | [`CreateThread`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread) или допустимый `_beginthreadex` |
| Ждать завершения | [`WaitForSingleObject`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject), [`WaitForMultipleObjects`](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects) |
| Узнать код завершения | [`GetExitCodeProcess`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess), `GetExitCodeThread` |
| Изменить приоритет | `SetPriorityClass`, `SetThreadPriority` |
| Приостановить / продолжить поток | `SuspendThread`, `ResumeThread` |
| Освободить дескриптор | [`CloseHandle`](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle) |
| Работать с Fiber | `ConvertThreadToFiber`, `CreateFiber`, `SwitchToFiber`, `DeleteFiber`, `ConvertFiberToThread` |
| Измерить интервал | [`QueryPerformanceCounter`](https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) и `QueryPerformanceFrequency` |
| Показать ошибку | `GetLastError`, [`FormatMessageW`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessagew) |

`CreateProcess` при успехе заполняет `PROCESS_INFORMATION`: `hProcess`, `hThread`, `dwProcessId`, `dwThreadId`. Оба возвращённых дескриптора принадлежат вызывающему процессу и должны быть закрыты. Функция возвращает до полной инициализации ребёнка, поэтому состояние и код завершения проверяют отдельно.

Для процесса и потока сигнальное состояние означает завершение. `WaitForSingleObject(handle, 0)` позволяет проверить его без блокировки; `WAIT_TIMEOUT` означает, что условие ещё не выполнено, а `WAIT_FAILED` требует проверки `GetLastError`. `WaitForMultipleObjects(..., TRUE, ...)` ждёт все объекты, `FALSE` — любой из них. Число одновременно ожидаемых дескрипторов ограничено `MAXIMUM_WAIT_OBJECTS`; для варианта с максимум восемью исполнителями это не проблема.

`GetExitCodeProcess` и `GetExitCodeThread` до завершения возвращают `STILL_ACTIVE`; после ожидания следует получить и вывести фактический код. Приоритет влияет на выбор потока планировщиком, но не ускоряет инструкции. В работе запрещены `REALTIME_PRIORITY_CLASS` и `THREAD_PRIORITY_TIME_CRITICAL`.

`SuspendThread` увеличивает счётчик приостановки и опасен как общий механизм синхронизации: поток может быть остановлен, удерживая ресурс. `ResumeThread` уменьшает этот счётчик; выполнение возможно при нуле. В отличие от них, ожидание не меняет состояние объекта, а ждёт его штатного завершения.

### Обязательные свойства программы

Программа проверяет входные данные и результат, сообщает ход работы, не оставляет созданных процессов или потоков и закрывает все созданные дескрипторы. При ошибке Windows API выводятся операция, `GetLastError()` и текст ошибки из `FormatMessageW`. Для каждого рабочего процесса нужны PID, номер, состояние и код завершения; для каждого потока — TID, номер, состояние и код завершения.

Время исследуемой операции измеряется высокоточным счётчиком. Для параллельного вычисления результат сравнивается с последовательным эталоном. Если вариант распараллеливается, каждый режим `K = 1, 2, 4, 8` запускается не менее трёх раз; в отчёт идёт среднее либо медиана. Размер данных выбирается так, чтобы различия времени были измеримы.

## 2. Контрольные вопросы: короткие ответы

1. **Чем процесс отличается от потока?** Процесс владеет изолированными ресурсами; поток исполняет код и разделяет ресурсы своего процесса с другими его потоками.
2. **Что такое PID и TID?** Числовые идентификаторы процесса и потока; это не `HANDLE`.
3. **Что такое дескриптор Windows?** `HANDLE` — средство обращения к системному объекту с правами доступа в текущем процессе.
4. **Что происходит после `CreateProcess`?** Создаются процесс и его первичный поток, им назначаются PID/TID, а вызывающему коду возвращается `PROCESS_INFORMATION`.
5. **Какие дескрипторы возвращает `CreateProcess`?** `hProcess` и `hThread`; оба нужно закрыть.
6. **Зачем `WaitForSingleObject`?** Ждать сигнального состояния одного объекта или окончания тайм-аута.
7. **Что означает `WAIT_TIMEOUT`?** Тайм-аут закончился, а условие ожидания не выполнено; это не ошибка.
8. **Как определить завершение процесса?** `WaitForSingleObject(hProcess, 0) == WAIT_OBJECT_0` или успешный `GetExitCodeProcess` с кодом не `STILL_ACTIVE`.
9. **Что такое код завершения процесса?** `DWORD`, с которым завершился процесс; его получают `GetExitCodeProcess` после завершения.
10. **Зачем `CloseHandle`?** Освобождает запись дескриптора; иначе возникает утечка ресурсов.
11. **Что разделяют потоки процесса?** Его адресное пространство, код, глобальные данные и открытые ресурсы. Стек и контекст у каждого свои.
12. **Что при аварии одного потока?** Необработанное исключение обычно завершает весь процесс; изоляции между его потоками нет.
13. **Что такое приоритет процесса?** Класс приоритета, участвующий в назначении приоритетов его потокам.
14. **Что такое приоритет потока?** Значение, которое вместе с классом процесса влияет на планирование этого потока.
15. **Почему высокий приоритет не гарантирует ускорение?** Он меняет доступ к CPU, но не число ядер, объём работы, ожидания, кэш и накладные расходы.
16. **Чем `SuspendThread` отличается от ожидания?** Первый принудительно ставит поток на паузу, второе ждёт его завершения и ничего не приостанавливает.
17. **Чем Fiber отличается от Thread?** Thread планирует ОС; Fiber переключает само приложение в контексте потока.
18. **Почему Fiber обычно не работают на нескольких ядрах?** Fiber одного потока выполняются по очереди; ОС распределяет ядра между потоками, а не Fiber.
19. **Почему больше потоков может замедлять программу?** Появляются переключения контекста, конкуренция за CPU и кэш, синхронизация; потоков может стать больше, чем ядер.
20. **Параллельность и конкурентность?** Параллельность — реальное одновременное исполнение на нескольких ядрах; конкурентность — чередование нескольких задач, возможное и на одном ядре.

## 3. Вариант 26 — «Process против Thread»

### Требование задания

Нужно реализовать одну вычислительную задачу двумя способами: `K` дочерними процессами и `K` потоками одного процесса. Исходные данные в обоих вариантах делятся **по одной и той же схеме**. Эксперимент проводится для `K = 1, 2, 4, 8`; сравниваются время выполнения, стоимость создания исполнителей, используемые ресурсы и сложность получения общего результата.

Задание не задаёт формулу, тип данных или канал обмена. Следовательно, вычисление следует выбрать независимым по частям, а границы частей для процессов и потоков сделать идентичными. Полученный общий результат должен быть сравнен с последовательным вычислением. Для потоков частичные результаты можно передать через общую память с корректной синхронизацией; для процессов потребуется явный межпроцессный обмен, например аргументы командной строки для входа и перенаправленный стандартный вывод, канал, файл или разделяемая память для результата. Выбор будет зафиксирован при реализации.

### Что должно быть в программе

Родительская часть формирует одинаковые диапазоны, запускает `K` исполнителей выбранного типа, сохраняет PID/TID и дескрипторы, ожидает окончание, получает коды завершения и объединяет частичные результаты. В варианте с процессами `CreateProcess` также создаёт первичный поток каждого ребёнка; его ненужный дескриптор следует сразу закрыть, оставив `hProcess` для ожидания. В варианте с потоками родитель ожидает дескрипторы рабочих потоков. Во всех случаях дескрипторы закрываются только после того, как больше не нужны.

Для честного сравнения отдельно фиксируются полное время «создание → вычисление → сбор результата» и стоимость создания: интервал от начала запуска исполнителей до создания последнего из них. Время непосредственно вычисления можно добавить, если архитектура его позволяет измерить. Ресурсы разумно представить числом созданных процессов, потоков и открытых дескрипторов, а также наблюдением потребления памяти в Диспетчере задач. В выводе нельзя заранее обещать, что потоки будут быстрее: результат зависит от машины, данных и выбранного IPC.

### Что показать в отчёте

Отчёт содержит цель, вариант, краткий алгоритм и схему взаимодействия; список API; существенные фрагменты кода; результаты и вывод. Для каждого `K` приводятся не менее трёх запусков и среднее или медиана. В сравнении нужно объяснить изоляцию процессов против общей памяти потоков, способ получения общего результата, различия по ресурсам и причину непропорционального ускорения при росте `K`.

| Критерий | Процессы | Потоки |
|---|---|---|
| Память и отказ | Отдельные адресные пространства; ошибка ребёнка обычно изолирована | Общая память; ошибка или гонка может повредить весь процесс |
| Обмен результатом | Нужен IPC | Возможна общая память, но нужна синхронизация |
| Создание | Новый процесс и первичный поток | Дополнительный поток внутри существующего процесса |
| Исполнение на ядрах | Планируются потоки процессов | Планируются рабочие потоки |

Перед сдачей: нет запрещённых приоритетов; результаты совпадают с последовательным эталоном; для каждого исполнителя выведены требуемые идентификатор, состояние и код завершения; пройдены все `K` и минимум три запуска.
