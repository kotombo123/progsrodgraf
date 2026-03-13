#include "app_tarcza.h"

#include <cmath>

const std::wstring app_tarcza::MAIN_CLASS = L"TarczaMain";
const std::wstring app_tarcza::CHILD_CLASS = L"TarczaChild";

double app_tarcza::initial_angle(int id)
{
    return id * 3.1415926535 / 5.0;
}

app_tarcza::app_tarcza(HINSTANCE instance)
{
    m_instance = instance;

    register_main();
    register_child();

    m_main = create_main();
}

bool app_tarcza::register_main()
{
    WNDCLASSEX wc{};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = main_proc_static;
    wc.hInstance = m_instance;
    wc.lpszClassName = MAIN_CLASS.c_str();
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    return RegisterClassEx(&wc);
}

bool app_tarcza::register_child()
{
    WNDCLASSEX wc{};

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = child_proc_static;
    wc.hInstance = m_instance;
    wc.lpszClassName = CHILD_CLASS.c_str();
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    return RegisterClassEx(&wc);
}

HWND app_tarcza::create_main()
{
    DWORD style =
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_MINIMIZEBOX;

    RECT r{0,0,CLIENT_SIZE,CLIENT_SIZE};

    AdjustWindowRect(&r, style, FALSE);

    HWND w = CreateWindowEx(
        0,
        MAIN_CLASS.c_str(),
        L"Tarcza",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        r.right - r.left,
        r.bottom - r.top,
        0,
        0,
        m_instance,
        this
    );

    return w;
}

void app_tarcza::create_children()
{
    for(int i=0;i<CHILD_COUNT;i++)
    {
        std::wstring title = std::to_wstring(i);

        CreateWindowEx(
            0,
            CHILD_CLASS.c_str(),
            title.c_str(),
            WS_CHILD | WS_VISIBLE | WS_CAPTION,
            0,
            0,
            CHILD_SIZE,
            CHILD_SIZE,
            m_main,
            (HMENU)i,
            m_instance,
            0
        );
    }

    layout_children();
}

void app_tarcza::layout_children()
{
    RECT rc;
    GetClientRect(m_main,&rc);

    int cx=(rc.right-rc.left)/2;
    int cy=(rc.bottom-rc.top)/2;

    for(int i=0;i<CHILD_COUNT;i++)
    {
        HWND child = GetDlgItem(m_main,i);

        double a = initial_angle(i)+m_delta;

        int x = cx + RADIUS*cos(a) - CHILD_SIZE/2;
        int y = cy + RADIUS*sin(a) - CHILD_SIZE/2;

        SetWindowPos(
            child,
            0,
            x,
            y,
            CHILD_SIZE,
            CHILD_SIZE,
            SWP_NOZORDER
        );
    }
}

LRESULT CALLBACK app_tarcza::main_proc_static(
    HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    app_tarcza* app;

    if(msg==WM_NCCREATE)
    {
        auto cs=(CREATESTRUCT*)lp;
        app=(app_tarcza*)cs->lpCreateParams;

        SetWindowLongPtr(
            w,
            GWLP_USERDATA,
            (LONG_PTR)app
        );
    }

    app=(app_tarcza*)GetWindowLongPtr(
        w,
        GWLP_USERDATA
    );

    if(app)
        return app->main_proc(w,msg,wp,lp);

    return DefWindowProc(w,msg,wp,lp);
}

LRESULT CALLBACK app_tarcza::child_proc_static(
    HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND parent=GetParent(w);

    auto app=(app_tarcza*)GetWindowLongPtr(
        parent,
        GWLP_USERDATA
    );

    if(app)
        return app->child_proc(w,msg,wp,lp);

    return DefWindowProc(w,msg,wp,lp);
}

LRESULT app_tarcza::main_proc(
    HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg)
    {

    case WM_CREATE:
        create_children();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(w,msg,wp,lp);
}

LRESULT app_tarcza::child_proc(
    HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg)
    {

    case WM_MOVE:
    {
        RECT rc;
        GetWindowRect(w,&rc);

        RECT parent;
        GetWindowRect(m_main,&parent);

        int cx=(parent.left+parent.right)/2;
        int cy=(parent.top+parent.bottom)/2;

        int mx=(rc.left+rc.right)/2;
        int my=(rc.top+rc.bottom)/2;

        double angle=atan2(
            my-cy,
            mx-cx
        );

        int id=GetWindowLongPtr(w,GWLP_ID);

        m_delta=angle-initial_angle(id);

        layout_children();

        return 0;
    }

    }

    return DefWindowProc(w,msg,wp,lp);
}

int app_tarcza::run(int show)
{
    ShowWindow(m_main,show);

    MSG msg;

    while(GetMessage(&msg,0,0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}