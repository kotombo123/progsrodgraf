#include "app_rect.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command) {
    app_rect app{ instance }; 
    return app.run(show_command); 
}