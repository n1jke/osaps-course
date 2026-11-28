#let flow(..steps) = {
  let items = steps.pos()
  align(center, stack(
    dir: ttb,
    spacing: 3pt,
    ..items.enumerate().map(((index, item)) => {
      let node = block(width: 78%, inset: 6pt, stroke: 0.6pt, align(center, item))
      if index == 0 { node } else { [↓ #linebreak() #node] }
    }),
  ))
}

#let message_path = flow(
  [Ввод пользователя или наступление срока таймера],
  [`GetMessage` получает сообщение для GUI-потока],
  [`WH_GETMESSAGE` проверяет событие и обновляет счётчики],
  [`CallNextHookEx` передаёт управление дальше],
  [`GetMessage` возвращает `MSG` циклу],
  [`DispatchMessage` вызывает оконную процедуру],
)

#let architecture = flow(
  [`options` проверяет параметры запуска],
  [`main` создаёт окно, кнопки и цикл сообщений],
  [`monitor` управляет замером и возвращает сводку в `main`],
  [`main` передаёт счётчики и буфер событий в `output`],
  [`output` записывает CSV и `lr6.log`],
)

#let measurement_flow = flow(
  [Старт. Подготовка счётчиков и установка перехватчика],
  [Запуск таймеров и фиксация начала замера],
  [Обновление счётчиков и накопление событий журнала в памяти],
  [Дедлайн, «Стоп» или закрытие окна],
  [Остановка учёта, снятие перехватчика и удаление таймеров],
  [Сохранение агрегатов в CSV и запись журнала],
)
