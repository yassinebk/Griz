/************* Includes ************/ 
#include<stdlib.h>
#include<sys/ioctl.h>
#include<errno.h>
#include<stdio.h>
#include<ctype.h>
#include<termios.h>
#include<unistd.h>
/**********defines****************/
#define CTRL_KEY(k) ((k)& 0x1f)
/************* data **************/
struct editorConfig{
 	struct termios orig_termios ; 
	 int screenrows ;
	int screencols; 
}; 
struct editorConfig E ; 

/************ terminal **********/
//error handling method  
void die(const char*s)
{
	write(STDOUT_FILENO,"\x1b[2J",4);
	write(STDOUT_FILENO,"x1b[H",3)	;
	perror(s); 
	exit(1); 
}

//returning the terminal to its normal state
void disableRawMode()
{

	if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.orig_termios )==-1) die("tcsetattr"); 
}
//enabling raw mode while desactivating canonincal mode  
void enableRawMode()	
{

if(	tcgetattr(STDIN_FILENO,&E.orig_termios) ==-1) die("tcgetattr") ;
	atexit(disableRawMode) ; 

//now we got a terminal variable we set the attributes
struct termios raw = E.orig_termios ; 	


//we turned off the  echo  option that allows the characters we type to be displayed on screen


	raw.c_iflag &= ~(IXON | ICRNL | ISTRIP|INPCK|BRKINT);
	// not the same be careful the previous one is input flags
	raw.c_lflag &=~(ECHO | ICANON | ISIG| IEXTEN ) ;
	// turning off the the output flags that returns the cursor at the start of the line and insert a new line \n 
	raw.c_oflag &=~(OPOST); 
	raw.c_cflag|=(CS8); 
	raw.c_cc[VMIN]= 0 ; 
	raw.c_cc[VTIME]= 20 ; 
	
// we  applied the changes 
// TCSAFLUSH : specifies when the output is written to the terminal (this case when the ouput is read) 
	tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) ; 
}

char editorReadKey()// reading the input by the editor 
	{
		int nread; 
		char c ; 
		while ((nread = read(STDIN_FILENO,&c,1))!=1)
					die("read"); 
		{
				if(nread ==-1 && errno != EAGAIN) die("read") ;
		}
		return c; 

		}
int GetWindowSize(int  *rows,int *cols) // returning the size of the terminal window
	{
		struct  winsize ws;
		if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws) ==-1 || ws.ws_col==0)
		return -1 ;
		else {
			*cols = ws.ws_col ;
			*rows = ws.ws_row ;
			return 0; 

		}
		
	}
/***********output********/
void editorDrawRows() // function that draws the dots at top right of the screen 
{
	int y ; 
	for( y =0 ; y<E.screenrows;y++)
		{
			write(STDOUT_FILENO,"~\r\n",3);
		}
}
void editorRefreshScreen()// drawing the interface of the editor in the terminal 
{
	write(STDOUT_FILENO,"\x1b[2J",4);
	write(STDOUT_FILENO,"\x1b[H",3);
	editorDrawRows();
	write(STDOUT_FILENO,"\x1b[H",3);
	
}

	
/**********input*********/
void editorProcessKeypress() //  when a the user quits the terminal gets cleared out
	{
		char c = editorReadKey(); 
		switch(c)
		{
			
			case CTRL_KEY('q'):
			write(STDOUT_FILENO,"x1b[2J",4);
			 write(STDOUT_FILENO,"x1b[H",3);
			  exit(0);


			break ; 
		}
	}

/************* init *************/
void initEditor() // simply getting the size of the terminal for now
	{
		if(GetWindowSize(&E.screenrows,&E.screencols)==-1 ) die("getWindowSize") ;
	}
int main()	

{
	enableRawMode()	;  
	initEditor();

	while(1)	
	{
		editorRefreshScreen(); 
		editorProcessKeypress(); 
	}


	return 0 ; 
}


