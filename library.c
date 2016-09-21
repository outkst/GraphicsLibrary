#include <stdio.h>		/* printf() */
#include <sys/types.h>	/* open() select() */
#include <time.h>		/* nanosleep() */
#include <unistd.h>		/* read() write() */
#include <sys/stat.h>	/* open() */
#include <fcntl.h>		/* open() */

/* 
	Joe Meszar (jwm54@pitt.edu)
	CS1550 Project 1 (FALL 2016)

[ ] init_graphics()
	
	INITIALIZE THE GRAPHICS LIBRARY

	[ ] Open the graphics device aka framebuffer (/dev/fb0).
	[ ] Map a file into address space so that we can treat it as an array.
		mmap() memory-mapping
			TAKES: a file descriptor from open()
			RETURNS: void * address space that represents the contents of the file.
		[ ] Use MAP_SHARED param b/c other parts of OS want to use the framebuffer too
	[ ] Use pointer arithmetic OR array subscripting to set each individual pixel
		[ ] How many pixels are there?
		[ ] How many bytes of data are associated with each pixel?

	[ ] The Screen is 640x480 (lengthxheight), 16-bit color.
		[ ] Create a a typedef color_t that is an unsigned 16bit value
		[ ] Make a macro/function to encode color_t from three RGB values
			[ ] use bit-shifting and masking to make a single 16-bit number
		[ ] Get screen size with system call ioctl()
			it queries and sets parameters for almost any device connected.
			TAKES: file descriptor AND a number that represents the request you're making:
				FBIOGET_VSCREENINFO: returns struct fb_var_screeninfo with virtual resolution.
				FBIOGET_FSCREENINFO: returns struct fb_fix_screeninfo to determin bit depth
		[ ] Determine the bit depth from returned struct fb_fix_screeninfo
		[ ] Total size of mmap()'ed file is: fb_var_screeninfo.yres_virtual * fb_fix_screeninfo.line_length
	[ ] Use ioctl() syscall to disable keypress echo which displays the keys are you're typing
		[ ] buffer the keypresses instead using TCGETS and TCSETS
		[ ] TCGETS and TCSETS take a struct termios describign the current terminal settings
		[ ] Disable CANONICAL MODE by unsetting the ICANON bit
		[ ] Disable echo'ing by unsetting the ECHO bit
		[ ] exit_graphics() will clean up terminal settings by restoring ICANON and ECHO
		[ ] Create a helper program called FIX to turn ECHO and ICANON bits back on in case of error.

[ ] exit_graphics()
	
	WILL UNDO WHATEVER CHANGES TO TERMINAL WERE MADE, CLOSE FILES, AND CLEAN UP MEMORY

	[ ] Use close() on files
	[ ] Use munmap on memory mapped file
	[ ] Make ioctl() call and re-set canonical mode by setting the struct termios.ICANON bit
	[ ] Make ioctl() call and re-set echo mode by setting the struct termios.ECHO bit

[ ] clear_screen()

	USE ANSI ESCAPE CODE TO CLEAR THE SCREEN RATH THAN BLANKING IT

	[ ] Use the ANSI escape code "\033[2J" to tell the terminal to clear itself

[ ] getkey()
	
	GATHER KEY PRESS INPUT BY READING A SINGLE CHARACTER

	[ ] First, implement the non-blocking syscall select()
		it polls a given standard input/output/error to see if it will block or not
	[ ] Read a single character from input using read()

[ ] sleep_ms()

	MAKE THE PROGRAM SLEEP BETWEEN FRAMES OF GRAPHICS BEING DRAWN

	[ ] Use syscall nanosleep()
	[ ] sleep for a number of milliseconds, not nanoseconds, by multiplying by 1,000,000
	[ ] Make nanosleep second parameter NULL b/c we aren't worried about the call being interrupted

[ ] draw_pixel()
	
	MAIN DRAWING CODE, SET THE PIXEL AT A COORDINATE(X,Y) TO THE SPECIFIED COLOR.

	[ ] Use the given X,Y coordinates to scale the base address of the memory-mapped framebuffer using pointer arithmetic
		- The framebuffer is stored in ROW-MAJOR ORDER: first row starts at offset 0

[ ] draw_line()

	USING draw_pixel(), MAKES A LINE FROM (X1,Y1) TO (X2,Y2) USING BRESENHAM'S ALGORITHM INTEGER MATH.

	[ ] Implement Bresenham's algorithm using only integer math.
	[ ] Make sure the implmentation works for all valid coordinates and slopes.

[ ] draw_text()
	
	DRAW A STRING WITH THE SPECIFIED COLOR AT THE LOCATION (X,Y); THE UPPER-LEFT CORNER OF THE FIRST LETTER.

	[ ] Draw starting with location (x,y) which is the upper-left corner of the first letter
		Each letter is 8x16 (widthxheight) enchoded as 16 1-byte integers
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
typedef unsigned short color_t; // store RGB color


/* get the udev, mmap it */
void clear_screen();										/* super easy, one line */
void draw_pixel(int x, int y, color_t color);				/* do next, make sure memory pointers correct */
void draw_line(int x1, int y1, int x2, int y2, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void init_graphics();
void exit_graphics();
char getkey();
void sleep_ms(long ms);

int main() {
	long t = 1;
	sleep_ms(t);			// sleep for t seconds

	char c;
	c = getkey();			// see if a key was pressed
	printf("key: %c", c);

	clear_screen();			// clear the terminal

	return 0;
}

/*
	Write the ANSI escape code to the screen to clear it. 
	The screen is STDOUT aka fd=1

	ssize_t write(int fd, const void *buf, size_t count);
*/
void clear_screen() {
	write(1,"\033[2J",8);
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

	select(NFDS, READ_FDS, WRITE_FDS, EXCEPT_FDS, TIMEOUT)

	Where NFDS is the number of the filedescriptor to check, PLUS ONE (+1):

		[ STDIN=0 ]  [ STDOUT=1 ]  [ STDERR=2 ]
		   NFDS=1        NFDS=2        NFDS=3
*/
char getkey() {
	char c = '\0';			// default return value of NULL
	char buff[2] = {0};		// two byte buffer (1 input char + newline)

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
	Sleep for the given amount of milliseconds.

	#include <time.h>
	struct timespec {
	    time_t tv_sec;		// seconds
	    long   tv_nsec;		// nanoseconds
	};
	int nanosleep(const struct timespec *req, struct timespec *rem);
*/
void sleep_ms(long ms) {
	struct timespec req = {0, ms*1000000};	// milli to nanoseconds
	nanosleep(&req, NULL); 					// don't worry about interrupts
}