#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <wiringPi.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <parser.h>

#define BCM2708_ST_BASE 0x3F003000 /* BCM 2835 System Timer */
#define GPS_PPS 4
#define CS 8
#define CLK 11
#define	MOSI 10
#define	MISO 9
#define	delay_time 50

volatile unsigned *TIMER_registers;
unsigned int last_time=0;
unsigned int this_time=0;
unsigned int delta=0;
float moving_offset=8388608.0;
bool tick_raw;
bool tick_registered;
bool new_code;

nmeaPARSER p;

unsigned int TIMER_GetSysTick()
{	return TIMER_registers[1];	}

void TIMER_Init()
{
    /* open /dev/mem */
    int TIMER_memFd;
    if ((TIMER_memFd = open("/dev/mem", O_RDWR/*|O_SYNC*/) ) < 0)
    {
        printf("can't open /dev/mem - need root ?\n");
        exit(-1);
    }

    /* mmap BCM System Timer */
    void *TIMER_map = mmap(
        NULL,
        4096, /* BLOCK_SIZE */
        PROT_READ /*|PROT_WRITE*/,
        MAP_SHARED,
        TIMER_memFd,
        BCM2708_ST_BASE
    );

    close(TIMER_memFd);

    if (TIMER_map == MAP_FAILED)
    {
        printf("mmap error %d\n", (int)TIMER_map);
        exit(-1);
    }
    TIMER_registers = (volatile unsigned *)TIMER_map;
}

bool bit(bool send_value){
	if (send_value){
		digitalWrite (MOSI, HIGH);
		digitalWrite (MOSI, HIGH);
	}else{
		digitalWrite (MOSI, LOW);
		digitalWrite (MOSI, LOW);
	}
	digitalWrite (CLK, LOW);
	digitalWrite (CLK, LOW);
	if (digitalRead (MISO) == LOW){
		digitalWrite (CLK, HIGH);
		digitalWrite (CLK, HIGH);
		return 0;
	}else{
		digitalWrite (CLK, HIGH);
		digitalWrite (CLK, HIGH);
		return 1;
	}
}
unsigned long long int bytez(unsigned long long int send_byte,int bytes){
	unsigned long long int return_byte = 0;
	unsigned long long int bitmask = pow(2,(bytes*8-1));
	int i;
	for (i=0; i<(8*bytes); i++){
		return_byte <<= 1;
		if (bit(send_byte&bitmask)){
				return_byte += 1;
		}
		send_byte <<= 1;
	}
	return return_byte;
}
void printbytes(unsigned int print_byte, int bytes){
	printf("%012u %8x ", TIMER_GetSysTick(),print_byte);
	while (print_byte) {
		if (print_byte & 1)
			printf("1");
		else
			printf(".");
		print_byte >>= 1;
	}
	printf("\n");
}

void linebytes(unsigned int print_byte, int gain, float offset){
	int offset_value=(int)(print_byte-offset);
	long long int scale_value=offset_value*pow(2,gain);
	unsigned int reoffset_value=(scale_value+0x820000); //0x800000 to centre and 0x20000 to un-dither
	unsigned int display_value=(reoffset_value)>>18;
	int i;
	//printf("%12x %12d %12lld %12d %12d %10.3f |",print_byte, offset_value, scale_value, reoffset_value, display_value, offset);
	if (display_value<=0)
		printf("<");
	else
		printf("|");
	for (i=1; i<64; i++){
		if (i==display_value)
			printf("*");
		else
			printf(" ");
	}
	if (display_value>=64)
		printf(">");
	else
		printf("|");
	if (tick_raw){
		printf("---\n");
		tick_raw=0;
	}else{
		printf("\n");
	}
}

void wigglebytes(unsigned int print_byte, int bytes){
	unsigned int byte1=((print_byte&0xff0000)>>16)-96;
	//unsigned int byte1=(print_byte&0xff0000)>>18;
	//unsigned int byte2=(print_byte&0xff00)>>10;
	//unsigned int byte3=(print_byte&0xff)>>2;
	int i;
	printf("%12x |",print_byte);
	for (i=0; i<64; i++){
		if (i==byte1)
			printf("O");
		//else if (i==byte2)
		//	printf("o");
		//else if (i==byte3)
		//	printf(".");
		else
			printf(" ");
	}
	printf("|\n");
}

unsigned int register_read(int reg, int bytes){
	int ignore = bytez((0x40 + (reg<<3)),1); /*command*/
	return bytez(0,bytes); /*data*/
}
void register_write(int reg, int bytes, unsigned int write_data){
	printf("write\n");
	unsigned int ignore = bytez((0x00 + (reg<<3)),1); /*command*/
	unsigned int mask = pow(2,(bytes*8))-1;
	ignore = bytez((write_data&mask),bytes); /*data*/
	printf("write b\n");
}

void IO_Init(void){
	wiringPiSetupGpio();
	pinMode (MISO, INPUT) ;
	pinMode (MOSI, OUTPUT) ;
	digitalWrite (MOSI, LOW);
	pinMode (CLK, OUTPUT) ;
	digitalWrite (CLK, LOW);
	pinMode (CS, OUTPUT) ;
	digitalWrite (CS, LOW);
	pinMode (GPS_PPS, INPUT) ;
}

void AD9172_Interface(void){
	printf("Here we go!\n");
	char stringinput[1], RW[1];
	unsigned int value, datainput, mask, commandByte, inputWord, ignoreCommandResponse, ignoreDataResponse,dataResponse, valid, bytes, reg;
	//delay(1000);
	while (1){
		printf("(R)ead (W)rite re(S)et (T)imed_read tr(A)ce (L)ine: ");
		scanf("\n%c",&RW);
		if ((tolower(RW[0]) == 'r') || (tolower(RW[0]) == 'w'))
		{
			valid = 1;
			printf("Which register (S)tatus (M)ode (C)onfig (D)ata data+stat(U)s (I)d (G)pocon (O)ffset (F)ull-scale: ");
			scanf("\n%c",&stringinput);
			switch(tolower(stringinput[0])){
			case 's' :
					bytes = 1; reg = 0; break;
			case 'm' :
					bytes = 3; reg = 1; break;
			case 'c' :
					bytes = 3; reg = 2; break;
			case 'd' :
					bytes = 3; reg = 3; break;
			case 'u' :
					bytes = 4; reg = 3; break;
			case 'i' :
					bytes = 1; reg = 4; break;
			case 'g' :
					bytes = 1; reg = 5; break;
			case 'o' :
					bytes = 3; reg = 6; break;
			case 'f' :
					bytes = 3; reg = 7; break;
			default :
					valid == 0;
					printf("%c\n",*stringinput);
					printf("no idea what you just pressed o.O\n");
			}
		}
		if (tolower(RW[0]) == 'r')
		{
			if (valid){
				value = register_read(reg, bytes);
				printbytes(value,bytes);
			}
		}
		else if (tolower(RW[0]) == 'w')
		{
			if (valid){
				printf("a\n");
				printf("%d bytes of data (in hex): ",bytes);
				printf("a\n");
				scanf("%x",datainput);
				printf("a\n");
				register_write(reg, bytes, datainput);
				printf("a\n");
			}
		}else if (tolower(RW[0]) == 's'){
			printf("Resetting...");
			value = bytez(0xffffffffff,5);
			printf(" ...Done\n");
		}else if (tolower(RW[0]) == 't'){
			printf("Continuous timed read:\n");
			while (1){
				delay(delay_time);
				ignoreCommandResponse = bytez(88,1);
				dataResponse = bytez(0xffffff,3);
				printbytes(dataResponse,3);
			}
		}else if (tolower(RW[0]) == 'a'){
			printf("Continuous trace:\n");
			while (1){
				delay(delay_time);
				ignoreCommandResponse = bytez(88,1);
				dataResponse = bytez(0xffffff,3);
				wigglebytes(dataResponse,3);

			}
		}else if (tolower(RW[0]) == 'l'){
			printf("Continuous timed read:\n");
			while (1){
				delay(delay_time);
				ignoreCommandResponse = bytez(88,1);
				dataResponse = bytez(0xffffff,3);
				moving_offset=moving_offset*0.99+dataResponse*0.01;
				linebytes(dataResponse,0,moving_offset);
			}
		}else{
			printf("%c\n",*RW);
			printf("huh?\n\n");
		}
	}
}

void Delta_Time(void){
	this_time=TIMER_GetSysTick();
	delta=this_time-last_time;
	//printf("interrupt time: %d\n", delta);
	last_time=this_time;
	tick_raw=1;
}

void Test_1M(void){
	int i=0;
	int start_time;
	int stop_time;
	float ave_time;
	while(1){
		start_time=TIMER_GetSysTick();
		for (i=0;i<1000;i++){
			//routine to time
		}
		stop_time=TIMER_GetSysTick();
		ave_time=(stop_time-start_time)/i;
		printf("time for 1M x routine is :%6.2fus\n",ave_time);
	}
}

int main (void){
	TIMER_Init();
	IO_Init();
	wiringPiISR (GPS_PPS, INT_EDGE_RISING, &Delta_Time) ;

	//AD9172_Interface();

	unsigned int value, datainput, mask, commandByte, inputWord, ignoreCommandResponse, ignoreDataResponse,dataResponse, valid, bytes, reg, gain;

	printf("Gain (0-23): ");
	scanf("%u",&gain);
	while (1){
		delay(delay_time);
		ignoreCommandResponse = bytez(88,1);
		dataResponse = bytez(0xffffff,3);
		moving_offset=moving_offset*0.99+dataResponse*0.01;
		linebytes(dataResponse,gain,moving_offset);
	}
	//Test_1M();
}

