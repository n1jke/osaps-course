# ЛР №5. Графики и сборка отчёта

Команды выполняются в PowerShell из корня репозитория.

## Установить и проверить Typst

```powershell
winget install --id Typst.Typst --exact
```

Открой новый PowerShell и проверь установку.

```powershell
typst --version
```

Если `winget` недоступен, скачай архив для Windows с [официальной страницы Typst](https://typst.app/open-source/), распакуй его и добавь папку с `typst.exe` в `PATH`.

Отчёт использует шаблон `bsuir_typst`, расположенный в корне репозитория. Если папки нет, клонируй его и выбери версию, с которой готовился отчёт.

```powershell
if (-not (Test-Path .\bsuir_typst)) {
    git clone https://github.com/artemious3/bsuir_typst.git bsuir_typst
    git -C bsuir_typst checkout b9e186318f94191086e919e624a1e710bf5db0f7
}
```

Установи шрифты из `bsuir_typst/README.md` и проверь их доступность.

```powershell
typst fonts | Select-String 'Times New Roman|Courier New|TeX Gyre Termes Math|XITS Math'
```

## Перестроить графики

Нужны Python и matplotlib.

```powershell
py --version
py -m pip install matplotlib
```

Укажи папку, где лежат 24 CSV одной серии, и каталог, в который сохранить графики. В отчёте сейчас выбрана серия `measurements-1`.

```powershell
$inputDir = "lr-5/measurements-1/lr5"
$outputDir = "lr-5/report-assets/measurements-1"
py lr-5/scripts/plot_results.py $inputDir $outputDir
```

Для другой серии замени обе переменные на нужные пути. Скрипт ожидает три запуска по 100 попыток для каждой пары `GET`/`HEAD` и `empty`/`small`/`large`/`missing`. Он создаёт `attempt-time.png`, `received-bytes.png` и `summary.csv` в выходном каталоге и перезаписывает эти три файла при повторном запуске. Исходные CSV не меняются.

В отчёт включены графики по путям `lr-5/report-assets/measurements-1/attempt-time.png` и `received-bytes.png`. Чтобы включить другую серию, измени оба пути в `lr-5/report.typ` и обнови соответствующие таблицы и анализ по её данным.

## Собрать PDF

```powershell
typst compile --root . lr-5/report.typ lr-5/report.pdf
```

`--root .` разрешает отчёту читать шаблон и файлы проекта из корня. Для сборки должны присутствовать `bsuir_typst`, `lr-5/report-assets`, `lr-5/scripts`, исходники клиента и сервера. PDF сохраняется в `lr-5/report.pdf`. Перед сдачей заполни титульник и раздел об ИИ, проверь актуальность версии и результаты ручных проверок.
