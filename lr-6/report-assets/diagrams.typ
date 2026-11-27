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
  [Пользовательский ввод или истечение периода таймера],
  [Получение сообщения GUI-потоком через `GetMessage`],
  [Вызов `WH_GETMESSAGE` и обновление счётчиков],
  [Возврат `MSG` и передача через `DispatchMessage`],
  [Оконная процедура окна-получателя],
)

#let measurement_flow = flow(
  [Старт. Подготовка счётчиков и установка перехватчика],
  [Запуск таймеров и фиксация начала замера],
  [Учёт сообщений по типам и секундным интервалам],
  [Дедлайн, «Стоп» или закрытие окна],
  [Остановка учёта, снятие перехватчика и удаление таймеров],
  [Показ сводки и автоматическое сохранение CSV],
)
