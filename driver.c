#include "library.c"

/* REFERENCES
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

[X] draw_line(int x2, int y2, int X2, int Y2, color_t C)

	USING draw_pixel(), MAKES A LINE FROM (x2,y2) TO (X2,Y2) USING BRESENHAM'S ALGORITHM INTEGER MATH.

	[X] Implement Bresenham's algorithm using only integer math.
	[X] Make sure the implmentation works for all valid coordinates and slopes.

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
	x2 = 839; y2 = 779;
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