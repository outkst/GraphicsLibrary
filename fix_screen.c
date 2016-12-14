#include <termios.h>        /* TCGETS TCSETS */
#include <sys/ioctl.h>      /* ioctl() */

/*
    Fixes the terminal settings back to allow ICANON and ECHO.

    These flags give the terminal an interactive view to the user, and
    when switched off the terminal does not show what the user has typed
    (no ECHO) and does not require the ENTER key to be used to send input
    to the device (no ICANON).
*/
int main() {
    struct termios terminal_settings;
    ioctl(1, TCGETS, &terminal_settings);       // get terminal settings
    terminal_settings.c_lflag |= ICANON;        // set ICANON flag
    terminal_settings.c_lflag |= ECHO;          // set ECHO flag
    ioctl(1, TCSETS, &terminal_settings);       // set terminal settings

    return 0;
}