#include <fcntl.h>		/* open() */
#include <termios.h>	/* TCGETS TCSETS */
#include <time.h>		/* nanosleep() */
#include <unistd.h>		/* read() write() */
#include <linux/fb.h>	/* FB_VAR_SCREENINFO FB_FIX_SCREENINFO */
#include <sys/ioctl.h>	/* ioctl() */
#include <sys/mman.h>	/* PROT_READ PROT_WRITE */
#include <sys/stat.h>	/* open() */
#include <sys/types.h>	/* open() select() */

/*
	[      RED     ]  [      GREEN      ]  [     BLUE     ]
	[15-14-13-12-11]  [10-09-08-07-06-05]  [04-03-02-01-00]
*/
typedef unsigned short color_t;			// store RGB color

int fd_display;							// reference to display
int res_height, res_width;				// store display-height and width calculation
size_t screen_size;						// number of bytes of entire display
color_t *display_addr;					// starting address of display
struct fb_var_screeninfo display_res;	// resolution for the mapped display
struct fb_fix_screeninfo display_depth;	// bit depth for the mapped display

void clear_screen();
void draw_pixel(int x, int y, color_t color);
void draw_line(int x1, int y1, int x2, int y2, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void init_graphics();
void exit_graphics();
char getkey();
void sleep_ms(long ms);

#define BMASK(c) (c & 0x101F)	// Blue mask
#define GMASK(c) (c & 0x17E0)	// Green mask
#define RMASK(c) (c & 0xF800) 	// Red mask
#define TRUE 1					// fucking C

/*
	lazy absolute value function
*/
int abs(int x) {
	if (x < 0) { x = -x; }
	return x;
}


/*
	Clear the screen by writing ANSI ESCAPE code "\033[2J".
	The screen is STDOUT aka FD=1

	ssize_t write(int FD, const void *BUF, size_t COUNT);
*/
void clear_screen() {
	write(1, "\033[2J", 8);
}

/*
	Using Bresenham's Algorithm for drawing a straight line
	between two given points.

	NOTE: The algorithm has a "standard error" associated 
	with it. So when lines get drawn they will be approximate
	per-pixel locations.

	https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
*/
void draw_line(int x1, int y1, int x2, int y2, color_t c) {

	// keep the coordinates within the bounds of the display.
	if (x1 < 0 || x1 >= res_width) { x1 = abs(x1 % res_width); }
	if (x2 < 0 || x2 >= res_width) { x1 = abs(x2 % res_width); }
	if (y1 < 0 || y1 >= res_height) { y1 = abs(y1 % res_height); }
	if (y2 < 0 || y2 >= res_height) { y2 = abs(y2 % res_height); }

	int dx = abs(x2-x1);						// get destination x length
	int sx = x1<x2 ? 1 : -1;					// get source x direction
	int dy = abs(y2-y1);						// get destination y length
	int sy = y1<y2 ? 1 : -1;					// get source y direction
	int err = (dx>dy ? dx : -dy)/2, e2;			// determine standard error for line

	while(TRUE) {
		sleep_ms(1);							// DEBUG
		draw_pixel(x1, y1, c);					// draw a point of the line
		if (x1==x2 && y1==y2) break;			// stop when reached the destination point
		e2 = err;								
		if (e2 >-dx) { err -= dy; x1 += sx; }	// determine new X point according to standard error
		if (e2 < dy) { err += dx; y1 += sy; }	// determine new Y point according to standard error
	}
}


/*
	Manipulate the display as a ROW MAJOR ORDER array. Inputs will be wrapped around
	the display using modulus calculation, just to keep from segmentation faults.

	To calculate each row, take the line length divided by the size
	in bytes dedicated for each pixel (color_t).
*/
void draw_pixel(int x, int y, color_t color) {
	if (x < 0 || x >= res_width) { x = abs(x % res_width); }	// keep within x boundary
	if (y < 0 || y >= res_height) { y = abs(y % res_height); }	// keep within y boundary	

	display_addr[(y * res_width) + x] = color;
}
/*
	Will get the framebuffer fb0, aka the mounted display device, and map it's address
	space for further manipulation. This happens by getting the resolution and bit-depth
	associated with the given display device and mapping into an array of LENGTHxWIDTH.

	The terminal is also manipulated by unsetting the CANONICAL and ECHO flags. This is done
	to make the input available immediately (without the user having to type a line-delimiter 
	character) by unsetting ICANON flag, and to not show any characters the user has typed on the
	screen by unsetting the ECHO flag.

	struct termios {
		tcflag_t c_iflag;		// input modes
    	tcflag_t c_oflag;		// output modes
    	tcflag_t c_cflag;		// control modes
    	tcflag_t c_lflag;		// local modes
    	cc_t c_cc[NCCS];		// control chars
	}
	int ioctl(int D, int REQUEST, ...);
	void *mmap(void *ADDR, size_t lengthint PROT, int FLAGS, int fd, off_t OFFSET);
*/
void init_graphics() {
	clear_screen();												// clear the terminal
	fd_display = open("/dev/fb0", O_RDWR);						// open display (framebuffer fb0)

	ioctl(fd_display, FBIOGET_VSCREENINFO, &display_res);		// get display resolution
	ioctl(fd_display, FBIOGET_FSCREENINFO, &display_depth);		// get display bit-depth

	res_height = display_res.yres_virtual;								// display-height resolution
	res_width = (display_depth.line_length/(sizeof *display_addr));		// display-width resolution
	screen_size = display_res.yres_virtual * display_depth.line_length;	// length x width (w/ bit depth)

	// map the opened display for manipulation, starting at offset 0 (map everything)
	display_addr = mmap(0, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_display, 0);

	struct termios terminal_settings;
	ioctl(1, TCGETS, &terminal_settings);	// get terminal settings from STDOUT
	terminal_settings.c_lflag &= ~(ICANON);	// unset ICANON flag
	terminal_settings.c_lflag &= ~(ECHO);	// unset ECHO flag
	ioctl(1, TCSETS, &terminal_settings);	// set terminal settings of STDOUT
}

/*
	Unmap the display device, set the terminal back, and then close the display
	file descriptor.

	int munmap(void *ADDR, size_t LENGTH);	
*/
void exit_graphics() {
	clear_screen();							// clear the screen and start cleaning up

	munmap(display_addr, screen_size);		// remove all mappings that contain pages in given address space

	struct termios terminal_settings;
	ioctl(1, TCGETS, &terminal_settings);	// get terminal settings
	terminal_settings.c_lflag &= ICANON;	// set ICANON flag
	terminal_settings.c_lflag &= ECHO;		// set ECHO flag
	ioctl(1, TCSETS, &terminal_settings);	// set terminal settings

	close(fd_display);						// close the display
}


/*
	Three independent sets of file descriptors are watched. Those listed in readfds 
	will be watched to see if characters become available for reading (more precisely, 
	to see if a read will not block; in particular, a file descriptor is also ready on 
	end-of-file), those in writefds will be watched to see if a write will not block, 
	and those in exceptfds will be watched for exceptions. 

	On exit, the sets are modified in place to indicate which file descriptors actually 
	changed status. 

	Each of the three file descriptor sets may be specified as NULL if no file descriptors 
	are to be watched for the corresponding class of events.

	Create a time interval of 0 seconds for constant polling of STDIN (keyboard); this
	will make the select() return immediately. Here is what will happen:

	"Hey, do you have input?" -- NO  -- immediately return NULL character '\0'
	"Hey, do you have input?" -- NO  -- immediately return NULL character '\0'
	"Hey, do you have input?" -- YES -- immediately return first character from STDIN
	"Hey, do you have input?" -- NO  -- immediately return NULL character '\0'

	int select(int NFDS, fd_set *READ_FDS, fd_set *WRITE_FDS, fd_set *EXCEPT_FDS, struct timeval *TIMEOUT);

	Where NFDS is the number of the filedescriptor to check, PLUS ONE (+1):

		[ STDIN=0 ]  [ STDOUT=1 ]  [ STDERR=2 ]
		   NFDS=1        NFDS=2        NFDS=3
*/
char getkey() {
	int c = '\0';			// default return value of NULL

	fd_set read_fds;		// create file descriptor for our READ operation
	FD_ZERO(&read_fds);		// init read_fds to zero (CLEAR)
	FD_SET(0, &read_fds);	// add read_fds to STDIN (0) aka keyboard fds

	struct timeval time_to_wait = {0, 0};
	if (select(1, &read_fds, NULL, NULL, &time_to_wait) {
		read(1, &c, sizeof(char));	//read int, but read size of char
	}

	return c;
}


/*
	Sleep for the given amount of milliseconds. It might sleep for less time because
	the cpu may issue an interrupt. In this case, we will not care about it and just
	return. (NULL) parameter used to not care about the interrupt. Otherwise, the
	time remaining will be held in this TIMESPEC parameter.

	struct timespec {
	    time_t tv_sec;		// seconds
	    long   tv_nsec;		// nanoseconds
	};
	int nanosleep(const struct timespec *REQ, struct timespec *REM);
*/
void sleep_ms(long ms) {
	struct timespec req = {0, ms*1000000};	// milliseconds to nanoseconds
	nanosleep(&req, NULL); 					// (NULL) says don't worry about interrupts
}