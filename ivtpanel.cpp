/*
This is c++ simple application for Raspberry Pi running via ssh session (putty)
This application emulated control panel IVT Heat Pump controlled by REGO 637.
To set REGO in Service mode You have to push and keep for 10 sec button "Menu" on control panel 
or controll this button via transoptor and Raspberry GPIO pin 27.
You have to connect heat pump Interface to /tty/USB0 via USB/RS232 Interface detail http://rago600.sourceforge.net/
You can type a sequence command in one line (in Your Choose = option) than push Enter to execute all 
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <termios.h>
#include <time.h>
#include <iostream>

using namespace std;

typedef struct _tRegoScreen    //  IVT screen data structure
{
	char row[4] [21];   // 20 char screen data + 1 char in 21 column for LED status
} RegoScreen ;

typedef union _tRegoCommand   
{
    unsigned char raw[9];
    struct
    {
        unsigned char address;
        unsigned char command;
        unsigned char regNum[3]; // 3 bajt adress
        unsigned char value[3];  // 3 bajt command or parameter 
        unsigned char crc;
    } data;
} RegoCommand;

typedef union _tRegoReply  // REGO answer format
{
    char raw[5];
    struct
    {
        char address;
        char value[3];
        char crc;
    } data;
} RegoReply;


char OdczytanyEkran [4] [28]= {
{"Controler IVT   637   Heat "}, 
{"ver 1.2        2019   AdHea"}, 
{"Autor                 CKWU "},
{"Krzysztof Sala        Alarm"},
};
bool DEBUG_PrintScreen=false;
bool DEBUG_ReadPanel=false;
bool DEBUG_WriteCommand=false;
int krok = 0;
int fd ;
int PinServiceMode=27;  // BCM PIN number to call Service Mode
int delay_time=100; //  ms delay after write command def 300
unsigned int nextTime ;
RegoCommand cmd ;
RegoReply rreg;

 /* Manual                   REGO  600,    635,     637  versions command  source Domoticz hardware/Rego6XXSerial.cpp
 and    http://rago600.sourceforge.net/
RegoRegisters g_allRegisters[] = { 
	{ "GT1 Radiator",           0x0209,	0x020B,	0x020D,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT2 Out",		        0x020A,	0x020C,	0x020E,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT3 Hot water",	        0x020B,	0x020D,	0x020F,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT4 Forward",	        0x020C,	0x020E,	0x0210,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT5 Room",			    0x020D,	0x020F,	0x0211,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT6 Compressor",	        0x020E,	0x0210,	0x0212,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT8 Hot fluid out",      0x020F,	0x0211,	0x0213,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT9 Hot fluid in",		0x0210,	0x0212,	0x0214,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT10 Cold fluid inKS",	0x0211,	0x0213,	0x0215,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT11 Cold fluid outKS",	0x0212,	0x0214,	0x0216,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT3x Ext hot water",		0x0213, 0x0215, 0x0217,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "P3 Cold fluid",	    	0x01FD,	0x01FF,	0x0201,	REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Compressor",				0x01FE,	0x0200,	0x0202,	REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Add heat 1",     		0x01FF,	0x0201,	0x0203,	REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Add heat 2",		        0x0200,	0x0202,	0x0204,	REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "P1 Radiator",    		0x0203,	0x0205,	0x0207,	REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "P2 Heat fluid",  		0x0204, 0x0206, 0x0208, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Three-way valve",        0x0205, 0x0207, 0x0209, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Alarm",                  0x0206, 0x0208, 0x020A, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Operating hours",        0x0046, 0x0048, 0x004A, REGO_TYPE_COUNTER,      -50.0, -1, 0 ,0x02 , 5},
	{ "Radiator hours",         0x0048, 0x004A, 0x004C, REGO_TYPE_COUNTER,      -50.0, -1, 0 ,0x02 , 5},
	{ "Hot water hours",        0x004A, 0x004C, 0x004E, REGO_TYPE_COUNTER,      -50.0, -1, 0 ,0x02 , 5},
	{ "GT1 Target",             0x006E,	0x006E,	0x006E,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT3 Target",             0x002B,	0x002B,	0x002B,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
	{ "GT4 Target",             0x006D,	0x006D,	0x006D,	REGO_TYPE_TEMP,         -50.0, -1, 0 ,0x02 , 5},
    { "LED Power",              0x0012, 0x0012, 0x0012, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x00 , 5},
	{ "LED Heat",               0x0013, 0x0013, 0x0013, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x00 , 5},
	{ "LED Add Heat",           0x0014, 0x0014, 0x0014, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x00 , 5},
	{ "LED CKWU",               0x0015, 0x0015, 0x0015, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x00 , 5},
	{ "Button Info",            0x000A, 0x000A, 0x000A, REGO_TYPE_SET,          -50.0, -1, 0 ,0x00 , 1},
	{ "Menu",                  0x0206, 0x0208, 0x020A, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},
	{ "Temp          	       0x0206, 0x0208, 0x020A, REGO_TYPE_STATUS,       -50.0, -1, 0 ,0x02 , 5},	
*/
//******************************************

void z_czasu (char komunikat)  // function for debug
{
	time_t czas;
	printf ("%d %s %c\n" , time( & czas ), " loop -",  komunikat);  
	
}	

void PrintScren() // Print 4 rows of panel screen
{
int line=0;
int column=0;

printf("\n+----------------------+\n");
for (line=0; line<4; line++) 
{	
printf ("| ");
	for (column=0; column<20; column++)
	{
	 printf ("%c", char(OdczytanyEkran [line][column]));
	}
	
printf (" | ");
for (column=20; column<28; column++)
{
printf ("%c", char(OdczytanyEkran [line][column]));   // LED status
}
printf ("\n"); 
}
printf("+-^-------^-------^----+\n");
printf("  h       i       m       s-service\n");
printf(" r -right l -left f -fresh  e- exit\n");

}  // end PrintScreen()

//******************************************
/* void WriteReg( char z1,char z2,char z3, char z4, char z5,char z6,char z7, char z8,char z9)
{
	
cmd.data.address 	= z1;
cmd.data.command 	= z2;
cmd.data.regNum[0] 	= z3;
cmd.data.regNum[1] 	= z4;
cmd.data.regNum[2] 	= z5;
cmd.data.value[0] 	= z6;
cmd.data.value[1] 	= z7;
cmd.data.value[2] 	= z8;
cmd.data.crc		= z9;	
rreg.raw[0]=-1;

 cmd.data.regNum[0] = (g_allRegisters[m_pollcntr].regNum_type1 & 0xC000) >> 14 ; // odcina 2 lewe bity od 15 do 16
 cmd.data.regNum[1] = (g_allRegisters[m_pollcntr].regNum_type1 & 0x3F80) >> 7 ;  // odcina 8 bitów od 14 do 8
 cmd.data.regNum[2] = g_allRegisters[m_pollcntr].regNum_type1 & 0x007F;			// odcina 7 bitów od 7 do 0 

serialFlush ( fd) ;  
cmd.data.crc =0;
for ( int icrc = 2; icrc < 8; icrc++ ) cmd.data.crc ^= cmd.raw[ icrc ]; // set crc 
for (int i=0; i<9; i++) serialPutchar (fd, cmd.raw[i]) ; // printf(",%i", cmd.raw[i]);
	for (int ii=0; ii<5; ii++) 
	{
	rreg.raw=serialGetchar (fd); 
	printf("wart=%i_",rreg.raw[ii]);
	}
if (rreg.raw[0]==1)  
{
	printf("Odczyt rejestru  pomyslny \n");
		else;
	printf("Odczyt rejestru ni pomyslny \n");
}
	serialFlush ( fd) ;  // na wszelko wypadek
}  //end WriteReg()
*/

void ReadLed()
{
int odczytano;
cmd.data.address = 0x81;
cmd.data.command = 0x00;
cmd.data.regNum[0] = 0;
cmd.data.regNum[1] = 0;	
		
cmd.data.value[0] = 0;
cmd.data.value[1] = 0;
cmd.data.value[2] = 0;	

for (int i=0; i<4; i++ )
{
	serialFlush ( fd) ;  
	cmd.data.crc =0;
	cmd.data.regNum[2] = 0x0013+i;	
	for ( int icrc = 2; icrc < 8; icrc++ ) cmd.data.crc ^= cmd.raw[ icrc ]; // set crc 
	for (int ii=0; ii<9; ii++) serialPutchar (fd, cmd.raw[ii]) ; // printf(",%i", cmd.raw[ii]);
	for (int ii=0; ii<4; ii++) 
		{
		odczytano=serialGetchar (fd); 
		delay(6);
		if (odczytano==1)
		OdczytanyEkran[i] [20]= '*';
		else
		OdczytanyEkran[i] [20]= ' ';  // printf("wart=%i row =%i  kol=%i \n",odczytano, i,ii);
		}
	odczytano=serialGetchar (fd); // printf("Nowy LED \n");		
}
}  //end ReadLed()


void SetInstallMode (int Pin)
{
  wiringPiSetupGpio () ;
  pinMode (Pin, OUTPUT) ;
  digitalWrite (Pin, HIGH) ; 
  delay (10000) ;
  digitalWrite (Pin,  LOW) ; 
}

//******************************************	
void ReadPanel()
{
int odczytano=-1;
cmd.data.address = 0x81;
cmd.data.command = 0x20;
cmd.data.regNum[0] = 0;
cmd.data.regNum[1] = 0;				
cmd.data.value[0] = 0;
cmd.data.value[1] = 0;
cmd.data.value[2] = 0;	

 for (int line = 0 ; line < 4; line++) // change panel line
  {
	cmd.data.regNum[2] = line; // line number
	cmd.data.crc=0;
	for ( int i = 2; i < 8; i++ ) cmd.data.crc ^= cmd.raw[ i ]; // set crc 
do
{
	odczytano=-1;
	for (int ii=0; ii<9; ii++) serialPutchar (fd, cmd.raw[ii]) ;  // write order to IVT port  ii - counter order char
	while( !serialDataAvail (fd) )
	delay(5);
	odczytano=serialGetchar (fd);  //  Read first replay char = 01 
 }
while (odczytano!=1);
	// printf("rep c=%c    o=%o  row =%i  kol=%i\n\n",char(odczytano), odczytano,line,0);  // for debug
	for (int column=0; column<20; column++)  // decode panel 
	{

	while( serialDataAvail (fd)<2 )
	delay(5);		
		odczytano=serialGetchar (fd)*16 + serialGetchar (fd);
		if (odczytano<10) odczytano=42 ;
		OdczytanyEkran[line] [column]=char(odczytano);
		//printf("rep c=%c    o=%o  row =%i  kol=%i",char(odczytano), odczytano,line,column+1);  // for debug
	}
		serialFlush ( fd) ;  
		delay (delay_time);
  }
}  // end ReadPanel()


//******************************************
void WriteCommand( char z1,char z2,char z3, char z4, char z5,char z6,char z7, char z8,char z9)
{
cmd.data.address 	= z1;
cmd.data.command 	= z2;
cmd.data.regNum[0] 	= z3;
cmd.data.regNum[1] 	= z4;
cmd.data.regNum[2] 	= z5;
cmd.data.value[0] 	= z6;
cmd.data.value[1] 	= z7;
cmd.data.value[2] 	= z8;
cmd.data.crc		= z9;	
int odczytano=-1;

serialFlush (fd) ;
for ( int i = 2; i < 8; i++ )  cmd.data.crc ^= cmd.raw[ i ];   // setup crc
do 
{
	odczytano=-1;
	for (int ii=0; ii<9; ii++) serialPutchar (fd, cmd.raw[ii]) ;  // write order ii char to port	printf(" h=%i_%i",cmd.raw[ii],ii+1);
	while (!serialDataAvail (fd)) delay(5);
	if (serialDataAvail (fd)>0)
	{
	odczytano=serialGetchar (fd);  
	serialFlush (fd) ;	
	delay (delay_time);
	}
}
while (odczytano!=1);  //  if odczytano=1 maenas REGO confirm last sent command is OK
}  // end WriteCommand()


int main ()
{
  	  if ((fd = serialOpen ("/dev/ttyUSB0", 19200)) < 0)
  {
    fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
    return 1 ;
  }

  if (wiringPiSetup () == -1)
  {
    fprintf (stdout, "Unable to start wiringPi: %s\n", strerror (errno)) ;
    return 1 ;
  }
    
int choice;
bool gameOn = true;
while (gameOn != false)
{
if ( choice != char(10)) 
{	
ReadLed();
PrintScren();
printf("Your Choice = ");
}
choice=getc( stdin );  
switch (choice)
{
case 'h':
	WriteCommand(0x81,0x01,0,0,0x09,0,0,0x01,0);
	ReadPanel();
	ReadLed();
break;
case 'i':
	WriteCommand(0x81,0x01,0,0,0x0A,0,0,0x01,0);
	ReadPanel();
//	delay (2000);
//	ReadLed();
break;
case 'm':
	WriteCommand(0x81,0x01,0,0,0x0B,0,0,0x01,0);
	ReadPanel();
	ReadLed();
break;
case 'r':
	WriteCommand(0x81,0x01,0,0,0x44,0,0,0x01,0);
	ReadPanel();
	ReadLed();
break;
case 'f':
	ReadPanel();
	ReadLed();
break;
case 'l':
	WriteCommand(0x81,0x01,0,0,0x44,0x7F,0x7F,0x7F,0);
	ReadPanel();
	ReadLed();
break;
case 't':
WriteCommand(0x81,0x03,0,0,0,0,0,0x0A,0);  		// nachylenie krzywej 1.0
delay (delay_time);
WriteCommand(0x81,0x03,0,0,0x01,0,0x00,0x00,0);  // obni¿enie krzywej na 0
delay (delay_time);
WriteCommand(0x81,0x03,0,0,0x21,0,0x01,0x34,0);  // temp pokojowa na 18 oC
delay (delay_time);
break;
case 's':
	SetInstallMode (PinServiceMode);
	ReadPanel();
break;
case 'e':
	cout << "End of Program.\n";
	gameOn = false;
break;

}	//switch
} 	//while
serialClose ( fd) ;
return 0;
}

