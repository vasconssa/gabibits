#include "sx/platform.h"

#if SX_PLATFORM_LINUX && GB_USE_XLIB

#include "macros.h"
#include "sx/sx.h"
#include "sx/os.h"
#include "sx/io.h"
#include "sx/allocator.h"
#include "sx/atomic.h"
#include "sx/threads.h"
#include "sx/string.h"
#include "device/types.h"
#include "device/vk_device.h"
#include <stdio.h>
/*#include "vk_renderer.h"*/
#include "world/renderer.h"
#include <stdbool.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static KeyboardButton x11_translate_key(KeySym x11_key)
{
	switch (x11_key)
	{
	case XK_BackSpace:    return KB_BACKSPACE;
	case XK_Tab:          return KB_TAB;
	case XK_space:        return KB_SPACE;
	case XK_Escape:       return KB_ESCAPE;
	case XK_Return:       return KB_ENTER;
	case XK_F1:           return KB_F1;
	case XK_F2:           return KB_F2;
	case XK_F3:           return KB_F3;
	case XK_F4:           return KB_F4;
	case XK_F5:           return KB_F5;
	case XK_F6:           return KB_F6;
	case XK_F7:           return KB_F7;
	case XK_F8:           return KB_F8;
	case XK_F9:           return KB_F9;
	case XK_F10:          return KB_F10;
	case XK_F11:          return KB_F11;
	case XK_F12:          return KB_F12;
	case XK_Home:         return KB_HOME;
	case XK_Left:         return KB_LEFT;
	case XK_Up:           return KB_UP;
	case XK_Right:        return KB_RIGHT;
	case XK_Down:         return KB_DOWN;
	case XK_Page_Up:      return KB_PAGE_UP;
	case XK_Page_Down:    return KB_PAGE_DOWN;
	case XK_Insert:       return KB_INS;
	case XK_Delete:       return KB_DEL;
	case XK_End:          return KB_END;
	case XK_Shift_L:      return KB_SHIFT_LEFT;
	case XK_Shift_R:      return KB_SHIFT_RIGHT;
	case XK_Control_L:    return KB_CTRL_LEFT;
	case XK_Control_R:    return KB_CTRL_RIGHT;
	case XK_Caps_Lock:    return KB_CAPS_LOCK;
	case XK_Alt_L:        return KB_ALT_LEFT;
	case XK_Alt_R:        return KB_ALT_RIGHT;
	case XK_Super_L:      return KB_SUPER_LEFT;
	case XK_Super_R:      return KB_SUPER_RIGHT;
	case XK_Num_Lock:     return KB_NUM_LOCK;
	case XK_KP_Enter:     return KB_NUMPAD_ENTER;
	case XK_KP_Delete:    return KB_NUMPAD_DELETE;
	case XK_KP_Multiply:  return KB_NUMPAD_MULTIPLY;
	case XK_KP_Add:       return KB_NUMPAD_ADD;
	case XK_KP_Subtract:  return KB_NUMPAD_SUBTRACT;
	case XK_KP_Divide:    return KB_NUMPAD_DIVIDE;
	case XK_KP_Insert:
	case XK_KP_0:         return KB_NUMPAD_0;
	case XK_KP_End:
	case XK_KP_1:         return KB_NUMPAD_1;
	case XK_KP_Down:
	case XK_KP_2:         return KB_NUMPAD_2;
	case XK_KP_Page_Down: // or XK_KP_Next
	case XK_KP_3:         return KB_NUMPAD_3;
	case XK_KP_Left:
	case XK_KP_4:         return KB_NUMPAD_4;
	case XK_KP_Begin:
	case XK_KP_5:         return KB_NUMPAD_5;
	case XK_KP_Right:
	case XK_KP_6:         return KB_NUMPAD_6;
	case XK_KP_Home:
	case XK_KP_7:         return KB_NUMPAD_7;
	case XK_KP_Up:
	case XK_KP_8:         return KB_NUMPAD_8;
	case XK_KP_Page_Up:   // or XK_KP_Prior
	case XK_KP_9:         return KB_NUMPAD_9;
	case '0':             return KB_NUMBER_0;
	case '1':             return KB_NUMBER_1;
	case '2':             return KB_NUMBER_2;
	case '3':             return KB_NUMBER_3;
	case '4':             return KB_NUMBER_4;
	case '5':             return KB_NUMBER_5;
	case '6':             return KB_NUMBER_6;
	case '7':             return KB_NUMBER_7;
	case '8':             return KB_NUMBER_8;
	case '9':             return KB_NUMBER_9;
	case 'a':             return KB_A;
	case 'b':             return KB_B;
	case 'c':             return KB_C;
	case 'd':             return KB_D;
	case 'e':             return KB_E;
	case 'f':             return KB_F;
	case 'g':             return KB_G;
	case 'h':             return KB_H;
	case 'i':             return KB_I;
	case 'j':             return KB_J;
	case 'k':             return KB_K;
	case 'l':             return KB_L;
	case 'm':             return KB_M;
	case 'n':             return KB_N;
	case 'o':             return KB_O;
	case 'p':             return KB_P;
	case 'q':             return KB_Q;
	case 'r':             return KB_R;
	case 's':             return KB_S;
	case 't':             return KB_T;
	case 'u':             return KB_U;
	case 'v':             return KB_V;
	case 'w':             return KB_W;
	case 'x':             return KB_X;
	case 'y':             return KB_Y;
	case 'z':             return KB_Z;
	default:              return KB_COUNT;
	}
}


typedef struct LinuxDevice {
   Display* x11_display;
    Atom wm_delete_window;
    Atom net_wm_state;
    Atom net_wm_state_maximized_horz;
    Atom net_wm_state_maximized_vert;
    Atom net_wm_state_fullscreen;
    Cursor x11_hidden_cursor;
    bool x11_detectable_autorepeat;
    XRRScreenConfiguration* screen_config;
    /*DeviceEventQueue _queue;*/
    /*Joypad _joypad;*/
    Window x11_window;
    XIC ic;
    XIM im;
    Pixmap bitmap; 
    s16 mouse_last_x;
    s16 mouse_last_y;
    CursorMode cursor_mode;
} LinuxDevice;

static LinuxDevice linux_device;
static DeviceWindow win;
static Cursor x11_cursors[MC_COUNT];

void create_window(u_int16_t width, u_int16_t height) {
    int screen = DefaultScreen(linux_device.x11_display);
    int depth = DefaultDepth(linux_device.x11_display, screen);
    Visual* visual = DefaultVisual(linux_device.x11_display, screen);

    Window root_window = RootWindow(linux_device.x11_display, screen);

    // Create main window
    XSetWindowAttributes win_attribs;
    win_attribs.background_pixmap = 0;
    /*win_attribs.background_pixel = BlackPixel(linux_device.x11_display, DefaultScreen(linux_device.x11_display));*/
    win_attribs.border_pixel = 0;
    win_attribs.event_mask = FocusChangeMask
        | StructureNotifyMask
        ;

    win_attribs.event_mask |= KeyPressMask
        | KeyReleaseMask
        | ButtonPressMask
        | ButtonReleaseMask
        | PointerMotionMask
        | EnterWindowMask
        ;

    linux_device.x11_window = XCreateWindow(linux_device.x11_display
            , root_window
            , 0
            , 0
            , width
            , height
            , 0
            , depth
            , InputOutput
            , visual
            , CWBorderPixel | CWEventMask
            , &win_attribs
            );
    sx_assert(linux_device.x11_window != None);

    XSetWMProtocols(linux_device.x11_display, linux_device.x11_window, &linux_device.wm_delete_window, 1);

    XMapRaised(linux_device.x11_display, linux_device.x11_window);

    XFlush(linux_device.x11_display);
}

void close()
{
    XDestroyWindow(linux_device.x11_display, linux_device.x11_window);
}

void show()
{
    XMapRaised(linux_device.x11_display, linux_device.x11_window);
}

void hide()
{
    XUnmapWindow(linux_device.x11_display, linux_device.x11_window);
}

void resize(u16 width, u16 height)
{
    XResizeWindow(linux_device.x11_display, linux_device.x11_window, width, height);
}

void move(u16 x, u16 y)
{
    XMoveWindow(linux_device.x11_display, linux_device.x11_window, x, y);
}

void maximize_or_restore(bool maximize)
{
    XEvent xev;
    xev.type = ClientMessage;
    xev.xclient.window = linux_device.x11_window;
    xev.xclient.message_type = linux_device.net_wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = maximize ? 1 : 0; // 0 = remove property, 1 = set property
    xev.xclient.data.l[1] = linux_device.net_wm_state_maximized_horz;
    xev.xclient.data.l[2] = linux_device.net_wm_state_maximized_vert;
    XSendEvent(linux_device.x11_display, DefaultRootWindow(linux_device.x11_display), False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);
}

void minimize()
{
    XIconifyWindow(linux_device.x11_display, linux_device.x11_window, DefaultScreen(linux_device.x11_display));
}

void maximize()
{
    maximize_or_restore(true);
}

void restore()
{
    maximize_or_restore(false);
}

const char* title()
{
    static char buf[512];
    memset(buf, 0, sizeof(buf));
    char* name;
    XFetchName(linux_device.x11_display, linux_device.x11_window, &name);
    strncpy(buf, name, sizeof(buf)-1);
    XFree(name);
    return buf;
}

void set_title (const char* title)
{
    XStoreName(linux_device.x11_display, linux_device.x11_window, title);
}

void* handle()
{
    return (void*)(uintptr_t)linux_device.x11_window;
}

void show_cursor(bool show)
{
    XDefineCursor(linux_device.x11_display
            , linux_device.x11_window
            , show ? None : linux_device.x11_hidden_cursor
            );
}

void set_fullscreen(bool full)
{
    XEvent xev;
    xev.xclient.type = ClientMessage;
    xev.xclient.window = linux_device.x11_window;
    xev.xclient.message_type = linux_device.net_wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = full ? 1 : 0;
    xev.xclient.data.l[1] = linux_device.net_wm_state_fullscreen;
    XSendEvent(linux_device.x11_display, DefaultRootWindow(linux_device.x11_display), False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);
}

void set_cursor(MouseCursor cursor)
{
    XDefineCursor(linux_device.x11_display, linux_device.x11_window, x11_cursors[cursor]);
}

void set_cursor_mode(CursorMode mode)
{
    if (mode == linux_device.cursor_mode)
        return;

    linux_device.cursor_mode = mode;

    if (mode == CM_DISABLED)
    {
        XWindowAttributes window_attribs;
        XGetWindowAttributes(linux_device.x11_display, linux_device.x11_window, &window_attribs);
        unsigned width = window_attribs.width;
        unsigned height = window_attribs.height;
        linux_device.mouse_last_x = width/2;
        linux_device.mouse_last_y = height/2;

        XWarpPointer(linux_device.x11_display
                , None
                , linux_device.x11_window
                , 0
                , 0
                , 0
                , 0
                , width/2
                , height/2
                );
        XGrabPointer(linux_device.x11_display
                , linux_device.x11_window
                , True
                , ButtonPressMask | ButtonReleaseMask | PointerMotionMask
                , GrabModeAsync
                , GrabModeAsync
                , linux_device.x11_window
                , linux_device.x11_hidden_cursor
                , CurrentTime
                );
        XFlush(linux_device.x11_display);
    }
    else if (mode == CM_NORMAL)
    {
        XUngrabPointer(linux_device.x11_display, CurrentTime);
        XFlush(linux_device.x11_display);
    }
}

static bool s_exit = false;
sx_queue_spsc* event_queue = NULL;

int run() {
    Status xs = XInitThreads();
    sx_assert(xs != 0);
    sx_unused(xs);
    // http://tronche.com/gui/x/xlib/display/XInitThreads.html

    linux_device.x11_display = XOpenDisplay(NULL);
    sx_assert(linux_device.x11_display != NULL);

    Window root_window = RootWindow(linux_device.x11_display, DefaultScreen(linux_device.x11_display));

    // Do we have detectable autorepeat?
    Bool detectable;
    linux_device.x11_detectable_autorepeat = (bool)XkbSetDetectableAutoRepeat(linux_device.x11_display, true, &detectable);

    linux_device.wm_delete_window = XInternAtom(linux_device.x11_display, "WM_DELETE_WINDOW", False);
    linux_device.net_wm_state = XInternAtom(linux_device.x11_display, "_NET_WM_STATE", False);
    linux_device.net_wm_state_maximized_horz = XInternAtom(linux_device.x11_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    linux_device.net_wm_state_maximized_vert = XInternAtom(linux_device.x11_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    linux_device.net_wm_state_fullscreen = XInternAtom(linux_device.x11_display, "_NET_WM_STATE_FULLSCREEN", False);

    // Save screen configuration
    linux_device.screen_config = XRRGetScreenInfo(linux_device.x11_display, root_window);

    Rotation rr_old_rot;
    const SizeID rr_old_sizeid = XRRConfigCurrentConfiguration(linux_device.screen_config, &rr_old_rot);

    XIM im;
    im = XOpenIM(linux_device.x11_display, NULL, NULL, NULL);
    sx_assert(im != NULL);

    linux_device.ic = XCreateIC(im
            , XNInputStyle
            , 0
            | XIMPreeditNothing
            | XIMStatusNothing
            , XNClientWindow
            , root_window
            , NULL
            );
    sx_assert(linux_device.ic != NULL);

    // Create hidden cursor
    Pixmap bitmap;
    const char data[8] = { 0 };
    XColor dummy;
    bitmap = XCreateBitmapFromData(linux_device.x11_display, root_window, data, 8, 8);
    linux_device.x11_hidden_cursor = XCreatePixmapCursor(linux_device.x11_display, bitmap, bitmap, &dummy, &dummy, 0, 0);

    // Create standard cursors
    x11_cursors[MC_ARROW]               = XCreateFontCursor(linux_device.x11_display, XC_top_left_arrow);
    x11_cursors[MC_HAND]                = XCreateFontCursor(linux_device.x11_display, XC_hand2);
    x11_cursors[MC_TEXT_INPUT]          = XCreateFontCursor(linux_device.x11_display, XC_xterm);
    x11_cursors[MC_CORNER_TOP_LEFT]     = XCreateFontCursor(linux_device.x11_display, XC_top_left_corner);
    x11_cursors[MC_CORNER_TOP_RIGHT]    = XCreateFontCursor(linux_device.x11_display, XC_top_right_corner);
    x11_cursors[MC_CORNER_BOTTOM_LEFT]  = XCreateFontCursor(linux_device.x11_display, XC_bottom_left_corner);
    x11_cursors[MC_CORNER_BOTTOM_RIGHT] = XCreateFontCursor(linux_device.x11_display, XC_bottom_right_corner);
    x11_cursors[MC_SIZE_HORIZONTAL]     = XCreateFontCursor(linux_device.x11_display, XC_sb_h_double_arrow);
    x11_cursors[MC_SIZE_VERTICAL]       = XCreateFontCursor(linux_device.x11_display, XC_sb_v_double_arrow);
    x11_cursors[MC_WAIT]                = XCreateFontCursor(linux_device.x11_display, XC_watch);


    win.connection = linux_device.x11_display;
    win.window = linux_device.x11_window;

    const sx_alloc* alloc = sx_alloc_malloc();

    event_queue = sx_queue_spsc_create(alloc, sizeof(OsEvent), 128);

    sx_thread* thrd = sx_thread_create(alloc, device_run, &win, 0, "RenderThread", NULL);
    if (!thrd) {
        return -1;
    }

    while (!s_exit)
    {
        int pending = XPending(linux_device.x11_display);

        if (!pending) {
            sx_os_sleep(8);
        } else {
				XEvent event;
				XNextEvent(linux_device.x11_display, &event);
                OsEvent ev_item;

				switch (event.type)
				{
				case EnterNotify:
					linux_device.mouse_last_x = (s16)event.xcrossing.x;
					linux_device.mouse_last_y = (s16)event.xcrossing.y;
                    ev_item.axis.type = OST_AXIS;
                    ev_item.axis.device_id = IDT_MOUSE;
                    ev_item.axis.device_num = 0;
                    ev_item.axis.axis_num = MA_CURSOR;
                    ev_item.axis.axis_x = event.xcrossing.x;
                    ev_item.axis.axis_y = event.xcrossing.y;
                    ev_item.axis.axis_z = 0;
                    sx_queue_spsc_produce(event_queue, &ev_item);
					break;

				case ClientMessage:
                    ev_item.type = OST_EXIT;
					if ((Atom)event.xclient.data.l[0] == linux_device.wm_delete_window)
						sx_queue_spsc_produce(event_queue, &ev_item);
					break;

				case ConfigureNotify:
                    ev_item.resolution.type = OST_RESOLUTION;
                    ev_item.resolution.width = event.xconfigure.width;

                    ev_item.resolution.height = event.xconfigure.height;
                    sx_queue_spsc_produce(event_queue, &ev_item);
					break;

				case ButtonPress:
				case ButtonRelease:
					{
						if (event.xbutton.button == Button4 || event.xbutton.button == Button5)
						{
                            ev_item.axis.type = OST_AXIS;
                            ev_item.axis.device_id = IDT_MOUSE;
                            ev_item.axis.device_num = 0;
                            ev_item.axis.axis_num = MA_WHEEL;
                            ev_item.axis.axis_x = 0;
                            ev_item.axis.axis_y = Button4 ? 1 : -1;
                            ev_item.axis.axis_z = 0;
                            sx_queue_spsc_produce(event_queue, &ev_item);
							break;
						}

						MouseButton mb;
						switch (event.xbutton.button)
						{
						case Button1: mb = MB_LEFT; break;
						case Button2: mb = MB_MIDDLE; break;
						case Button3: mb = MB_RIGHT; break;
						default: mb = MB_COUNT; break;
						}

						if (mb != MB_COUNT)
						{
                            ev_item.button.type = OST_BUTTON;
                            ev_item.button.device_id = IDT_MOUSE;
                            ev_item.button.device_num = 0;
                            ev_item.button.button_num = mb;
                            ev_item.button.pressed = event.type == ButtonPress;
                            sx_queue_spsc_produce(event_queue, &ev_item);
						}
					}
					break;

				case MotionNotify:
					{
						const s32 mx = event.xmotion.x;
						const s32 my = event.xmotion.y;
						s16 deltax = mx - linux_device.mouse_last_x;
						s16 deltay = my - linux_device.mouse_last_y;
						if (linux_device.cursor_mode == CM_DISABLED)
						{
							XWindowAttributes window_attribs;
							XGetWindowAttributes(linux_device.x11_display, linux_device.x11_window, &window_attribs);
							unsigned width = window_attribs.width;
							unsigned height = window_attribs.height;
							if (mx != (s32)width/2 || my != (s32)height/2)
							{
                                ev_item.axis.type = OST_AXIS;
                                ev_item.axis.device_id = IDT_MOUSE;
                                ev_item.axis.device_num = 0;
                                ev_item.axis.axis_num = MA_CURSOR_DELTA;
                                ev_item.axis.axis_x = deltax;
                                ev_item.axis.axis_y = deltay;
                                ev_item.axis.axis_z = 0;
                                sx_queue_spsc_produce(event_queue, &ev_item);
								XWarpPointer(linux_device.x11_display
									, None
									, linux_device.x11_window
									, 0
									, 0
									, 0
									, 0
									, width/2
									, height/2
									);
								XFlush(linux_device.x11_display);
							}
						}
						else if (linux_device.cursor_mode == CM_NORMAL)
						{
                            ev_item.axis.type = OST_AXIS;
                            ev_item.axis.device_id = IDT_MOUSE;
                            ev_item.axis.device_num = 0;
                            ev_item.axis.axis_num = MA_CURSOR_DELTA;
                            ev_item.axis.axis_x = deltax;
                            ev_item.axis.axis_y = deltay;
                            ev_item.axis.axis_z = 0;
                            sx_queue_spsc_produce(event_queue, &ev_item);
						}
                        ev_item.axis.type = OST_AXIS;
                        ev_item.axis.device_id = IDT_MOUSE;
                        ev_item.axis.device_num = 0;
                        ev_item.axis.axis_num = MA_CURSOR_DELTA;
                        ev_item.axis.axis_x = (uint16_t)mx;
                        ev_item.axis.axis_y = (uint16_t)my;
                        ev_item.axis.axis_z = 0;
                        sx_queue_spsc_produce(event_queue, &ev_item);
						linux_device.mouse_last_x = (uint16_t)mx;
						linux_device.mouse_last_y = (uint16_t)my;
					}
					break;

				case KeyPress:
				case KeyRelease:
					{
						KeySym keysym = XLookupKeysym(&event.xkey, 0);

						KeyboardButton kb = x11_translate_key(keysym);
						if (kb != KB_COUNT)
						{
                            ev_item.button.type = OST_BUTTON;
                            ev_item.button.device_id = IDT_KEYBOARD;
                            ev_item.button.device_num = 0;
                            ev_item.button.button_num = kb;
                            ev_item.button.pressed = event.type == KeyPress;
                            sx_queue_spsc_produce(event_queue, &ev_item);
						}

						if (event.type == KeyPress)
						{
							Status status = 0;
							u8 utf8[4] = { 0 };
							int len = Xutf8LookupString(linux_device.ic
								, &event.xkey
								, (char*)utf8
								, sizeof(utf8)
								, NULL
								, &status
								);

							if (status == XLookupChars || status == XLookupBoth)
							{
                                ev_item.text.type = OST_TEXT;
                                ev_item.text.len = len;
                                memcpy(ev_item.text.utf8, utf8, len);
								if (len)
                                    sx_queue_spsc_produce(event_queue, &ev_item);
							}
						}
					}
					break;
				case KeymapNotify:
					XRefreshKeyboardMapping(&event.xmapping);
					break;

				default:
					break;
				}
			}

    }

    // Free standard cursors
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_WAIT]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_SIZE_VERTICAL]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_SIZE_HORIZONTAL]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_CORNER_BOTTOM_RIGHT]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_CORNER_BOTTOM_LEFT]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_CORNER_TOP_RIGHT]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_CORNER_TOP_LEFT]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_TEXT_INPUT]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_HAND]);
    XFreeCursor(linux_device.x11_display, x11_cursors[MC_ARROW]);

    // Free hidden cursor
    XFreeCursor(linux_device.x11_display, linux_device.x11_hidden_cursor);
    XFreePixmap(linux_device.x11_display, linux_device.bitmap);

    XDestroyIC(linux_device.ic);
    XCloseIM(linux_device.im);

    // Restore previous screen configuration
    Rotation rr_rot;
    const SizeID rr_sizeid = XRRConfigCurrentConfiguration(linux_device.screen_config, &rr_rot);

    /*if (rr_rot != rr_old_rot || rr_sizeid != rr_old_sizeid)*/
    /*{*/
        /*XRRSetScreenConfig(linux_device.x11_display*/
            /*, linux_device.screen_config*/
            /*, root_window*/
            /*, rr_old_sizeid*/
            /*, rr_old_rot*/
            /*, CurrentTime*/
            /*);*/
    /*}*/
    XRRFreeScreenConfigInfo(linux_device.screen_config);

    XCloseDisplay(linux_device.x11_display);
    return 0;
}

bool next_event(OsEvent* ev) {
    return sx_queue_spsc_consume(event_queue, ev);
}

int main(int argc, char** argv) {
    run();
    return 0;
}
#endif
