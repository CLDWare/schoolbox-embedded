#ifndef MENU_HPP
#define MENU_HPP

#include <Arduino.h>

extern const char SERIAL_HELP_MENU[];

class Menu {
    public:
        Menu();
        void loop();
        static void showMenu();
    private:
        void resetFlash();
};

#endif