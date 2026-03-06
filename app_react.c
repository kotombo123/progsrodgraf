#include "app_rect.h"
#include <algorithm> // Dla std::min

std::wstring const app_rect::s_class_name{ L"RectWindowClass" }; 

app_rect::app_rect(HINSTANCE instance) 
    : m_instance{ instance }, m_main{}, m_active_rect{ nullptr }, m_start_point{ 0, 0 } 
{
    m_rect_brush = CreateSolidBrush(RGB(170, 70, 80)); // Kolor z treści zadania [cite: 390]
    register_class(); 
    m_main = create_main_window();
}

app_rect::~app_rect() {
    DeleteObject(m_rect_brush); // Pamiętaj o zwalnianiu zasobów! [cite: 20, 835]
}

bool app_rect::register_class() {
    WNDCLASSEXW desc{}; 
    if (GetClassInfoExW(m_instance, s_class_name.c_str(), &desc) != 0) return true;

    desc = {
        .cbSize = sizeof(WNDCLASSEXW), 
        .lpfnWndProc = window_proc_static, 
        .hInstance = m_instance, 
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .hbrBackground = CreateSolidBrush(RGB(30, 50, 90)), // Kolor tła okna głównego
        .lpszClassName = s_class_name.c_str() 
    };
    return RegisterClassExW(&desc) != 0; 
}

HWND app_rect::create_main_window() {
    return CreateWindowExW(
        0, s_class_name.c_str(), L"Not WM_PAINT", 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, 800, 600,
        nullptr, nullptr, m_instance, this 
    );
}

HWND app_rect::create_rect_child(int x, int y, int w, int h) {
    return CreateWindowExW(
        0, L"STATIC", nullptr, 
        WS_CHILD | WS_VISIBLE, 
        x, y, w, h, 
        m_main, nullptr, m_instance, nullptr 
    );
}

int app_rect::run(int show_command) {
    ShowWindow(m_main, show_command); 
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) { 
        TranslateMessage(&msg); 
        DispatchMessageW(&msg); 
    }
    return EXIT_SUCCESS; 
}

LRESULT CALLBACK app_rect::window_proc_static(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    app_rect* app = nullptr;
    if (message == WM_NCCREATE) { 
        app = static_cast<app_rect*>(reinterpret_cast<LPCREATESTRUCTW>(lparam)->lpCreateParams); 
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app)); 
    } else {
        app = reinterpret_cast<app_rect*>(GetWindowLongPtrW(window, GWLP_USERDATA)); 
    }
    return app ? app->window_proc(window, message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam); 
}

LRESULT app_rect::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_LBUTTONDOWN:
        m_start_point.x = (short)LOWORD(lparam);
        m_start_point.y = (short)HIWORD(lparam);
        m_active_rect = create_rect_child(m_start_point.x, m_start_point.y, 0, 0);
        SetCapture(window);
        return 0;

    case WM_MOUSEMOVE:
        if (m_active_rect) {
            int curr_x = (short)LOWORD(lparam);
            int curr_y = (short)HIWORD(lparam);
            
            // Obliczanie współrzędnych tak, by móc rysować w każdym kierunku
            int x = std::min((int)m_start_point.x, curr_x);
            int y = std::min((int)m_start_point.y, curr_y);
            int w = std::abs(curr_x - (int)m_start_point.x);
            int h = std::abs(curr_y - (int)m_start_point.y);

            SetWindowPos(m_active_rect, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE); [cite: 478]
        }
        return 0;

    case WM_LBUTTONUP:
        if (m_active_rect) {
            m_rects.push_back(m_active_rect);
            m_active_rect = nullptr;
            ReleaseCapture();
        }
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_BACK && !m_rects.empty()) { // Obsługa Backspace
            DestroyWindow(m_rects.back());
            m_rects.pop_back();
            InvalidateRect(window, nullptr, TRUE); // Odśwież tło
        }
        return 0;

    case WM_CTLCOLORSTATIC: // Ustawianie koloru prostokątów [cite: 334, 401]
        return (LRESULT)m_rect_brush; 

    case WM_DESTROY:
        PostQuitMessage(EXIT_SUCCESS); 
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}