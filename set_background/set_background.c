#include <unistd.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <Imlib2.h>

void setRootWindowBackground(Bool type,unsigned int color,char *filename)
{
    Imlib_Image img;
    Display *dpy;
    Pixmap pix;
    Window root;
    Screen *scn;

    /* try 5 times */
    for (int i=0;i<5;i++){
        usleep(500*1000);
        dpy = XOpenDisplay(NULL);
        printf("%p\n", dpy);
        if (dpy)
                break;
    }
    if (!dpy){
        printf("Cannot connect to X server %s\n", XDisplayName(NULL));
        return;
    }

    scn = DefaultScreenOfDisplay(dpy);
    root = DefaultRootWindow(dpy);

    int screenCount = XScreenCount(dpy);

    /* Default use screen 0
     */
    Screen* screent = XScreenOfDisplay(dpy, 0);
    int width = XDisplayWidth(dpy, 0);
    int height = XDisplayHeight(dpy, 0);

    pix = XCreatePixmap(dpy, root, width, height,
        DefaultDepthOfScreen(scn));

    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisualOfScreen(scn));
    imlib_context_set_colormap(DefaultColormapOfScreen(scn));
    imlib_context_set_drawable(pix);

    if(type == 0){
        img = imlib_load_image(filename);
        if (!img) {
            fprintf(stderr, "%s:Unable to load image\n", filename);
            return ;
        }
        imlib_context_set_image(img);

    }else if(type == 1){
        img = imlib_create_image(width, height);
        imlib_context_set_image(img);
        int blue = color & 0xFF;
        int green = color >> 8 & 0xFF;
        int red = color >> 16 & 0xFF;

        imlib_context_set_color(red, green,blue, 255);
        imlib_image_fill_rectangle(0, 0, width, height);
    }

    imlib_context_set_image(img);

    imlib_render_image_on_drawable_at_size(0, 0, width,height);

    XSetWindowBackgroundPixmap(dpy, root, pix);

    XClearWindow(dpy, root);
    printf("%d \n", XPending(dpy));
    printf("%d \n", XEventsQueued(dpy, 2));
    while (XPending(dpy)) {
        printf("in while %d \n", XPending(dpy));
        XEvent ev;
        XNextEvent(dpy, &ev);
    }


    XFreePixmap(dpy, pix);
    imlib_free_image();

    sleep(5);
    XCloseDisplay(dpy);

    return ;
}

int file_exists(char *filename)
{
   return (access(filename, 0) == 0);
}

int main(int argc,char** argv)
{
        char* default_background = "/usr/share/backgrounds/warty-final-ubuntukylin.jpg";
        if (argc > 2) {
                return 0;
        }

        if (argc == 2){
                printf("%s \n", argv[1]);
                if(file_exists(argv[1])){
                        setRootWindowBackground(0,0,argv[1]);
                }else{
                        setRootWindowBackground(0,0,"/usr/share/backgrounds/bg.png");
                }
                return 0;
        }
        setRootWindowBackground(0,0,default_background);
        return 0;
}

