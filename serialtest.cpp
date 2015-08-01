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
#include <parser.h>

int uart0_filestream = -1;
struct termios options;
unsigned char fragment[255];
int fragment_length=0;

nmeaINFO info;
nmeaPARSER parser;

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

    nmea_zero_INFO(&info);
    nmea_parser_init(&parser);


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

time_t get_time_t(nmeaTIME &nt) {
	tm ct;
	ct.tm_year = nt.year;
	ct.tm_mon = nt.mon;
	ct.tm_mday = nt.day;
	ct.tm_hour = nt.hour;
	ct.tm_min = nt.min;
	ct.tm_sec = nt.sec;
	return mktime(&ct) - timezone;
}

void print_details() {
	time_t t1, t2;
	t1 = get_time_t(info.utc);
	t2 = time(NULL);

	printf("time %d-%d-%d %02d:%02d:%02d:%02d diff %d\n", info.utc.year+1900, info.utc.mon+1, info.utc.day+1, info.utc.hour, info.utc.min, info.utc.sec, info.utc.hsec, t1 - t2);
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
						nmea_parse(&parser, (char*)fragment, fragment_length, &info);
						//printf("Valid - %s",fragment);
						print_details();
					}
					fragment_length=0;
				}
			}
			//rx_buffer[rx_length] = '\0';
			//printf("\n%i bytes read : %s\n\n", rx_length, rx_buffer);
		}
	}
}

void send(const char *sendstring){
	if (uart0_filestream != -1)
	{
		int count = write(uart0_filestream, sendstring, strlen(sendstring));		//Filestream, bytes to write, number of bytes to write
		if (count < 0) { printf("UART TX error\n"); }
	}
}

int main(void){
	setup();
	delay(1);
	send("$PMTK314,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n"); // only send the RMC sentences
	send("$PMTK300,200,0,0,0,0*2F\r\n"); // Increase update rate to 5Hz (200ms) for microstack GPS
	send("$PMTK220,200*2C\r\n"); // Increase update rate to 5Hz (200ms) for adafruit GPS
	while(1){
		receive();
	}
}

