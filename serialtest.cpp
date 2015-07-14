#include <ctype.h>
#include <stdio.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>			//Used for UART
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <wiringPi.h>
#include <math.h>
#include <sys/mman.h>
#include <fcntl.h>

int uart0_filestream = -1;
struct termios options;
unsigned char fragment[255];
int fragment_length=0;

void setup(void){
	//-------------------------
	//----- SETUP USART 0 -----
	//-------------------------
	//At bootup, pins 8 and 10 are already set to UART0_TXD, UART0_RXD (ie the alt0 function) respectively
	
	//OPEN THE UART
	//The flags (defined in fcntl.h):
	//	Access modes (use 1 of these):
	//		O_RDONLY - Open for reading only.
	//		O_RDWR - Open for reading and writing.
	//		O_WRONLY - Open for writing only.
	//
	//	O_NDELAY / O_NONBLOCK (same function) - Enables nonblocking mode. When set read requests on the file can return immediately with a failure status
	//											if there is no input immediately available (instead of blocking). Likewise, write requests can also return
	//											immediately with a failure status if the output can't be written immediately.
	//
	//	O_NOCTTY - When set and path identifies a terminal device, open() shall not cause the terminal device to become the controlling terminal for the process.
	uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1)
	{
		//ERROR - CAN'T OPEN SERIAL PORT
		printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
	}
	
	//CONFIGURE THE UART
	//The flags (defined in /usr/include/termios.h - see http://pubs.opengroup.org/onlinepubs/007908799/xsh/termios.h.html):
	//	Baud rate:- B1200, B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000, B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000, B4000000
	//	CSIZE:- CS5, CS6, CS7, CS8
	//	CLOCAL - Ignore modem status lines
	//	CREAD - Enable receiver
	//	IGNPAR = Ignore characters with parity errors
	//	ICRNL - Map CR to NL on input (Use for ASCII comms where you want to auto correct end of line characters - don't use for bianry comms!)
	//	PARENB - Parity enable
	//	PARODD - Odd parity (else even)
	tcgetattr(uart0_filestream, &options);
	options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;		//<Set baud rate
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(uart0_filestream, TCIFLUSH);
	tcsetattr(uart0_filestream, TCSANOW, &options);
}

bool checksum(unsigned char* s)
{
	unsigned char sum = 0;
	int failsafe=0; //make sure we don't keep just reading characters
	s++;
	while ((*s != 42)&&(*s != 0)&&(failsafe<200))
	{
		sum ^= *s;
		s++;
		failsafe++;
	}
	s++;
	int check=strtol((char*)s,NULL,16);
	if (check==sum)
		return 1;
	else{
		printf("Check-sum Failed: %x %x %s\n",check,sum,s);
		return 0;
	}
}

void parse(unsigned char input_string[]){
	const char delimiters[] = ",*";
	char *running;
	char *token;
	double minutes,deg,speed;
	
	//printf("string: %s",input_string);
	running = strdup ((char*)input_string);
	token = strsep (&running, delimiters);
	if (strcmp(token, "$GPRMC") == 0) 
	{
		printf("%s",input_string);
		printf("Recommended Minimum Data:\n");
		token = strsep (&running, delimiters);  
		if (strlen(token)==10){
			printf("Time: %c%c:%c%c:%c%c UTC\n",token[0],token[1],token[2],token[3],token[4],token[5]);
		} else{
			printf("Invalid Time Field Length\n");
		}
		token = strsep (&running, delimiters);  
		if (*token=='A'){
			printf("Status: Active\n");
		} else if (*token=='V'){
			printf("Status Void\n");
		} else {
			printf("Status Invalid");
		}

		minutes = atof(strsep (&running, delimiters));
		deg = trunc(minutes/100);
		minutes -=deg*100;
		token = strsep (&running, delimiters);  
		printf("Lat : %.0f deg %02.4f' %c\n",deg ,minutes,*token); 
 
		minutes = atof(strsep (&running, delimiters));
		deg = trunc(minutes/100);
		minutes -=deg*100;
		token = strsep (&running, delimiters);  
		printf("Long : %.0f deg %02.4f' %c\n",deg ,minutes,*token); 

		speed = atof(strsep (&running, delimiters));
		deg = atof(strsep (&running, delimiters));
		printf("Velocity: %3.2f knots at %3.2f deg True\n",speed,deg);

		token = strsep (&running, delimiters);  
		if (strlen(token)==6){
			printf("Date: %c%c/%c%c/20%c%c\n",token[0],token[1],token[2],token[3],token[4],token[5]);
		} else{
			printf("Invalid Date Field Length\n");
		}
		token = strsep (&running, delimiters); //this gps unit will not have magnetic variation 
		token = strsep (&running, delimiters); //this gps unit will not have magnetic variation 
		token = strsep (&running, delimiters); // A=Autonomous D=differential, E=Estimated, N=not valid, S=Simulator
		if (*token=='A'){
			printf("Autonomous Fix\n");
		} else if (*token=='D'){ 
			printf("Differential Fix\n");
		} else if (*token=='E'){ 
			printf("Estimated Fix\n");
		} else if (*token=='N'){ 
			printf("Not Valid Fix\n");
		} else if (*token=='S'){ 
			printf("Simulator Fix\n");
		} else {
			printf("Invalid Fix Status\n");
		}
	} 
	else if (strcmp(token, "xxx") == 0)
	{
		// do something else
	}
		/* more else if clauses */
	else /* default: */
	{
		printf("  ?: %s",input_string);
	}
}

void receive(void){
	//----- CHECK FOR ANY RX BYTES -----
	if (uart0_filestream != -1)
	{
		// Read up to 255 characters from the port if they are there
		unsigned char rx_buffer[256];
		int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
		if (rx_length < 0)
		{
			//An error occured (will occur if there are no bytes)
		}
		else if (rx_length == 0)
		{
			//No data waiting
		}
		else
		{
			//Bytes received
			int i;
			for (i=0;i<rx_length;i++){
				fragment[fragment_length]=rx_buffer[i];
				fragment_length++;
				if (rx_buffer[i]==10){
					fragment[fragment_length] = '\0';
					if (checksum(fragment)){
						parse(fragment);
						//printf("Valid - %s",fragment);
					}
					fragment_length=0;
				}
			}
			//rx_buffer[rx_length] = '\0';
			//printf("\n%i bytes read : %s\n\n", rx_length, rx_buffer);
		}
	}
}

void send(void){
	//----- TX BYTES -----
	unsigned char tx_buffer[20];
	unsigned char *p_tx_buffer;
	
	p_tx_buffer = &tx_buffer[0];
	*p_tx_buffer++ = 'H';
	*p_tx_buffer++ = 'e';
	*p_tx_buffer++ = 'l';
	*p_tx_buffer++ = 'l';
	*p_tx_buffer++ = 'o';
	
	if (uart0_filestream != -1)
	{
		int count = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));		//Filestream, bytes to write, number of bytes to write
		if (count < 0)
		{
			printf("UART TX error\n");
		}
	}
}

int main(void){
	setup();
	delay(3);
	while(1){
		receive();
	}
}

