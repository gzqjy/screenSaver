#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <child_id> <parent_id>\n", argv[0]);
        return 1;
    }
    Window child = strtol(argv[1], NULL, 16);
    Window parent = strtol(argv[2], NULL, 16);
    Display *d = XOpenDisplay(NULL);
    if (!d) return 1;
    XReparentWindow(d, child, parent, 0, 0);
    XMapWindow(d, child);
    XRaiseWindow(d, child);
    XFlush(d);
    XCloseDisplay(d);
    return 0;
}
