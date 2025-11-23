#include <menu.hpp>
#include <nvs_flash.h>

const char SERIAL_HELP_MENU[] PROGMEM = \
"\n\n[HELP] \n"
"Welcome to SchoolBox serial interface. From here you can configure the device. \n"
"Press one of the following keys to activate the commands. \n"
"0. Show this menu. \n"
"1. Reset FLASH (will reset authentication and will reset wifi credentials to original value from firmware). \n";

Menu::Menu() {}

void Menu::loop() {
    int read = Serial.read();
    if (read == -1) return;

    switch (read) {
        case '0':
            this->showMenu();
        case '1':
            this->resetFlash();
            break;

        default:
            break;
    }
}

void Menu::showMenu() {
    Serial.println(SERIAL_HELP_MENU);
}
void Menu::resetFlash() {
    Serial.println("[MENU] Resetting flash...");
    nvs_flash_erase();
    nvs_flash_init();
    Serial.println("[MENU] Flash reset complete. Please restart.");
    while (true);
}