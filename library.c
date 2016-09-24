#include <fcntl.h>		/* open() */
#include <termios.h>	/* TCGETS TCSETS */
#include <time.h>		/* nanosleep() */
#include <unistd.h>		/* read() write() */
#include <linux/fb.h>	/* FB_VAR_SCREENINFO FB_FIX_SCREENINFO */
#include <sys/ioctl.h>	/* ioctl() */
#include <sys/mman.h>	/* PROT_READ PROT_WRITE */
#include <sys/stat.h>	/* open() */
#include <sys/types.h>	/* open() select() */


#include <stdio.h>		/* DEBUG */
/* REFERENCES
	(DELETE) FRAMEBUFFER: https://gist.github.com/FredEckert/3425429
	TERMIOS: https://blog.nelhage.com/2009/12/a-brief-introduction-to-termios-termios3-and-stty/

*/

/* 
	Linux Graphics Library

	Joe Meszar (jwm54@pitt.edu)
	CS1550 Project 1 (FALL 2016)
*/

/*
Zip project into USERNAME-project1.tar.gz and submit to ~jrmst106/submit/1550
include a driver.c that runs everything

[X] init_graphics()
	
	INITIALIZE THE GRAPHICS LIBRARY

	[X] Open the graphics device aka framebuffer (/dev/fb0) using open().
	[X] Map a file into address space so that we can treat it as an array.
		mmap() memory-mapping
			TAKES: a file descriptor from open()
			RETURNS: void * address space that represents the contents of the file.
		[X] Use MAP_SHARED param b/c other parts of OS want to use the framebuffer too
	[X] Use pointer arithmetic OR array subscripting to set each individual pixel
		[X] How many pixels are there?
		[X] How many bytes of data are associated with each pixel?

	[X] Find the screen resolution and bit depth programmatically.
		[X] Create a typedef color_t that is an unsigned 16bit value
		[X] Make a macro/function to encode color_t from three RGB values
			[X] use bit-shifting and masking to make a single 16-bit number
		[X] Get screen size with system call ioctl()
			it queries and sets parameters for almost any device connected.
			TAKES: file descriptor AND a number that represents the request you're making:
				FBIOGET_VSCREENINFO: returns struct fb_var_screeninfo with virtual resolution.
				FBIOGET_FSCREENINFO: returns struct fb_fix_screeninfo to determin bit depth
		[X] Determine the bit depth from returned struct fb_fix_screeninfo
		[X] Total size of mmap()'ed file is: fb_var_screeninfo.yres_virtual * fb_fix_screeninfo.line_length
	[X] Use ioctl() syscall to disable keypress echo which displays the keys are you're typing
		[X] buffer the keypresses instead using TCGETS and TCSETS
		[X] TCGETS and TCSETS take a struct TERMIOS describing the current terminal settings
		[X] Disable CANONICAL MODE by unsetting the ICANON bit
		[X] Disable echo'ing by unsetting the ECHO bit

[X] exit_graphics()
	
	WILL UNDO WHATEVER CHANGES TO TERMINAL WERE MADE, CLOSE FILES, AND CLEAN UP MEMORY

	[X] Use close() on files
	[X] Use munmap on memory mapped file
	[X] Make ioctl() call and re-set canonical mode by setting the struct TERMIOS ICANON bit
	[X] Make ioctl() call and re-set echo mode by setting the struct TERMIOS ECHO bit

[X] clear_screen()

	USE ANSI ESCAPE CODE TO CLEAR THE SCREEN RATH THAN BLANKING IT

	[X] Use the ANSI escape code "\033[2J" to tell the terminal to clear itself

[X] getkey()
	
	GATHER KEYPRESS INPUT BY READING A SINGLE CHARACTER

	[X] First, implement the non-blocking syscall select()
		it polls a given standard input/output/error to see if it will block or not
	[X] Read a single character from input using read() if input available

[X] sleep_ms(long MS)

	MAKE THE PROGRAM SLEEP BETWEEN FRAMES OF GRAPHICS BEING DRAWN

	[X] Use syscall nanosleep()
	[X] sleep for a number of milliseconds, not nanoseconds, by multiplying by 1,000,000
	[X] Make nanosleep second parameter NULL b/c we aren't worried about the call being interrupted

[X] draw_pixel(int X, int Y, color_t COLOR)
	
	MAIN DRAWING CODE, SET THE PIXEL AT A COORDINATE(X,Y) TO THE SPECIFIED COLOR.

	[X] Use the given X,Y coordinates to scale the base address of the memory-mapped framebuffer using pointer arithmetic
		- The framebuffer is stored in ROW-MAJOR ORDER: first row starts at offset 0

[ ] draw_line(int x2, int y2, int X2, int Y2, color_t C)

	USING draw_pixel(), MAKES A LINE FROM (x2,y2) TO (X2,Y2) USING BRESENHAM'S ALGORITHM INTEGER MATH.

	[ ] Implement Bresenham's algorithm using only integer math.
	[ ] Make sure the implmentation works for all valid coordinates and slopes.

[ ] draw_text(int X, int Y, const char *TEXT, color_t C)
	
	DRAW A STRING WITH THE SPECIFIED COLOR AT THE LOCATION (X,Y); THE UPPER-LEFT CORNER OF THE FIRST LETTER.

	[ ] Draw starting with location (x,y) which is the upper-left corner of the first letter
		Each letter is 8x26 (widthxheight) enchoded as 16 1-byte integers
	[ ] Use the Apple font encoded into an array in iso_font.h 
	[ ] Use the array defined in iso_font.h
		- e.g. The 16 values for the letter 'A' (ASCII 65) is found at indices (65)*(16+0) to (65)*(16+15)
	[ ] Use shifting and masking to go through each bit of the 16 rows and draw a pixel at the appropriate coordinate, if the bit is a 1.
	[ ] Break into two functions, one for drawing a single character, and then the required one for drawing all of the characters in a string.
	[ ] DO NOT USE STRLEN() since it is a C standard library function.
	[ ] Iterate until you find '\0'
	[ ] Do not worry about line-breaking
*/


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
#define TRUE 1					// fuck C

int main() {
	init_graphics();

	int x, y;
	color_t color = 0xF800;
	for (x=0; x<640; x++) {
		for (y=0; y<480; y++) {
			draw_pixel(x, y, RMASK(color) | GMASK(color) | BMASK(color));
		}
	}
	sleep_ms(1000);
	color = 0x17E0;
	for (y=0; y<480; y++) {
		for (x=0; x<640; x++) {
			draw_pixel(x, y, RMASK(color) | GMASK(color) | BMASK(color));
		}
	}
	sleep_ms(1000);

	int x1, x2, y1, y2;
	x1 = 100; y1 = 200;
	x2 = 639; y2 = 479;
	color = 0xF800;
	draw_line(x1, y1, x2, y2, RMASK(color) | GMASK(color) | BMASK(color));

	//exit_graphics();

	/* DEBUG */
	//printf("(y * display_res.xres_virtual) + x: %d\n", ((479 * display_res.xres_virtual) + 639));
	//printf("(y * (display_depth.line_length/(sizeof *display_addr))) + x: %d\n", ((479 * (display_depth.line_length/(sizeof *display_addr))) + 639));
	// printf("sizeof(*display_addr): %d\n", (sizeof *display_addr));
	// printf("display_res.yres: %d\n", display_res.yres);
	// printf("display_res.xres: %d\n", display_res.xres);
	// printf("display_res.yres_virtual: %d\n", display_res.yres_virtual);
	// printf("display_res.xres_virtual: %d\n", display_res.xres_virtual);
	// printf("display_depth.line_length: %d\n", display_depth.line_length);
	return 0;
}


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

	res_height = display_res.yres_virtual;								// display height resolution
	res_width = (display_depth.line_length/(sizeof *display_addr));		// display width resolution
	screen_size = display_res.yres_virtual * display_depth.line_length;	// length x width (w/ bit depth)

	// map the opened display for manipulation, starting at offset 0 (map everything)
	display_addr = mmap(0, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_display, 0);

	struct termios terminal_settings;
	ioctl(fd_display, TCGETS, &terminal_settings);	// get terminal settings
	terminal_settings.c_lflag &= ~(ICANON);			// unset ICANON flag
	terminal_settings.c_lflag &= ~(ECHO);			// unset ECHO flag
	ioctl(fd_display, TCSETS, &terminal_settings);	// set terminal settings
}

/*
	Unmap the display device, set the terminal back, and then close the display
	file descriptor.

	int munmap(void *ADDR, size_t LENGTH);	
*/
void exit_graphics() {
	clear_screen();									// clear the screen and start cleaning up

	munmap(display_addr, screen_size);				// remove all mappings that contain pages in given address space

	struct termios terminal_settings;
	ioctl(fd_display, TCGETS, &terminal_settings);	// get terminal settings
	terminal_settings.c_lflag &= ICANON;			// set ICANON flag
	terminal_settings.c_lflag &= ECHO;				// set ECHO flag
	ioctl(fd_display, TCSETS, &terminal_settings);	// set terminal settings

	close(fd_display);								// close the display
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
	char c = '\0';			// default return value of NULL

	fd_set read_fds;		// create file descriptor set for STDIN
	FD_ZERO(&read_fds);		// init read_fds to zero (CLEAR)
	FD_SET(0, &read_fds);	// add read_fds to STDIN (0) aka keyboard fds

	struct timeval time_to_wait = {1, 0};
	if (select(1, &read_fds, NULL, NULL, &time_to_wait)) {
		read(0, &c, 1); 	// get first character from STDIN
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