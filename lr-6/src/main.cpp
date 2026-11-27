#include "monitor.h"
#include "output.h"

#include <commdlg.h>

#include <iomanip>
#include <new>
#include <sstream>
#include <utility>
#include <vector>

namespace {

enum Control { frequency = 100, duration, start, stop, test, save, summary };

HWND child(HWND window, Control id)
{
    return GetDlgItem(window, id);
}

void set_label(HWND window, const std::wstring& text)
{
    if (!SetWindowText(child(window, summary), text.c_str())) {
        throw output::Error{L"SetWindowText", GetLastError()};
    }
}

void update_controls(HWND window)
{
    const bool active = monitor::active();
    const bool ready = monitor::can_start();
    for (const auto id : {frequency, duration, start}) {
        EnableWindow(child(window, id), ready);
    }
    EnableWindow(child(window, stop), active);
    EnableWindow(child(window, test), active);
    EnableWindow(child(window, save), !active && monitor::result().valid);
}

std::wstring run_id()
{
    return std::to_wstring(monitor::result().id);
}

struct Event {
    SYSTEMTIME time;
    const wchar_t* operation;
    std::wstring object;
    const wchar_t* status;
    DWORD code;
};

std::vector<Event> events;
std::size_t written_events = 0;

void remember(const wchar_t* operation, std::wstring object, const wchar_t* status,
              DWORD code = 0)
{
    SYSTEMTIME time{};
    GetLocalTime(&time);
    events.push_back({time, operation, std::move(object), status, code});
}

void write_events()
{
    while (written_events < events.size()) {
        const auto& event = events[written_events];
        output::log(event.operation, event.object, event.status, event.code, &event.time);
        ++written_events;
    }
    events.clear();
    written_events = 0;
}

std::wstring csv_filename()
{
    return L"test_" + run_id() + L".csv";
}

void save_result(const std::wstring& path, bool replace);

void show_error(HWND window, const output::Error& error)
{
    auto message = output::describe(error);
    try {
        if (monitor::active()) {
            remember(error.operation, run_id(), L"error", error.code);
        } else {
            output::log(error.operation, run_id(), L"error", error.code);
        }
    } catch (const output::Error& log_error) {
        message += L"\n" + output::describe(log_error);
    }
    if (monitor::active()) {
        SetWindowText(child(window, summary), message.c_str());
    } else {
        MessageBox(window, message.c_str(), L"ЛР №6", MB_OK | MB_ICONERROR);
    }
}

void add(HWND window, const wchar_t* type, const wchar_t* caption, DWORD style,
         int x, int y, int width, int height, int id = 0)
{
    const auto control = CreateWindowEx(0, type, caption, WS_CHILD | WS_VISIBLE | style,
                                        x, y, width, height, window,
                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                        GetModuleHandle(nullptr), nullptr);
    if (!control) {
        throw output::Error{L"CreateWindowEx (элемент окна)", GetLastError()};
    }
    SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(
                    GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

void create_controls(HWND window)
{
    add(window, L"STATIC", L"Частота, срабатываний/с", 0, 16, 12, 180, 20);
    add(window, L"STATIC", L"Длительность, с", 0, 215, 12, 150, 20);
    add(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
        16, 35, 170, 130, frequency);
    for (const auto* value : {L"10", L"50", L"100"}) {
        if (SendMessage(child(window, frequency), CB_ADDSTRING, 0,
                        reinterpret_cast<LPARAM>(value)) < 0) {
            throw output::Error{L"CB_ADDSTRING", ERROR_NOT_ENOUGH_MEMORY};
        }
    }
    if (SendMessage(child(window, frequency), CB_SETCURSEL, 0, 0) == CB_ERR) {
        throw output::Error{L"CB_SETCURSEL", ERROR_INVALID_DATA};
    }
    add(window, L"EDIT", L"30", WS_BORDER | ES_NUMBER, 215, 35, 100, 24, duration);
    SendMessage(child(window, duration), EM_SETLIMITTEXT, 3, 0);
    add(window, L"BUTTON", L"Старт", 0, 16, 76, 92, 28, start);
    add(window, L"BUTTON", L"Стоп", 0, 116, 76, 92, 28, stop);
    add(window, L"BUTTON", L"Тест", 0, 216, 76, 92, 28, test);
    add(window, L"BUTTON", L"Сохранить CSV", 0, 316, 76, 132, 28, save);
    add(window, L"STATIC", L"Готово к замеру", SS_LEFT, 16, 124, 432, 115, summary);
    update_controls(window);
}

void start_run(HWND window)
{
    BOOL parsed = FALSE;
    const UINT seconds = GetDlgItemInt(window, duration, &parsed, FALSE);
    if (!parsed || seconds < 1 || seconds > monitor::max_seconds) {
        set_label(window, L"Укажи длительность от 1 до 300 секунд.");
        return;
    }
    const LRESULT index = SendMessage(child(window, frequency), CB_GETCURSEL, 0, 0);
    if (index < 0 || index >= 3) {
        throw output::Error{L"CB_GETCURSEL", ERROR_INVALID_DATA};
    }
    constexpr unsigned rates[] = {10, 50, 100};
    write_events();
    set_label(window, L"Идёт замер…");
    monitor::start(window, rates[index], seconds);
    update_controls(window);
    remember(L"START", run_id(), L"ok");
}

void stop_run(HWND window)
{
    try {
        monitor::stop();
    } catch (const output::Error&) {
        write_events();
        throw;
    }
    update_controls(window);
    const auto& run = monitor::result();
    if (!run.valid) {
        remember(L"STOP", run_id(), L"empty");
        write_events();
        set_label(window, L"Замер не содержит учтённого интервала.");
        return;
    }
    const auto elapsed = static_cast<double>(run.end - run.start) / run.clock_hz;
    std::wostringstream view;
    view << (run.completed ? L"Замер завершён" : L"Замер остановлен досрочно")
         << L"\r\nДлительность: " << std::fixed << std::setprecision(3) << elapsed
         << L" с. Сообщений: " << run.total
         << L"\r\nУчебный таймер: " << run.timer_total << L" событий, "
         << std::setprecision(2) << run.timer_total / elapsed
         << L" срабатываний/с при заданных " << run.hz
         << L"\r\nТестовых сообщений отправлено: " << run.posted;
    set_label(window, view.str());
    remember(L"STOP", run_id(), run.completed ? L"completed" : L"stopped");
    const auto path = csv_filename();
    try {
        save_result(path, false);
    } catch (const output::Error& error) {
        write_events();
        throw;
    }
    write_events();
    set_label(window, view.str() + L"\r\nCSV: " + path);
}

void export_run(HWND window)
{
    wchar_t path[4096]{};
    const auto filename = csv_filename();
    filename.copy(path, filename.size());
    OPENFILENAME dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFile = path;
    dialog.nMaxFile = sizeof(path) / sizeof(path[0]);
    dialog.lpstrFilter = L"CSV (*.csv)\0*.csv\0";
    dialog.lpstrDefExt = L"csv";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileName(&dialog)) {
        const DWORD error = CommDlgExtendedError();
        if (error) {
            throw output::Error{L"GetSaveFileName", error};
        }
        return;
    }
    save_result(path, true);
    write_events();
    const auto message = std::wstring(L"Сохранено в ") + path;
    MessageBox(window, message.c_str(), L"ЛР №6", MB_OK);
}

void save_result(const std::wstring& path, bool replace)
{
    LARGE_INTEGER begin{}, end{};
    if (!QueryPerformanceCounter(&begin)) {
        throw output::Error{L"QueryPerformanceCounter", GetLastError()};
    }
    output::save_csv(path, monitor::result(), replace);
    if (!QueryPerformanceCounter(&end)) {
        throw output::Error{L"QueryPerformanceCounter", GetLastError()};
    }
    const double ms = 1000.0 * (end.QuadPart - begin.QuadPart) / monitor::result().clock_hz;
    remember(L"EXPORT", run_id() + L" path=" + path +
             L" elapsed_ms=" + std::to_wstring(ms), L"ok");
}

LRESULT CALLBACK process(HWND window, UINT message, WPARAM wp, LPARAM lp)
{
    try {
        switch (message) {
        case WM_CREATE:
            create_controls(window);
            return 0;
        case WM_COMMAND:
            if (HIWORD(wp) == BN_CLICKED && lp != 0) {
                switch (LOWORD(wp)) {
                case start:
                    if (monitor::can_start()) {
                        start_run(window);
                    }
                    return 0;
                case stop:
                    if (monitor::active()) {
                        stop_run(window);
                    }
                    return 0;
                case test:
                    if (monitor::active()) {
                        const auto before = monitor::result().posted;
                        try {
                            monitor::post_test();
                        } catch (const output::Error& error) {
                            remember(L"TEST", run_id() + L" posted=" +
                                std::to_wstring(monitor::result().posted - before),
                                L"error", error.code);
                            throw;
                        }
                        remember(L"TEST", run_id() + L" posted=" +
                            std::to_wstring(monitor::result().posted - before), L"ok");
                        set_label(window, L"Идёт замер… Тестовых сообщений отправлено: " +
                                          std::to_wstring(monitor::result().posted));
                    }
                    return 0;
                case save:
                    if (!monitor::active() && monitor::result().valid) {
                        export_run(window);
                    }
                    return 0;
                }
            }
            break;
        case monitor::test_message:
            return 0;
        case WM_TIMER:
            if (monitor::deadline_reached()) {
                stop_run(window);
            }
            return 0;
        case WM_CLOSE:
            if (monitor::active()) {
                try {
                    stop_run(window);
                } catch (const output::Error& error) {
                    show_error(window, error);
                }
            }
            try {
                monitor::stop();
            } catch (const output::Error& error) {
                show_error(window, error);
            }
            if (!DestroyWindow(window)) {
                throw output::Error{L"DestroyWindow", GetLastError()};
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    } catch (const output::Error& error) {
        show_error(window, error);
        if (message != WM_CREATE) {
            update_controls(window);
        }
    } catch (const std::bad_alloc&) {
        if (monitor::active()) {
            SetWindowText(child(window, summary), L"Недостаточно памяти.");
        } else {
            MessageBox(window, L"Недостаточно памяти.", L"ЛР №6", MB_OK | MB_ICONERROR);
        }
    }
    if (message == WM_CREATE) {
        return -1;
    }
    return DefWindowProc(window, message, wp, lp);
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    HWND window = nullptr;
    try {
        WNDCLASS cls{};
        cls.lpfnWndProc = process;
        cls.hInstance = instance;
        cls.lpszClassName = L"Lab6Window";
        cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!cls.hCursor) {
            throw output::Error{L"LoadCursor", GetLastError()};
        }
        cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        if (!RegisterClass(&cls)) {
            throw output::Error{L"RegisterClass", GetLastError()};
        }

        window = CreateWindowEx(0, cls.lpszClassName, L"ЛР №6 — Оконные сообщения",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 485, 280,
                                nullptr, nullptr, instance, nullptr);
        if (!window) {
            throw output::Error{L"CreateWindowEx", GetLastError()};
        }
        ShowWindow(window, show);

        MSG msg{};
        for (;;) {
            const BOOL received = GetMessage(&msg, nullptr, 0, 0);
            if (received == -1) {
                throw output::Error{L"GetMessage", GetLastError()};
            }
            if (!received) {
                return 0;
            }
            if (monitor::deadline_reached()) {
                try {
                    stop_run(window);
                } catch (const output::Error& error) {
                    update_controls(window);
                    show_error(window, error);
                }
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    } catch (const output::Error& error) {
        try {
            monitor::stop();
        } catch (const output::Error& cleanup) {
            show_error(window, cleanup);
        }
        show_error(window, error);
        if (window) {
            DestroyWindow(window);
        }
        return 1;
    }
}
