/*
 * X.org screen grabber for MJPG-streamer
 * Based on Linux-UVC streaming input-plugin for MJPG-streamer
 * 2021 Ilya Chelyadin (C)
 */
#define XSHM

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/X.h>
#include <jpeglib.h>
#include <stdbool.h>
#include "jpeg_utils.h"

#ifdef XSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
#define INPUT_PLUGIN_NAME "XGRAB input plugin"

/* private functions and variables to this plugin */
static pthread_t   worker;
static globals     *pglobal;
static pthread_mutex_t controls_mutex;
static int plugin_number;

void *worker_thread(void *);
void worker_cleanup(void *);
void help(void);

int width=1920, height=1080;
int quality=80;
int fps=60;
bool grabPointer=false;

struct xcursor {
    XFixesCursorImage *xcim;
    int x, y;
    int to_line, to_column;
};

/*** plugin interface functions ***/

/******************************************************************************
Description.: parse input parameters
Input Value.: param contains the command line string and a pointer to globals
Return Value: 0 if everything is ok
******************************************************************************/
int input_init(input_parameter *param, int plugin_no)
{

    if(pthread_mutex_init(&controls_mutex, NULL) != 0) {
        IPRINT("could not initialize mutex variable\n");
        exit(EXIT_FAILURE);
    }

    param->argv[0] = INPUT_PLUGIN_NAME;

    /* show all parameters for DBG purposes */
    for(int i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
   
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"r", required_argument, 0, 0},
            {"resolution", required_argument, 0, 0},
            {"q", required_argument, 0, 0},
            {"quality", required_argument, 0, 0},
            {"fps", required_argument, 0, 0},
            {"cursor", required_argument, 0, 0},
            {"c", required_argument, 0, 0}
        };
        
        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;
        switch(option_index) {
        case 0:
        case 1:
            DBG("case 0,1\n");
            parse_resolution_opt(optarg, &width, &height);
            break;
        case 2:
        case 3:
            sscanf(optarg, "%d", &quality);
            break;
        case 4:
            sscanf(optarg, "%d", &fps);
            break;
        case 5:
        case 6:
            sscanf(optarg, "%d", &grabPointer);
            break;

         }

    }

    pglobal = param->global;

    IPRINT("resolution........: %i x %i\n", width, height);
    IPRINT("quality...........: %i\n", quality);
    IPRINT("framerate.........: %i\n", fps);
    IPRINT("pointer...........: %s\n", grabPointer ? "true" : "false");

    return 0;
}

/******************************************************************************
Description.: stops the execution of the worker thread
Input Value.: -
Return Value: 0
******************************************************************************/
int input_stop(int id)
{
    DBG("will cancel input thread\n");
    pthread_cancel(worker);

    return 0;
}

/******************************************************************************
Description.: starts the worker thread and allocates memory
Input Value.: -
Return Value: 0
******************************************************************************/
int input_run(int id)
{
    pglobal->in[id].buf = malloc(2048 * 1024);
    if(pglobal->in[id].buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&worker, 0, worker_thread, NULL) != 0) {
        free(pglobal->in[id].buf);
        fprintf(stderr, "could not start worker thread\n");
        exit(EXIT_FAILURE);
    }
    pthread_detach(worker);

    return 0;
}

/******************************************************************************
Description.: print help message
Input Value.: -
Return Value: -
******************************************************************************/
void help(void)
{
    fprintf(stderr, " ---------------------------------------------------------------\n" \
    " Help for input plugin..: "INPUT_PLUGIN_NAME"\n" \
    " ---------------------------------------------------------------\n");
}



void grab_mouse_pointer(struct xcursor *xc, Display *dpy)
{
    XFree(xc->xcim);
    xc->xcim = NULL;

    xc->xcim = XFixesGetCursorImage(dpy);
    xc->x = xc->xcim->x - xc->xcim->xhot;
    xc->y = xc->xcim->y - xc->xcim->yhot;
    xc->to_line   = (xc->y + xc->xcim->height);
    xc->to_column = (xc->x + xc->xcim->width);

}

void draw_mouse_pointer(struct xcursor *xc, unsigned char *target_array, int column, int line, int width) {
    if (column < xc->x || line < xc->y || column >= xc->to_column || line >= xc->to_line) return;

    int xcim_addr  = (line  - xc->y) * xc->xcim->width + column - xc->x;

    int r          = (uint8_t)(xc->xcim->pixels[xcim_addr] >>  0);
    int g          = (uint8_t)(xc->xcim->pixels[xcim_addr] >>  8);
    int b          = (uint8_t)(xc->xcim->pixels[xcim_addr] >> 16);
    int a          = (uint8_t)(xc->xcim->pixels[xcim_addr] >> 24);

    if (a == 255) {
        target_array[(column + width * line) * 3+0] = r;
        target_array[(column + width * line) * 3+1] = g;
        target_array[(column + width * line) * 3+2] = b;
    }
     else if (a) {
        // pixel values from XFixesGetCursorImage come premultiplied by alpha
        target_array[(column + width * line) * 3+0] = r + (target_array[(column + width * line) * 3+0] * (255 - a) + 255 / 2) / 255;
        target_array[(column + width * line) * 3+1] = g + (target_array[(column + width * line) * 3+1] * (255 - a) + 255 / 2) / 255;
        target_array[(column + width * line) * 3+2] = b + (target_array[(column + width * line) * 3+2] * (255 - a) + 255 / 2) / 255;
        }

}


/******************************************************************************
Description.: copy a picture from testpictures.h and signal this to all output
              plugins, afterwards switch to the next frame of the animation.
Input Value.: arg is not used
Return Value: NULL
******************************************************************************/
void *worker_thread(void *arg)
{

    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(worker_cleanup, NULL);
   Display *display = XOpenDisplay(NULL);
   Window root = DefaultRootWindow(display);
   XWindowAttributes gwa;

   XGetWindowAttributes(display, root, &gwa);
   
   unsigned char array[width * height * 3]; 
   unsigned long red_mask, green_mask, blue_mask, pixel;
   unsigned char blue, green, red;
   XImage *image;
   struct xcursor pointer_grab_context;
   int x, y;
   int frametime = 1000/fps;

   #ifdef XSHM
   XShmSegmentInfo shminfo;
   image = XShmCreateImage(display,
                           DefaultVisual(display,0), // Use a correct visual. Omitted for brevity
                           DefaultDepth(display, 0),   // Determine correct depth from the visual. Omitted for brevity
                           ZPixmap, NULL, &shminfo, width, height);

   shminfo.shmid = shmget(IPC_PRIVATE,
                          image->bytes_per_line * image->height,
                          IPC_CREAT|0777);

   shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
   shminfo.readOnly = False;
   XShmAttach(display, &shminfo);
   #endif

    while(!pglobal->stop) {
        #ifdef XSHM
        XShmGetImage(display, RootWindow(display,0), image, 0, 0, AllPlanes);
        #else
        image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
        #endif
        red_mask = image->red_mask;
        green_mask = image->green_mask;
        blue_mask = image->blue_mask;
        if (grabPointer) grab_mouse_pointer(&pointer_grab_context, display);

        for (x = 0; x < width; x++)
            for (y = 0; y < height ; y++)
            {
                pixel = XGetPixel(image,x,y);

                blue = pixel & blue_mask;
                green = (pixel & green_mask) >> 8;
                red = (pixel & red_mask) >> 16;
                
                array[(x + width * y) * 3] = red;
                array[(x + width * y) * 3+1] = green;
                array[(x + width * y) * 3+2] = blue;
                if (grabPointer) draw_mouse_pointer(&pointer_grab_context, array, x, y,width);
            }
        #ifndef XSHM
        XDestroyImage(image);
        #endif
        /* copy JPG picture to global buffer */
        
        pthread_mutex_lock(&pglobal->in[plugin_number].db);
        pglobal->in[plugin_number].size = save_jpg(array, width, height, quality, pglobal->in[plugin_number].buf);

        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[plugin_number].db_update);
        pthread_mutex_unlock(&pglobal->in[plugin_number].db);

        usleep(1000 * frametime);
    }

    IPRINT("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

/******************************************************************************
Description.: this functions cleans up allocated resources
Input Value.: arg is unused
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up resources\n");
        return;
    }

    first_run = 0;
    DBG("cleaning up resources allocated by input thread\n");

    if(pglobal->in[plugin_number].buf != NULL) free(pglobal->in[plugin_number].buf);
}
