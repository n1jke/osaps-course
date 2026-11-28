# ЛР №6. Сборка отчёта

Команды выполняются в PowerShell из корня репозитория.

## 1. Установить и проверить Typst

```powershell
winget install --id Typst.Typst --exact
```

Открой новый PowerShell и проверь установку:

```powershell
typst --version
typst fonts | Select-String 'Times New Roman|Courier New|TeX Gyre Termes Math|XITS Math'
```

Шаблон отчёта расположен в `bsuir_typst` в корне проекта. Шрифты перечислены в `bsuir_typst/README.md`.

## 2. Построить графики из выбранных CSV

Запусти из корня репозитория с Python 3:

```powershell
py lr-6/scripts/plot_results.py lr-6/csv lr-6/report-assets
```

Скрипт читает девять файлов `test-1-1.csv` … `test-3-3.csv` и создаёт `timer-frequency.svg`, `message-counts.svg`, `runs.csv` и `summary.csv`. Для другой серии укажи её папку вместо `lr-6/csv`.

## 3. Собрать PDF

```powershell
typst compile --root . lr-6/report.typ lr-6/report.pdf
```

Перед сдачей заполни приложение об использовании ИИ.
