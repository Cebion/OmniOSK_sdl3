#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static Window find_window(Display *display, Window root, const char *title)
{
    Window window;
    Window parent;
    Window *children = NULL;
    unsigned int child_count = 0;
    unsigned int index;
    char *name = NULL;

    if (XFetchName(display, root, &name) != 0) {
        int matches = strncmp(name, title, strlen(title)) == 0;
        XFree(name);
        if (matches) {
            return root;
        }
    }
    if (XQueryTree(display, root, &window, &parent, &children, &child_count) == 0) {
        return 0;
    }
    for (index = 0; index < child_count; ++index) {
        Window found = find_window(display, children[index], title);
        if (found != 0) {
            XFree(children);
            return found;
        }
    }
    if (children != NULL) {
        XFree(children);
    }
    return 0;
}

int main(int argc, char **argv)
{
    Display *display;
    Window window;
    KeySym symbol;
    KeyCode code;
    if (argc != 3) {
        fprintf(stderr, "usage: %s WINDOW_TITLE KEY\n", argv[0]);
        return 2;
    }
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "unable to open X display\n");
        return 77;
    }
    window = find_window(display, DefaultRootWindow(display), argv[1]);
    symbol = XStringToKeysym(argv[2]);
    code = XKeysymToKeycode(display, symbol);
    if (window == 0 || symbol == NoSymbol || code == 0) {
        fprintf(stderr, "window or key not found\n");
        XCloseDisplay(display);
        return 1;
    }
    XRaiseWindow(display, window);
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XSync(display, False);
    XTestFakeKeyEvent(display, code, True, CurrentTime);
    XTestFakeKeyEvent(display, code, False, CurrentTime);
    XSync(display, False);
    usleep(20000);
    XCloseDisplay(display);
    return 0;
}
