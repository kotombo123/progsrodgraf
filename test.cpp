#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <windows.h>
#include <windowsx.h> // Wymagane dla GET_X_LPARAM i GET_Y_LPARAM

class AppRectangles {
private:
    HINSTANCE m_instance;
    HWND m_main;
    static std::wstring const s_class_name;

    // Pędzle do tła i prostokątów
    HBRUSH m_bgBrush;
    HBRUSH m_rectBrush;

    // Stan rysowania i wektor uchwytów utworzonych prostokątów
    std::vector<HWND> m_rects;
    bool m_isDrawing = false;
    int m_startX = 0;
    int m_startY = 0;

    bool register_class() {
        WNDCLASSEXW desc{};
        if (GetClassInfoExW(m_instance, s_class_name.c_str(), &desc) != 0)
            return true;

        desc = { .cbSize = sizeof(WNDCLASSEXW),
                .lpfnWndProc = window_proc_static,
                .hInstance = m_instance,
                .hCursor = LoadCursorW(nullptr, L"IDC_ARROW"),
                .hbrBackground = m_bgBrush, // Tło głównego okna
                .lpszClassName = s_class_name.c_str() };
        return RegisterClassExW(&desc) != 0;
    }

    HWND create_window() {
        // Okno o stałym rozmiarze: bez WS_THICKFRAME (zmiana rozmiaru) i
        // WS_MAXIMIZEBOX Używamy WS_CLIPCHILDREN aby zredukować migotanie przy
        // zmianie rozmiaru dzieci
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
            WS_CLIPCHILDREN;

        // Obliczamy rozmiar całego okna, tak aby obszar roboczy (client area)
        // wynosił równo 800x600
        RECT size{ 0, 0, 800, 600 };
        AdjustWindowRectEx(&size, style, false, 0);

        return CreateWindowExW(0, s_class_name.c_str(), L"Not WM_PAINT", style,
            CW_USEDEFAULT, 0, size.right - size.left,
            size.bottom - size.top, nullptr, nullptr, m_instance,
            this);
    }

    static LRESULT CALLBACK window_proc_static(HWND window, UINT message,
        WPARAM wparam, LPARAM lparam) {
        AppRectangles* app = nullptr;
        if (message == WM_NCCREATE) {
            auto p = reinterpret_cast<LPCREATESTRUCTW>(lparam);
            app = static_cast<AppRectangles*>(p->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        }
        else {
            app = reinterpret_cast<AppRectangles*>(
                GetWindowLongPtrW(window, GWLP_USERDATA));
        }

        if (app != nullptr) {
            return app->window_proc(window, message, wparam, lparam);
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_LBUTTONDOWN: {
            m_isDrawing = true;

            // Zapisujemy punkt początkowy (narożnik)
            m_startX = GET_X_LPARAM(lparam);
            m_startY = GET_Y_LPARAM(lparam);

            // Tworzymy nowy prostokąt (statyczną kontrolkę) o zerowych wymiarach
            HWND newRect = CreateWindowExW(
                0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE, m_startX, m_startY, 0,
                0, window, nullptr, m_instance, nullptr);

            m_rects.push_back(newRect);

            // Przechwytujemy myszkę, aby śledzić ją nawet gdy opuści okno klienta
            SetCapture(window);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (m_isDrawing && !m_rects.empty()) {
                int currentX = GET_X_LPARAM(lparam);
                int currentY = GET_Y_LPARAM(lparam);

                // Obliczanie położenia lewego górnego rogu oraz wymiarów
                int x = std::min(m_startX, currentX);
                int y = std::min(m_startY, currentY);
                int w = std::abs(currentX - m_startX);
                int h = std::abs(currentY - m_startY);

                // Aktualizacja aktywnego prostokąta
                SetWindowPos(m_rects.back(), nullptr, x, y, w, h,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (m_isDrawing) {
                m_isDrawing = false;
                ReleaseCapture(); // Koniec śledzenia myszki
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (wparam == VK_BACK) {
                if (!m_rects.empty()) {
                    // Niszczymy uchwyt ostatniego okienka-prostokąta
                    DestroyWindow(m_rects.back());
                    m_rects.pop_back();

                    // W przypadku, gdy użytkownik usunął w trakcie rysowania
                    if (m_isDrawing) {
                        m_isDrawing = false;
                        ReleaseCapture();
                    }

                    // Odświeżenie okna matki, aby usunąć wizualne pozostałości
                    InvalidateRect(window, nullptr, TRUE);
                }
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            // Ta wiadomość pozwala nam zmienić kolor statycznej kontrolki
            // (prostokąta)
            return reinterpret_cast<INT_PTR>(m_rectBrush);
        }
        case WM_DESTROY: {
            PostQuitMessage(EXIT_SUCCESS);
            return 0;
        }
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

public:
    AppRectangles(HINSTANCE instance) : m_instance{ instance }, m_main{} {
        // Zgodnie z zadaniem tworzymy unikalne kolory
        m_bgBrush = CreateSolidBrush(RGB(30, 50, 90));
        m_rectBrush = CreateSolidBrush(RGB(170, 70, 80));

        register_class();
        m_main = create_window();
    }

    ~AppRectangles() {
        // Należy pamiętać o zarządzaniu zasobami i zwolnieniu pędzli
        DeleteObject(m_bgBrush);
        DeleteObject(m_rectBrush);
    }

    int run(int show_command) {
        ShowWindow(m_main, show_command);
        MSG msg{};
        BOOL result = TRUE;
        while ((result = GetMessageW(&msg, nullptr, 0, 0)) != 0) {
            if (result == -1)
                return EXIT_FAILURE;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return EXIT_SUCCESS;
    }
};

std::wstring const AppRectangles::s_class_name{ L"Not_WM_PAINT_Class" };

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command) {
    AppRectangles app{ instance };
    return app.run(show_command);
}
