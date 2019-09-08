#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "ff.h"	// Do³¹czenie biblioteki FatFS	
#include "lcd.h"// Do³¹czenie bilbioteki do obs³ugi wyœwietlacza 

#define UART_BAUD 9600 // Standardowa prêdkoœæ transmisji modu³u GPS
#define __UBRR (((F_CPU / (UART_BAUD * 16UL))) - 1)
#define GPS_BUFFOR_SIZE 80 // Rozmiar bufforu

char GPSBuffor[GPS_BUFFOR_SIZE] = {""}; // G³ówny bufor zdañ nmea
char lat[15] = {""}; // Bufor szerokoœci geograficznej
char lon[15] = {""}; // Bufor d³ugoœci geograficznej
char latDir[2] = {""}; // Bufor kierunku szerokoœci geograficznej
char lonDir[2] = {""}; // Bufor kierunku d³ugoœci geograficznej
char speed[15] = {""}; // Bufor prêdkoœci 
float latf = 0;// Zmienna do przechowywania wartoœci zmienno przecinkowej szerokoœci geograficznej
float lonf = 0;// Zmienna do przechowywania wartoœci zmienno przecinkowej szerokoœci geograficznej

//Zmienne do obs³ugi karty pamiêci
FATFS FatFs;		
FIL Fil;			

// Inicjalizacja Uarta
void USART_Init(void) 
{
	UBRR0H = (uint8_t) (__UBRR >> 8);
	UBRR0L = (uint8_t) (__UBRR);
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01); // Ustawienie ramki: 8bitów danych i 1 bit stopu
}

// Odbiór danych z Uarta
char USART_Receive(void) 
{	
	while (!(UCSR0A & (1<<RXC0)));
	return UDR0;
}

//Czyszczenie ca³ego buffora dla zdañ nmea odczytywanych z modu³u GPS
void clearBuffor()
{
	int i, j;
	
	for(i = 0; i < GPS_BUFFOR_SIZE-1; ++i)
	{
		GPSBuffor[i] = "";
	}
	
	latf = 0;
	lonf = 0;
}

// Pobieranie/budowanie zdañ nmea (Zape³nianie buffora danymi)
int getNmea()
{
	clearBuffor();		// Czyszczenie buffora z ewentualnych pozosta³oœci
	char buffor = "";	// Buffor znaku odbieranego z uarta
	int nmeaReady = 0;	// Flaga informuj¹ca czy ramka nmea jest poprawna i gotowa do dalszego przetwarzania
	int i = 0;
	
	// G³ówna pêtla budowania zdañ nmea
	while(1)
	{
		buffor = USART_Receive(); // Odbieranie znaku z modu³u GPSa 
			
		//0x0D  cr
		//0x0A nl
		if(buffor == (char)0x0D || buffor == (char)0x0A || i > GPS_BUFFOR_SIZE - 1 ) {	// Sprawdzenie czy odebrany znak jest jednym ze znaków koñcz¹cych 
			
			//0x24 = $
			//0x47 = G
			//0x50 = P
			if(GPSBuffor[0] == (char)0x24 && GPSBuffor[1] == (char)0x47 && GPSBuffor[2] == (char)0x50) // Sprawdzenie czy pierwsze 3 znaki znajduj¹ce siê w buforze s¹ znakami od których musi zaczynaæ siê ka¿de zdanie nmea ($GP)
			{
				//Jeœli tak ustawia flagê poprawnoœci na 1 i przerywa g³ówn¹ pêtle odbioru danych z GPSa 
				nmeaReady = 1;
				break;
			} else {
				//Jeœli zdanie jest nie poprawne czyœci buffor i równie¿ przerywa dzia³anie pêtli pobieraj¹cej dane
				clearBuffor();	
				break;
			}
		}
		
		//0x24 = $
		if(buffor == (char)0x24 && (GPSBuffor[0] == (char)0x24 || GPSBuffor[1] == (char)0x24)) { //Jeœli odebrany znak to $ (znak pocz¹tku dla zdania nmea)
			// Czyœci bufor i przerywa pêtle poniewa¿ oznacza to ¿e ci¹g znaków jest niepoprawny
			clearBuffor();	
			break;
		}
		
		//0x24 = $
		if(buffor == (char)0x24 || GPSBuffor[0] == (char)0x24) { //Jeœli pierwszy znak jest poprawny lub jeœli bufor jest ju¿ poprawnie zainicjalizowany
			//Dodaje znak do bufora oraz inkrementuje zmienn¹ i					
			GPSBuffor[i] = buffor;
			i = i+1;
		}
	}
	
	//Jeœli pêtla zosta³a przerwana zwracamy flagê informuj¹c¹ czy w bufforze znajduje siê poprawne zdanie nmea
	return nmeaReady;
}

//Zapis na kartê pamiêci
void saveBufforToMmc()
{
	UINT bw1, bw2;
	f_mount(&FatFs, "", 0);												
	if (f_open(&Fil, "GPS.txt", FA_WRITE | FA_OPEN_APPEND) == FR_OK) {	// Otwiera plik w trybie do zapisu i dopisywania
		f_write(&Fil, GPSBuffor, GPS_BUFFOR_SIZE, &bw1);				// Dopisuje zawartoœæ bufora			    
		f_write(&Fil, "\r\n", 2, &bw2);									// Dopisuje znaki nowej lini i powrotu karetki
		f_close(&Fil);													// Zamyka plik
	}
}

// Wyœwietlanie na ekranie
void showOnDisplay()
{
	//Buffory znakowe
	char latStr[30];
	char lonStr[30];
	char speedStr[30];
	
	// Konwersja odebranych ci¹gów znaków na floaty
	latf = atof(lat);
	lonf = atof(lon);
	float speedkmh = atof(speed);
	int speedkmhint = speedkmh*1.852; // Przeliczenie wêz³ów na km/h	
	speedkmhint = (speedkmhint < 10) ? 0 : speedkmhint; // Zaokr¹glenie prêgkoœci
	
	if(latf>0 && lonf>0) { // Jeœli wspó³rzêdne s¹ ró¿ne od 0 0 (Z za³o¿enia urz¹dzenie jest stosowane w samochodzie a punkt 0 0 wystêpuje na oceanie atlantyckim)

		//Przeliczanie wspó³rzêdnych otrzymanych z GPSa na format który przyjmuje Google Maps 
		int latdegrees = latf / 100;		// Wyci¹gamy DD
		latf = latf - (latdegrees*100);		// Wyci¹gamy 0.MMMMMM
		float latminutes = latf / 60;		// Zamieniamy 0.MMMMMM na 0.DDDDDD
		unsigned long latmin = (unsigned long)(latminutes * 100000); // I 0.DDDDDD na DDDDDD

		//Przeliczanie wspó³rzêdnych otrzymanych z GPSa na format który przyjmuje Google Maps 
		int londegrees = lonf / 100;
		lonf = lonf - (londegrees*100);
		float lonminutes = lonf / 60;
		unsigned long lonmin = (unsigned long)(lonminutes * 1000000);
		
		//Budowanie ci¹gów znaków do wyœwietlenia
		sprintf(latStr, "Lat: %d.%ld %s", latdegrees, latmin, latDir);
		sprintf(lonStr, "Lon: %d.%ld %s", londegrees, lonmin, lonDir);
		sprintf(speedStr, "Speed: %d", speedkmhint);
		
		// Wyœwietlanie przetworzonych danych na wyœwietlaczu
		lcd_clrscr();
		lcd_puts(latStr);
		lcd_gotoxy(0,2);
		lcd_puts(lonStr);
		lcd_gotoxy(0,4);
		lcd_puts(speedStr);

		_delay_ms(5000);
	}
}

// Parsowanie buffora
void parseBuffor()
{
	char buffor[GPS_BUFFOR_SIZE];	 // Lokalny bufor nmea
	strcpy(buffor, GPSBuffor);		 // Kopiowanie globalnego buffora nmea do lokalnej zmiennej 

	char* Message_ID = strtok(buffor,",");	// Pobieranie ID wiadomoœci (Klucza okreœlaj¹cego typ ramki np $GPRMC, $GPGLL)
	
	// Jeœli ramka jest typu "$GPRMC"
	if(strcmp(Message_ID, "$GPRMC") == 0) {
		// $GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62
		char* Time = strtok(NULL,",");			// Pobierz czas
		char* Data_Valid = strtok(NULL,",");	// Pobierz flagê poprawnoœci
		
		strcpy(lat, strtok(NULL,","));			// Pobierz szerokoœæ geograficzn¹
		strcpy(latDir, strtok(NULL,","));		// Pobierz kierunek szerokoœci 
		strcpy(lon, strtok(NULL,","));			// Pobierz d³ugoœæ geograficzn¹
		strcpy(lonDir, strtok(NULL,","));		// Pobierz kierunek d³ugoœci
		strcpy(speed, strtok(NULL,","));		// Pobierz prêdkoœæ
		char* COG = strtok(NULL,",");
		char* Date = strtok(NULL,",");
		char* Magnetic_Variation = strtok(NULL,",");
		char* M_E_W = strtok(NULL,",");
		char* Positioning_Mode = strtok(NULL,",");

		showOnDisplay(); // Wyœwietl dane na wyœwietlaczu
	
	} 
		
}

int main(void)
{	
	int flag = 0; // Flaga informuj¹ca o poprawnoœci ramki nmea

	USART_Init();			// Inicjalizacja Uarta
    lcd_init(LCD_DISP_ON);  // Inicjalizacja wyœwietlacza 
	
	lcd_clrscr();			// Czyszczenie wyœwietlacza
	lcd_gotoxy(5,2);		// Przesuniêcie kursora do pozycji 5, 2
	lcd_puts("Starting...");// Wypisanie komunikatu o starcie urz¹dzenia	
			
	//G³ówna pêtla programu		
	while(1) 
	{
		flag = getNmea(); // Pobieranie zdania nmea 	
		
		if(flag == 1) {   // Jeœli zdanie jest poprawne
			saveBufforToMmc();	// Zapisz buffor na kartê pamiêci			
			parseBuffor();		// Przeparsuj buffor + wyœwietlanie
		}
		clearBuffor();	// Czyszczenie buffora
	}
	
	return 0;
}

