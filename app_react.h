#pragma once
#include <windows.h>
#include <vector>
#include <string>

class app_rect
{
private:
    // Rejestracja klasy okna i tworzenie okna głównego
    bool register_class();
    HWND create_main_window();
    
    // Metoda pomocnicza do tworzenia prostokątów jako okien STATIC
    HWND create_rect_child(int x, int y, int w, int h);

    // Statyczna i instancyjna procedura okna
    static LRESULT CALLBACK window_proc_static(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

    HINSTANCE m_instance;
    HWND m_main;
    HWND m_active_rect; // Obecnie rysowany prostokąt
    POINT m_start_point; // Punkt początkowy kliknięcia
    std::vector<HWND> m_rects; // Kontener na zapisane prostokąty
    
    static std::wstring const s_class_name;
    HBRUSH m_rect_brush; // Pędzel do koloru prostokątów

public:
    app_rect(HINSTANCE instance);
    ~app_rect(); // Destruktor do zwalniania zasobów
    int run(int show_command);
};