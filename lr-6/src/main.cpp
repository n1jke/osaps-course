#include "monitor.h"
#include "options.h"
#include "output.h"

#include <shellapi.h>

#include <iomanip>
#include <memory>
#include <new>
#include <sstream>
#include <utility>
#include <vector>

namespace {

enum Control { start = 100, stop, test, summary };

Options options;

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
    EnableWindow(child(window, start), monitor::can_start());
    EnableWindow(child(window, stop), active);
    EnableWindow(child(window, test), active);
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
    return options.csv.empty() ? L"test_" + run_id() + L".csv" : options.csv;
}

void save_result(const std::wstring& path);

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
    const auto settings = L"Частота: " + std::to_wstring(options.hz) +
                          L" срабатываний/с. Длительность: " +
                          std::to_wstring(options.seconds) + L" с.";
    add(window, L"STATIC", settings.c_str(), 0, 16, 12, 432, 24);
    add(window, L"BUTTON", L"Старт", 0, 16, 46, 92, 28, start);
    add(window, L"BUTTON", L"Стоп", 0, 116, 46, 92, 28, stop);
    add(window, L"BUTTON", L"Тест", 0, 216, 46, 92, 28, test);
    add(window, L"STATIC", L"Готово к замеру", SS_LEFT, 16, 92, 432, 130, summary);
    update_controls(window);
}

void start_run(HWND window)
{
    write_events();
    set_label(window, L"Идёт замер…");
    monitor::start(window, options.hz, options.seconds);
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
        save_result(path);
    } catch (const output::Error& error) {
        write_events();
        throw;
    }
    write_events();
    set_label(window, view.str() + L"\r\nCSV: " + path);
}

void save_result(const std::wstring& path)
{
    LARGE_INTEGER begin{}, end{};
    if (!QueryPerformanceCounter(&begin)) {
        throw output::Error{L"QueryPerformanceCounter", GetLastError()};
    }
    output::save_csv(path, monitor::result());
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
        int count = 0;
        const std::unique_ptr<wchar_t*, decltype(&LocalFree)> argv(
            CommandLineToArgvW(GetCommandLineW(), &count), &LocalFree);
        if (!argv) {
            throw output::Error{L"CommandLineToArgvW", GetLastError()};
        }
        std::vector<std::wstring_view> arguments;
        for (int i = 1; i < count; ++i) {
            arguments.emplace_back(argv.get()[i]);
        }
        if (!parse_options(arguments, options)) {
            const auto message = std::wstring(L"Неверные параметры запуска.\n") + usage;
            MessageBox(nullptr, message.c_str(), L"ЛР №6", MB_OK | MB_ICONERROR);
            return 2;
        }

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
