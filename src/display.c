#include "types.h"
#include "common.h"
#include "screen.h"
#include "memory.h"
#include "display.h"
#include <stdlib.h>

const uint8 COLOURS[] = {0xFF, 0xC0, 0x60, 0x00};
uint8 backgroundColourOffset[] = {0, 1, 2, 3};

#ifdef X11
    Display *display;
    Window window;
    int screen;
    XEvent event;
    XWindowAttributes windowAttributes = {0};
    XSetWindowAttributes setWindowAttributes;
#endif
#ifdef OPENGL
    Window root;
    GLint attributes[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
    XVisualInfo *visualInfo;
    Colormap colormap;
    GLXContext glContext;
    GLuint textureID;
#endif
#ifdef SDL
    SDL_Window* window = NULL;
    SDL_Surface* screenSurface = NULL;
#endif

char frameBuffer[3 * DISPLAY_WIDTH * DISPLAY_HEIGHT];
uint8 tiles[384][8][8];

// Start display
void startDisplay() {
    #ifdef X11
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
        glClearColor( 1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glXSwapBuffers(display, window);
    #endif
    #ifdef SDL
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
        screenSurface = SDL_GetWindowSurface(window);
    #endif
}

// Update colour palette for the background
void updateBackgroundColour(uint8 value) {
    for (uint8 i = 0; i < 4; i++) {
        // Mask the two bits to get the background colour set
        backgroundColourOffset[i] = (value >> (i * 2)) & 0b11;
    }
}

// Load all tiles. 384 in total as set 0 overlaps set 1 by 128 tiles.
void loadTiles(cpu_state *cpu) {
    uint8 *vram = cpu->MEM + 0x8000;
    for (int tileNum = 0; tileNum < 384; tileNum++) {
        for (int y = 0; y < 8; y++) {
            uint8 byteLow = *(vram + 2*y + tileNum*16);
            uint8 byteHigh = *(vram + 2*y + 1 + tileNum*16);
            for (int x = 0; x < 8; x++) {
                tiles[tileNum][x][y] = ((byteLow & 0x80) >> 7) | ((byteHigh & 0x80) >> 6);
                byteLow <<= 1;
                byteHigh <<= 1;
            }
        }
    }
}

bool backgroundEnabled = true;
// Load Background into framebuffer
static void loadBackgroundLine(uint8 scanLine, bool tileSet, cpu_state *cpu) {
    if (scanLine == 0 || scanLine > 143) {
        backgroundEnabled = readBit(0, &cpu->MEM[LCDC]);
    }
    // Check if background enabled
    if (backgroundEnabled) {
        int mapLocation = (readBit(3, &cpu->MEM[LCDC])) ? 0x9C00 : 0x9800;
        // Draw tileset onto the window
        mapLocation += ((scanLine + cpu->MEM[SCROLL_Y]) >> 3) << 5;
        short lineOffset = cpu->MEM[SCROLL_X] >> 3;
        short x = cpu->MEM[SCROLL_X] & 7;
        short y = (scanLine + cpu->MEM[SCROLL_Y]) & 7;
        short tile = cpu->MEM[mapLocation + lineOffset];
        // Tile set 0 is numbered -128 to 128
        if (!tileSet) {
            tile = ((int8) tile) + 256;
        }
        short drawOffset = DISPLAY_WIDTH * scanLine;
        for (int i = 0; i < DISPLAY_WIDTH; i++) {
            frameBuffer[(drawOffset + i)*3 + 0] = COLOURS[backgroundColourOffset[tiles[tile][x][y]]];
            frameBuffer[(drawOffset + i)*3 + 1] = COLOURS[backgroundColourOffset[tiles[tile][x][y]]];
            frameBuffer[(drawOffset + i)*3 + 2] = COLOURS[backgroundColourOffset[tiles[tile][x][y]]];
            x++;
            if (x == 8) {
                x = 0;
                lineOffset = (lineOffset + 1) & 31;
                tile = cpu->MEM[mapLocation + lineOffset];
                // Tile set 0 is numbered -128 to 128
                if (!tileSet) {
                    tile = ((int8) tile) + 256;
                }
            }
        }
    }
}

//uint8 nextPosition = 0;
// Load window into frameBuffer
static void loadWindowLine(uint8 scanLine, bool tileSet, cpu_state *cpu) {
    // Check if window is enabled
    if (readBit(5, &cpu->MEM[LCDC])) {
        int16 windowX = cpu->MEM[WINDOW_X] - 7;
        uint8 windowY = cpu->MEM[WINDOW_Y];
        //printf("Window enabled! RAW_X: %d X: %d Y: %d\n", cpu->MEM[WINDOW_X], windowX, windowY);
        // Skip line if the window is below
        if (scanLine < windowY) {
            return;
        }
        int mapLocation = (readBit(6, &cpu->MEM[LCDC])) ? 0x9C00 : 0x9800;
        // Draw tileset onto the window
        mapLocation += ((scanLine + cpu->MEM[WINDOW_Y]) >> 3) << 5;
        short lineOffset = cpu->MEM[WINDOW_X] >> 3;
        short x = (cpu->MEM[WINDOW_X] - 7) & 7;
        short y = (scanLine + cpu->MEM[WINDOW_Y]) & 7;
        //nextPosition++;
        short tile = cpu->MEM[mapLocation + lineOffset];
        // Tile set 0 is numbered -128 to 128
        if (!tileSet) {
            tile = ((int8) tile) + 256;
        }
        short drawOffset = DISPLAY_WIDTH * scanLine;
        for (int i = windowX; i < DISPLAY_WIDTH; i++) {
            if (i >= 0) {
                frameBuffer[(drawOffset + i)*3 + 0] = COLOURS[backgroundColourOffset[tiles[tile][x][y]]];
                frameBuffer[(drawOffset + i)*3 + 1] = COLOURS[backgroundColourOffset[tiles[tile][x][y]]];
                frameBuffer[(drawOffset + i)*3 + 2] = COLOURS[backgroundColourOffset[tiles[tile][x][y]]];
            }
            x++;
            if (x == 8) {
                x = 0;
                lineOffset = (lineOffset + 1) & 31;
                tile = cpu->MEM[mapLocation + lineOffset];
                // Tile set 0 is numbered -128 to 128
                if (!tileSet) {
                    tile = ((int8) tile) + 256;
                }
            }
        }
        if (scanLine >= 143) {
            //nextPosition = 0;
        }
    }
}

// Load scanline into the frame buffer
void loadScanline(cpu_state *cpu) {
    uint8 scanLine = cpu->MEM[SCANLINE];
    bool tileSet = readBit(4, &cpu->MEM[LCDC]);
    loadBackgroundLine(scanLine, tileSet, cpu);
    loadWindowLine(scanLine, tileSet, cpu);
}

// Draw framebuffer to screen
void draw(cpu_state *cpu) {
    //loadTiles(cpu);
    //for (uint8 i = 0; i < DISPLAY_HEIGHT; i++) {
        //loadScanline(i, cpu);
    //}
    //XGetWindowAttributes(display, window, &windowAttributes);
    //glViewport(0, 0, windowAttributes.width, windowAttributes.height);
    #ifdef X11
        /*glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        //TODO: switch to non-deprecated method of drawing
        gluBuild2DMipmaps(GL_TEXTURE_2D, 4, 160, 144, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer);
        glColor3f(1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2i(0, 1); glVertex2i(-1, -1);
        glTexCoord2i(0, 0); glVertex2i(-1, 1);
        glTexCoord2i(1, 0); glVertex2i(1, 1);
        glTexCoord2i(1, 1); glVertex2i(1, -1);
        glEnd();*/
        glRasterPos2f(-1, 1);
        glPixelZoom(2, -2);
        glDrawPixels(DISPLAY_WIDTH, DISPLAY_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer);
        glXSwapBuffers(display, window);
    #endif
    #ifdef SDL
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void*)&frameBuffer,
                DISPLAY_WIDTH,
                DISPLAY_HEIGHT,
                3 * 8,          // bits per pixel = 24
                DISPLAY_WIDTH * 3,  // pitch
                0x0000FF,              // red mask
                0x00FF00,              // green mask
                0xFF0000,              // blue mask
                0);                    // alpha mask (none)
         //SDL_BlitSurface(surface, NULL, screenSurface, NULL);
         //SDL_UpdateWindowSurface(window);
    #endif
}

// CLose display.
void stopDisplay() {
    #ifdef OPENGL
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, glContext);
    #endif
    #ifdef X11
        XDestroyWindow(display, window);
        XCloseDisplay(display);
    #endif
    #ifdef SDL
        //SDL_FreeSurface(surface);
        //surface = NULL;
        SDL_DestroyWindow(window);
        window = NULL;
        SDL_Quit();
    #endif
}
