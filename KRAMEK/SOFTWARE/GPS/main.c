#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "ff.h"	// Do��czenie biblioteki FatFS	
#include "lcd.h"// Do��czenie bilbioteki do obs�ugi wy�wietlacza 

#define UART_BAUD 9600 // Standardowa pr�dko�� transmisji modu�u GPS
#define __UBRR (((F_CPU / (UART_BAUD * 16UL))) - 1)
#define GPS_BUFFOR_SIZE 80 // Rozmiar bufforu

char GPSBuffor[GPS_BUFFOR_SIZE] = {""}; // G��wny bufor zda� nmea
char lat[15] = {""}; // Bufor szeroko�ci geograficznej
char lon[15] = {""}; // Bufor d�ugo�ci geograficznej
char latDir[2] = {""}; // Bufor kierunku szeroko�ci geograficznej
char lonDir[2] = {""}; // Bufor kierunku d�ugo�ci geograficznej
char speed[15] = {""}; // Bufor pr�dko�ci 
float latf = 0;// Zmienna do przechowywania warto�ci zmienno przecinkowej szeroko�ci geograficznej
float lonf = 0;// Zmienna do przechowywania warto�ci zmienno przecinkowej szeroko�ci geograficznej

//Zmienne do obs�ugi karty pami�ci
FATFS FatFs;		
FIL Fil;			

// Inicjalizacja Uarta
void USART_Init(void) 
{
	UBRR0H = (uint8_t) (__UBRR >> 8);
	UBRR0L = (uint8_t) (__UBRR);
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01); // Ustawienie ramki: 8bit�w danych i 1 bit stopu
}

// Odbi�r danych z Uarta
char USART_Receive(void) 
{	
	while (!(UCSR0A & (1<<RXC0)));
	return UDR0;
}

//Czyszczenie ca�ego buffora dla zda� nmea odczytywanych z modu�u GPS
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

// Pobieranie/budowanie zda� nmea (Zape�nianie buffora danymi)
int getNmea()
{
	clearBuffor();		// Czyszczenie buffora z ewentualnych pozosta�o�ci
	char buffor = "";	// Buffor znaku odbieranego z uarta
	int nmeaReady = 0;	// Flaga informuj�ca czy ramka nmea jest poprawna i gotowa do dalszego przetwarzania
	int i = 0;
	
	// G��wna p�tla budowania zda� nmea
	while(1)
	{
		buffor = USART_Receive(); // Odbieranie znaku z modu�u GPSa 
			
		//0x0D  cr
		//0x0A nl
		if(buffor == (char)0x0D || buffor == (char)0x0A || i > GPS_BUFFOR_SIZE - 1 ) {	// Sprawdzenie czy odebrany znak jest jednym ze znak�w ko�cz�cych 
			
			//0x24 = $
			//0x47 = G
			//0x50 = P
			if(GPSBuffor[0] == (char)0x24 && GPSBuffor[1] == (char)0x47 && GPSBuffor[2] == (char)0x50) // Sprawdzenie czy pierwsze 3 znaki znajduj�ce si� w buforze s� znakami od kt�rych musi zaczyna� si� ka�de zdanie nmea ($GP)
			{
				//Je�li tak ustawia flag� poprawno�ci na 1 i przerywa g��wn� p�tle odbioru danych z GPSa 
				nmeaReady = 1;
				break;
			} else {
				//Je�li zdanie jest nie poprawne czy�ci buffor i r�wnie� przerywa dzia�anie p�tli pobieraj�cej dane
				clearBuffor();	
				break;
			}
		}
		
		//0x24 = $
		if(buffor == (char)0x24 && (GPSBuffor[0] == (char)0x24 || GPSBuffor[1] == (char)0x24)) { //Je�li odebrany znak to $ (znak pocz�tku dla zdania nmea)
			// Czy�ci bufor i przerywa p�tle poniewa� oznacza to �e ci�g znak�w jest niepoprawny
			clearBuffor();	
			break;
		}
		
		//0x24 = $
		if(buffor == (char)0x24 || GPSBuffor[0] == (char)0x24) { //Je�li pierwszy znak jest poprawny lub je�li bufor jest ju� poprawnie zainicjalizowany
			//Dodaje znak do bufora oraz inkrementuje zmienn� i					
			GPSBuffor[i] = buffor;
			i = i+1;
		}
	}
	
	//Je�li p�tla zosta�a przerwana zwracamy flag� informuj�c� czy w bufforze znajduje si� poprawne zdanie nmea
	return nmeaReady;
}

//Zapis na kart� pami�ci
void saveBufforToMmc()
{
	UINT bw1, bw2;
	f_mount(&FatFs, "", 0);												
	if (f_open(&Fil, "GPS.txt", FA_WRITE | FA_OPEN_APPEND) == FR_OK) {	// Otwiera plik w trybie do zapisu i dopisywania
		f_write(&Fil, GPSBuffor, GPS_BUFFOR_SIZE, &bw1);				// Dopisuje zawarto�� bufora			    
		f_write(&Fil, "\r\n", 2, &bw2);									// Dopisuje znaki nowej lini i powrotu karetki
		f_close(&Fil);													// Zamyka plik
	}
}

// Wy�wietlanie na ekranie
void showOnDisplay()
{
	//Buffory znakowe
	char latStr[30];
	char lonStr[30];
	char speedStr[30];
	
	// Konwersja odebranych ci�g�w znak�w na floaty
	latf = atof(lat);
	lonf = atof(lon);
	float speedkmh = atof(speed);
	int speedkmhint = speedkmh*1.852; // Przeliczenie w�z��w na km/h	
	speedkmhint = (speedkmhint < 10) ? 0 : speedkmhint; // Zaokr�glenie pr�gko�ci
	
	if(latf>0 && lonf>0) { // Je�li wsp�rz�dne s� r�ne od 0 0 (Z za�o�enia urz�dzenie jest stosowane w samochodzie a punkt 0 0 wyst�puje na oceanie atlantyckim)

		//Przeliczanie wsp�rz�dnych otrzymanych z GPSa na format kt�ry przyjmuje Google Maps 
		int latdegrees = latf / 100;		// Wyci�gamy DD
		latf = latf - (latdegrees*100);		// Wyci�gamy 0.MMMMMM
		float latminutes = latf / 60;		// Zamieniamy 0.MMMMMM na 0.DDDDDD
		unsigned long latmin = (unsigned long)(latminutes * 100000); // I 0.DDDDDD na DDDDDD

		//Przeliczanie wsp�rz�dnych otrzymanych z GPSa na format kt�ry przyjmuje Google Maps 
		int londegrees = lonf / 100;
		lonf = lonf - (londegrees*100);
		float lonminutes = lonf / 60;
		unsigned long lonmin = (unsigned long)(lonminutes * 1000000);
		
		//Budowanie ci�g�w znak�w do wy�wietlenia
		sprintf(latStr, "Lat: %d.%ld %s", latdegrees, latmin, latDir);
		sprintf(lonStr, "Lon: %d.%ld %s", londegrees, lonmin, lonDir);
		sprintf(speedStr, "Speed: %d", speedkmhint);
		
		// Wy�wietlanie przetworzonych danych na wy�wietlaczu
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

	char* Message_ID = strtok(buffor,",");	// Pobieranie ID wiadomo�ci (Klucza okre�laj�cego typ ramki np $GPRMC, $GPGLL)
	
	// Je�li ramka jest typu "$GPRMC"
	if(strcmp(Message_ID, "$GPRMC") == 0) {
		// $GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62
		char* Time = strtok(NULL,",");			// Pobierz czas
		char* Data_Valid = strtok(NULL,",");	// Pobierz flag� poprawno�ci
		
		strcpy(lat, strtok(NULL,","));			// Pobierz szeroko�� geograficzn�
		strcpy(latDir, strtok(NULL,","));		// Pobierz kierunek szeroko�ci 
		strcpy(lon, strtok(NULL,","));			// Pobierz d�ugo�� geograficzn�
		strcpy(lonDir, strtok(NULL,","));		// Pobierz kierunek d�ugo�ci
		strcpy(speed, strtok(NULL,","));		// Pobierz pr�dko��
		char* COG = strtok(NULL,",");
		char* Date = strtok(NULL,",");
		char* Magnetic_Variation = strtok(NULL,",");
		char* M_E_W = strtok(NULL,",");
		char* Positioning_Mode = strtok(NULL,",");

		showOnDisplay(); // Wy�wietl dane na wy�wietlaczu
	
	} 
		
}

int main(void)
{	
	int flag = 0; // Flaga informuj�ca o poprawno�ci ramki nmea

	USART_Init();			// Inicjalizacja Uarta
    lcd_init(LCD_DISP_ON);  // Inicjalizacja wy�wietlacza 
	
	lcd_clrscr();			// Czyszczenie wy�wietlacza
	lcd_gotoxy(5,2);		// Przesuni�cie kursora do pozycji 5, 2
	lcd_puts("Starting...");// Wypisanie komunikatu o starcie urz�dzenia	
			
	//G��wna p�tla programu		
	while(1) 
	{
		flag = getNmea(); // Pobieranie zdania nmea 	
		
		if(flag == 1) {   // Je�li zdanie jest poprawne
			saveBufforToMmc();	// Zapisz buffor na kart� pami�ci			
			parseBuffor();		// Przeparsuj buffor + wy�wietlanie
		}
		clearBuffor();	// Czyszczenie buffora
	}
	
	return 0;
}

