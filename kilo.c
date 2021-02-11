/************* Includes ************/
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h> 
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>

/**********defines****************/

#define CTRL_KEY(k) ((k)&0x1f)
#define KILO_VERSION "0.0.1"

enum editorKey
{
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	PAGE_UP,
	PAGE_DOWN,
	HOME_KEY , 
	END_KEY ,
	DEL_KEY , 
};

/************* data **************/
typedef struct erow { 
	int size ;
	char * chars ;
} erow ; 
struct editorConfig
{
	int cx, cy;
	int screenrows;
	int screencols;
	int numrows ;
	erow row ;
	struct termios orig_termios;
};

struct editorConfig E;

/************ terminal **********/

//error handling method

void die(const char *s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "x1b[H", 3);
	perror(s);
	exit(1);
}

//returning the terminal to its normal state
void disableRawMode()
{

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
		die("tcsetattr");
}
//enabling raw mode while desactivating canonincal mode
void enableRawMode()
{
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
		die("tcgetattr");
	atexit(disableRawMode);
	//now we got a terminal variable we set the attributes
	struct termios raw = E.orig_termios;
	//we turned efe the  echo  option that allows the characters we type to be displayed on screen
	raw.c_iflag &= ~(IXON | ICRNL | ISTRIP | INPCK | BRKINT);
	// not the same be careful the previous one is input flags
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	// turning off the the output flags that returns the cursor at the start of the line and insert a new line \n
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	// we  applied the changes
	// TCSAFLUSH : specifies when the output is written to the terminal (this case when the ouput is read)
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int editorReadKey() // reading the input by the editor
{
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN)
			die("read");
	}
	if (c == '\x1b')
	{
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1)
			return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1)
			return '\x1b';

		if (seq[0] == '[')
		{
			if (seq[1] >= '0' && seq[1] <= '9')
			{
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					return '\x1b';
				if (seq[2] == '~')
				{
					switch (seq[1])
					{
					case '1': case '7': return HOME_KEY ; 
					case '3': 			return DEL_KEY ;
					case '4': case '8': return END_KEY ; 
					case '5': 			return PAGE_UP;
					case '6': 			return PAGE_DOWN;
					}
				}
			}
				else
				{
					switch (seq[1])
					{
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY ;
					case 'F': return END_KEY ;
					}
				}
			}
			else if(seq[0]=='O')
			{
				switch(seq[1])
				{
					case 'H' : return HOME_KEY ;
					case 'F' : return END_KEY ; 
				}
			}
			return '\x1b';
		}
		else
		{
			return c;
		}
	}


	int getCursorPosition(int *rows, int *cols)
	{
		char buf[32];
		unsigned int i = 0;

		if (write(STDIN_FILENO, "\x1b[6n", 4) != 4)
			return -1;
		while (i < sizeof(buf) - 1)
		{
			if (read(STDIN_FILENO, &buf[i], 1) != 1)
				break;
			if (buf[i] == 'R')
				break;
			i++;
		}
		buf[i] = '\0';
		if (buf[0] != '\x1b' || buf[1] == '[')
			return -1;
		if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
			return -1;
		return 0;
	}

	int GetWindowSize(int *rows, int *cols) // returning the size of the terminal window
	{
		struct winsize ws;
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
		{
			if (write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
				return -1;
			return getCursorPosition(rows, cols);
		}

		else
		{
			*cols = ws.ws_col;
			*rows = ws.ws_row;
			return 0;
		}
	}

	/*********append buffer ******/

	struct abuf
	{
		char *b;
		int len;
	};

#define ABUF_INIT \
	{             \
		NULL, 0   \
	}

	void abAppend(struct abuf * ab, const char *s, int len)
	{
		// we allocate a new variable that will containt the text + the new addition
		char *new = realloc(ab->b, ab->len + len);

		if (new == NULL)
			return;

		//after copying we make sure that ab is updated
		memcpy(&new[ab->len], s, len);
		ab->b = new;
		ab->len += len;
	}

	/*destructor for our memory in b (in case the program is closed) */

	void abFree(struct abuf * ab)
	{
		free(ab->b);
	}

	/***********output********/
	void editorDrawRows(struct abuf * ab) // function that draws the dots at top right of the screen
	{
		int y;
		for (y = 0; y < E.screenrows; y++)
		{
			if (y == E.screenrows / 3)
			{
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome),
										  "Kilo editor -- verson %s ", KILO_VERSION);

				if (welcomelen > E.screencols)
					welcomelen = E.screencols;
				int padding = (E.screencols - welcomelen) / 2;
				{
					if (padding)
						abAppend(ab, "~", 1);
					padding--;
				}
				while (padding--)
					abAppend(ab, " ", 1);
				abAppend(ab, welcome, welcomelen);
			}

			else
			{
				abAppend(ab, "~", 1);
			}

			abAppend(ab, "\x1b[K", 3);
			if (y < E.screenrows - 1)
			{
				abAppend(ab, "\r\n", 2);
			}
		}
	}

	void editorRefreshScreen() // drawing the interface of the editor in the terminal
	{
		struct abuf ab = ABUF_INIT;
		abAppend(&ab, "\x1b[?25l", 6);
		abAppend(&ab, "\x1b[H", 3);

		editorDrawRows(&ab);

		char buf[32];
		snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
		abAppend(&ab, buf, strlen(buf));

		abAppend(&ab, "\x1b[?25h", 6);

		write(STDOUT_FILENO, ab.b, ab.len);
		abFree(&ab);
	}

	/**********input*********/
	void editorMoveCursor(int key)
	{
		/*TODO :  optimize it so it will run 
		for arrow keys upper and lower of a better version of vim*/
		switch (key)
		{
		case ARROW_LEFT:
			if (E.cx != 0)
				E.cx--;
			break;
		case ARROW_RIGHT:
			if (E.cx < E.screenrows)
				E.cx++;
			break;
		case ARROW_UP:
			if (E.cy != 0)
				E.cy--;
			break;
		case ARROW_DOWN:
			if (E.cy < E.screencols)
				E.cy++;
			break;
		}
	}
	void editorProcessKeypress() //  when a the user quits the terminal gets cleared out
	{
		int c = editorReadKey();
		switch (c)
		{
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "x1b[2J", 4);
			write(STDOUT_FILENO, "x1b[H", 3);
			exit(0);
			break;
		case HOME_KEY : E.cx = 0 ; break ;  
		case END_KEY  : E.cx = E.screencols ; break ;  
		
		
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break ;  		
		
		case PAGE_UP : case PAGE_DOWN :
		{
			int times = E.screencols ; 
			while (times--)
				{
					editorMoveCursor (c== PAGE_UP ? ARROW_UP : ARROW_DOWN) ;  
				}
		}
			break ; 


		}
	}

	/************* init *************/
	void initEditor() // simply getting the size of the terminal for now
	{
		E.cx = E.cy = 0; //setting the starting position of the cursor at the top left of the screen
		E.numrows = 0 ; 
		if (GetWindowSize(&E.screenrows, &E.screencols) == -1)
			die("getWindowSize");
	}

	int main()
	{
		enableRawMode();
		initEditor();
		while (1)
		{
			editorRefreshScreen();
			editorProcessKeypress();
		}

		return 0;
	}
