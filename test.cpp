#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>

// Struktura przechowująca dane o zapisanym prostokącie
struct RectangleData {
    HWND hwnd;
};

class RectApp {
    HINSTANCE m_instance;
    HWND m_main_window;
    HWND m_active_rect = nullptr;
    std::vector<RectangleData> m_saved_rects;
    POINT m_start_point = { 0, 0 };
    static std::wstring const s_class_name;

public:
    RectApp(HINSTANCE instance) : m_instance(instance), m_main_window(nullptr) {
        register_class();
        m_main_window = create_main_window();
    }

    // Rejestracja klasy okna - podobnie jak w notatkach [cite: 102-112]
    bool register_class() {
        WNDCLASSEXW desc{};
        if (GetClassInfoExW(m_instance, s_class_name.c_str(), &desc) != 0) return true;

        desc = {
            .cbSize = sizeof(WNDCLASSEXW),
            .lpfnWndProc = window_proc_static,
            .hInstance = m_instance,
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .hbrBackground = CreateSolidBrush(RGB(30, 50, 90)), // Kolor tła z treści zadania
            .lpszClassName = s_class_name.c_str()
        };
        return RegisterClassExW(&desc) != 0;
    }

    HWND create_main_window() {
        return CreateWindowExW(
            0, s_class_name.c_str(), L"Not WM_PAINT",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
            nullptr, nullptr, m_instance, this
        );
    }

    // Tworzenie prostokąta jako okna potomnego (static)
    HWND create_rect_window(int x, int y, int w, int h, COLORREF color) {
        HWND rect = CreateWindowExW(
            0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE,
            x, y, w, h,
            m_main_window, nullptr, m_instance, nullptr
        );
        
        // Ustawienie koloru poprzez podesłanie pędzla w WM_CTLCOLORSTATIC (obsłużone w proc)
        return rect;
    }

    int run(int show_command) {
        ShowWindow(m_main_window, show_command);
        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return (int)msg.wParam;
    }

private:
    static LRESULT CALLBACK window_proc_static(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
        RectApp* app = nullptr;
        if (msg == WM_NCCREATE) {
            app = static_cast<RectApp*>(reinterpret_cast<LPCREATESTRUCT>(lp)->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<RectApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        }
        return app ? app->window_proc(hWnd, msg, wp, lp) : DefWindowProcW(hWnd, msg, wp, lp);
    }

    LRESULT window_proc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_LBUTTONDOWN: {
            m_start_point.x = (short)LOWORD(lp);
            m_start_point.y = (short)HIWORD(lp);
            
            // Tworzymy "aktywny" prostokąt o rozmiarze 0x0
            m_active_rect = create_rect_window(m_start_point.x, m_start_point.y, 0, 0, RGB(170, 70, 80));
            SetCapture(hWnd); // Przechwytujemy mysz
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (m_active_rect) {
                int curr_x = (short)LOWORD(lp);
                int curr_y = (short)HIWORD(lp);

                // Obliczanie wymiarów (obsługa przeciągania w dowolną stronę)
                int x = std::min((int)m_start_point.x, curr_x);
                int y = std::min((int)m_start_point.y, curr_y);
                int w = std::abs(curr_x - (int)m_start_point.x);
                int h = std::abs(curr_y - (int)m_start_point.y);

                SetWindowPos(m_active_rect, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_active_rect) {
                m_saved_rects.push_back({ m_active_rect });
                m_active_rect = nullptr;
                ReleaseCapture();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            if (wp == VK_BACK && !m_saved_rects.empty()) {
                // Usuwanie ostatniego prostokąta
                DestroyWindow(m_saved_rects.back().hwnd);
                m_saved_rects.pop_back();
                // Wymuszenie odświeżenia tła zgodnie z instrukcją
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            // Każdy prostokąt (klasa STATIC) dostaje ten kolor
            static HBRUSH rectBrush = CreateSolidBrush(RGB(170, 70, 80));
            return (LRESULT)rectBrush;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wp, lp);
    }
};

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    RectApp app(hInst);
    return app.run(nShow);
}