/**
 * \file render_api.h
 * \brief Header for functions defined in the AOSP opengl libs
 */
#ifndef __RENDER_API_H__
#define __RENDER_API_H__

#define STREAM_MODE_TCP 1

/* our custom functions */
/** Define a callback to be executed locally on screen rotation */
float AiC_CallbackRotation(void (*fn)(float));

/** Set the pixel density of the screen */
void AiC_setDPI(int dpi);

/* sdk/emulator/opengl/host/libs/libOpenglRender/render_api.cpp */ /**/

/** Initialize the library and tries to load the GLES translator libs */
int initLibrary(void);
/** Change the stream mode, must be called before initOpenGLRenderer */
int setStreamMode(int mode);
/** Initialize the OpenGL renderer process */
int initOpenGLRenderer(int width, int height, char* addr, size_t addrLen);

/** Create a native subwindow to be used for framebuffer display */
int createOpenGLSubwindow(void* window, int x, int y, int width, int height, float zRot);
/** Destroy a native subwindow */
int destroyOpenGLSubwindow(void);
/** Repaint the OpenGL subwindow with the framebuffer content */
void repaintOpenGLDisplay(void);
/** Set the framebuffer display image rotation */
void setOpenGLDisplayRotation(float zRot);

#endif
