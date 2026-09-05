/*
 * conio.h compatibility shim for macOS / Unix.
 *
 * Many beginner C/C++ textbooks written for Windows Dev-C++ use
 * getch()/getche()/kbhit() from <conio.h>. This header provides
 * the commonly used subset on top of termios so that such programs
 * compile and behave the same on macOS.
 *
 * Installed with Red Panda C++ (macOS) into <app>/Contents/Resources/compat/include.
 */
#ifndef REDPANDA_COMPAT_CONIO_H
#define REDPANDA_COMPAT_CONIO_H

#if defined(__APPLE__) || defined(__unix__)

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int _conio_read_key(int echo) {
    struct termios oldt, newt;
    int ch;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0)
        return getchar();
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (echo && ch != EOF) {
        putchar(ch);
        fflush(stdout);
    }
    return ch;
}

/* Read one key without echo. */
static inline int getch(void) { return _conio_read_key(0); }
static inline int _getch(void) { return _conio_read_key(0); }

/* Read one key with echo. */
static inline int getche(void) { return _conio_read_key(1); }
static inline int _getche(void) { return _conio_read_key(1); }

/* Return non-zero if a key is waiting. */
static inline int kbhit(void) {
    struct termios oldt, newt;
    fd_set fds;
    struct timeval tv;
    int ready;
    if (tcgetattr(STDIN_FILENO, &oldt) != 0)
        return 0;
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ready > 0;
}
static inline int _kbhit(void) { return kbhit(); }

/* Clear the screen (ANSI). */
static inline void clrscr(void) {
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}

/* Move cursor to column x, row y (1-based, like Turbo C). */
static inline void gotoxy(int x, int y) {
    printf("\033[%d;%dH", y, x);
    fflush(stdout);
}

#ifdef __cplusplus
}
#endif

#endif /* __APPLE__ || __unix__ */

#endif /* REDPANDA_COMPAT_CONIO_H */
