#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <X11/Xlib.h>

void emit(int fd, int type, int code, int val)
{
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

   write(fd, &ie, sizeof(ie));
}

#if 0
void *disable_leftbutton(void *arg) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        return NULL;
    }

    Window root = DefaultRootWindow(display);

    XGrabButton(display, Button1, AnyModifier, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    XEvent ev;
    while (1) {
        XNextEvent(display, &ev);

        if (ev.type == ButtonPress && ev.xbutton.button == Button1) {
		printf("left button disabled\n");
            continue;
        }
    }

    XCloseDisplay(display);
    return NULL;
}
#endif

int main(void)
{
   struct uinput_setup usetup;
   struct uinput_abs_setup uabs;

   int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

   ioctl(fd, UI_SET_EVBIT, EV_KEY);
   ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);

   ioctl(fd, UI_SET_EVBIT, EV_REL);
   ioctl(fd, UI_SET_RELBIT, REL_X);
   ioctl(fd, UI_SET_RELBIT, REL_Y);

   ioctl(fd, UI_SET_EVBIT, EV_ABS);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
   ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);

   memset(&usetup, 0, sizeof(usetup));
   usetup.id.bustype = BUS_USB;
   usetup.id.vendor = 0x1234;
   usetup.id.product = 0x5678;
   strcpy(usetup.name, "kylin virtual touch device");

   ioctl (fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

   memset(&uabs, 0, sizeof(uabs));
   uabs.code = ABS_MT_SLOT;
   uabs.absinfo.minimum = 0;
   uabs.absinfo.maximum = 9;
   ioctl(fd, UI_ABS_SETUP, &uabs);

   memset(&uabs, 0, sizeof(uabs));
   uabs.code = ABS_MT_POSITION_X;
   uabs.absinfo.minimum = 0;
   uabs.absinfo.maximum = 1920;
   uabs.absinfo.resolution= 76;
   ioctl(fd, UI_ABS_SETUP, &uabs);

   memset(&uabs, 0, sizeof(uabs));
   uabs.code = ABS_MT_POSITION_Y;
   uabs.absinfo.minimum = 0;
   uabs.absinfo.maximum = 1200;
   uabs.absinfo.resolution= 106;
   ioctl(fd, UI_ABS_SETUP, &uabs);

   ioctl(fd, UI_DEV_SETUP, &usetup);
   ioctl(fd, UI_DEV_CREATE);

   Display *display;
   Window root;
   Window child;
   int root_x, root_y, win_x, win_y;
   unsigned int mask;

   display = XOpenDisplay(NULL);
   if (display == NULL) {
       return 1;
   }

#if 0
   pthread_t thread;
   if (pthread_create(&thread, NULL, disable_leftbutton, NULL) != 0) {
        return 1;
   }
#endif
    root = DefaultRootWindow(display);

   /*
    * On UI_DEV_CREATE the kernel will create the device node for this
    * device. We are inserting a pause here so that userspace has time
    * to detect, initialize the new device, and can start listening to
    * the event, otherwise it will not notice the event we are about
    * to send. This pause is only needed in our example code!
    */
   sleep(1);

   int mouse_fd = open("/dev/input/event4", O_RDONLY);
   if (mouse_fd < 0) {
       ioctl(fd, UI_DEV_DESTROY);
       close(fd);
       return EXIT_FAILURE;
   }

   while (1) {
       struct input_event ie;
       read(mouse_fd, &ie, sizeof(ie));

       if (ie.type == EV_REL && (ie.code == REL_X || ie.code == REL_Y)) {
	       continue;
       }

       switch (ie.code){
	      	case BTN_LEFT:
			if(ie.value==1){
				XQueryPointer(display, root, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
			      	emit(fd, EV_ABS, ABS_MT_TRACKING_ID, 1);
			      	emit(fd, EV_ABS, ABS_MT_POSITION_X, root_x);
			      	emit(fd, EV_ABS, ABS_MT_POSITION_Y, root_y);
			      	emit(fd, EV_KEY, BTN_TOUCH, 1);
			      	emit(fd, EV_SYN, SYN_REPORT, 0);
			}else{
			      	emit(fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
				emit(fd, EV_KEY, BTN_TOUCH, 0);
				emit(fd, EV_SYN, SYN_REPORT, 0);
			}
		default:
                        break;
       }
   }

   /*
    * Give userspace some time to read the events before we destroy the
    * device with UI_DEV_DESTOY.
    */
   sleep(1);
#if 0
   if (pthread_join(thread, NULL) != 0) {
        return 1;
   }
#endif
   XCloseDisplay(display);
   ioctl(fd, UI_DEV_DESTROY);
   close(fd);

   return 0;
}
