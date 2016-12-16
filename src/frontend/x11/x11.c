#include "../../gbe.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>

#include "../frontend.h"

#include "../../window.h"
#include "../../display.h"
#include "../../input.h"
#include "../../gfx/gl.h"

#define WINDOW_HEIGHT 288
#define WINDOW_WIDTH 320

Display *display;
Window window;
int screen;
XEvent event;
XSetWindowAttributes setWindowAttributes;
Window root;
GLint attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
XVisualInfo *visualInfo;
Colormap colormap;
GLXContext glContext;
GLuint textureID;

static void clearStdin() {
    int c;
    while((c = getchar()) != '\n' && c != EOF ) {}
}

// Get current input
void getInput(input *current_input) {
    current_input->A = false;
    current_input->B = false;
    current_input->select = false;
    current_input->start = false;
    current_input->up = false;
    current_input->down = false;
    current_input->left = false;
    current_input->right = false;
    char button[11];
    scanf("%10s", button);
    clearStdin();
    if (strcmp("n", button)) {
        return;
    } else if (strcmp("A", button)) {
       current_input->A = true;
    } else if (strcmp("B", button)) {
       current_input->B = true;
    } else if (strcmp("Start", button)) {
       current_input->start = true;
    } else if (strcmp("Select", button)) {
       current_input->select = true;
    } else if (strcmp("U", button)) {
       current_input->up = true;
    } else if (strcmp("D", button)) {
       current_input->down = true;
    } else if (strcmp("L", button)) {
       current_input->left = true;
    } else if (strcmp("R", button)) {
       current_input->right = true;
    }
    //current_input->A = true;
    /*if (XPending(display)) {
        XNextEvent(display, &event);
        if (event.type == KeyPress || event.type == KeyRelease) {
            //printf("Key code is: %d\n", event.xkey.keycode);
            printf("hi");
            switch (event.xkey.keycode) {                
                case 36: // Enter key
                    current_input->start = (event.type == KeyPress && printf("Start"));
                    printf("wtf: %s\n", (current_input->start) ? "Start pressed" : "Start not pressed");
                    break;
                case 52: // z key
                    current_input->A = (event.type == KeyPress && printf("A"));                                        
                    break;
                case 53: // x key
                    current_input->B = (event.type == KeyPress && printf("B"));                     
                    break;
                case 62: // Shift key
                    current_input->select = (event.type == KeyPress && printf("Select"));                     
                    break;
                case 111: // Up arrow
                    current_input->up = (event.type == KeyPress && printf("Up"));                     
                    break;                
                case 113: // Left arrow
                    current_input->left = (event.type == KeyPress && printf("Left"));                     
                    break;
                case 114: // Right arrow
                    current_input->right = (event.type == KeyPress && printf("Right"));                     
                    break;
                case 116: // Down arrow
                    current_input->down = (event.type == KeyPress && printf("Down"));                     
                    break;
            }
        }
    }*/
}

// Swap buffers
void frontend_swap_buffers() {
    glXSwapBuffers(display, window);
}

// Return viewport size
void frontend_get_window_size(windowSize *size) {
    XWindowAttributes windowAttributes = {0};
    XGetWindowAttributes(display, window, &windowAttributes);
    size->width = windowAttributes.width;
    size->height = windowAttributes.height;
}

// Display frameBuffer on screen
void displayOnWindow(uint8 *frameBuffer) {
    gl_display_framebuffer_on_window(frameBuffer);
}

// Start display
void startDisplay() {
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        printf("X11: failue to open display\n");
        exit(11);
    }

    visualInfo = glXChooseVisual(display, 0, attributes);
    if (visualInfo == NULL) {
        printf("GL: Failure to choose a visual\n");
        exit(31);
    }

    root = DefaultRootWindow(display);
    colormap = XCreateColormap(display, root, visualInfo->visual, AllocNone);

    setWindowAttributes.colormap = colormap;
    setWindowAttributes.event_mask = ExposureMask | KeyPressMask;

    window = XCreateWindow(display, root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, visualInfo->depth,
            InputOutput, visualInfo->visual, CWColormap | CWEventMask, &setWindowAttributes);

    XMapWindow(display, window);
    XStoreName(display, window, "GBE");

    screen = DefaultScreen(display);

    glContext = glXCreateContext(display, visualInfo, NULL, GL_TRUE);
    glXMakeCurrent(display, window, glContext);
    gl_clear_window();
}

// CLose display.
void stopDisplay() {
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, glContext);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

// Run emulator from this method.
int main(int argc, char *argv[]) {
    startDisplay();
    int out = startEmulator(argc, argv);
    stopDisplay();
    return out;
}
