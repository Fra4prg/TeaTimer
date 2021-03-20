/****************************************************************************
 Title	:   TeaTimer
 Author:    (c) 2008 Frank Scholl <Frank_Scholl@t-online.de>
 File:	    $Id: TeaTimer.c, V091110$
 Software:  AVR-Studio V4.15 Build 623, WinAVR-20080610
 Target:    ATmega168/ATmega8 mit Servo und 7-Segmentanzeige

 DESCRIPTION
       Programm zur Steuerung des Teebeutel-Automaten

 USAGE
       See the C include lcd.h file for a description of each function


Verwendung der Timer:
############# importiert von Helitimer -->>>> anpassen!!!!!!!!!!!!!!!!!!!!!!
**	16-Bit-Timer1 Overflow Interrupt ISR, 
**	wird alle 20ms bei Erreichen des BOTTOM-Wertes ausgefuehrt
**	Zeitmessung, Usercounter, Berechnung Servoposition,
**	Tastenabfrage, Messung Batteriespannung

Timer0 (8bit, aufwärts, 10bit prescaler)
	LED-PWM, nur aktiv, wenn LED-PWM ein
	|<---------- var.µs ------->|<------ (10ms-var.µs) ------>|
    |                           +-> Timer0 OVR : alle Ziffern aus
    +-> Timer0 OVR : alle Ziffern an



Timer1 (16bit, auf-/abwärts, 10bit prescaler, 2x output compare (output PB1, PB2), 1x input capture (PB0), CTC)
  |<----------------------- 20ms ------------------------>|
  |             +-> 5ms OC1B: 2.Ziffer aktivieren
  +->  Zähler-Start bei 0

Timer2 (8bit, aufwärts, 10bit prescaler, 1x output compare (output PB3), unabh. externer Quarz 32kHz möglich (PB6,PB7), CTC)
	Sound-Ausgabe über output compare PB3 (CTC mode)


 ToDo:
- Servo invers: Konstanten gSERVOPOS_Up, ... in Variablen ändern, die über ADC6 oder ADC7 initialisiert werden
- Servo macht ruckelige Bewegungen, besonders bei langsamer Geschwindigkeit (HW-Problem???)
    --> Portbeschreibung 7-Segment? evtl. in 20ms-Int integrieren / OC1B?
- LED-PWM nicht ausgeprägt sit Umstellung auf Timer0
- Feld für Sound-Sequenzen als struct statt BYTE
- Parameter in EEPROM: Uint8_t-Vergleich mit Komplement und Bereich 1-9 funktioniert nicht
- Unterspannungserkennung

****************************************************************************/


#include <stdlib.h>
#include <stdint.h>  // z.B. für uint16_t
#include <stdio.h>
//#include <string.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h> // fuer sei(), cli() und ISR():
#include "bool.h"
#include "TeaTimer.h"
#include <util/delay.h>     /* definiert _delay_ms() ab avr-libc Version 1.2.0 */


/****************************************************************************
*****************************************************************************
*****   allgemeine Routinen                                             *****
*****************************************************************************
****************************************************************************/


/*************************************************
**	 längere Wartezeiten
**	   Die maximale Zeit pro Funktionsaufruf _delay_ms() ist begrenzt auf 
**	   262.14 ms / F_CPU in MHz (im Beispiel: 262.1 / 3.6864 = max. 71 ms) 
**	   Daher wird die kleine Warteschleife mehrfach aufgerufen,
**	   um auf eine längere Wartezeit zu kommen. Die zusätzliche 
**	   Prüfung der Schleifenbedingung lässt die Wartezeit geringfügig
**	   ungenau werden (macht hier vielleicht 2-3ms aus).
**************************************************/
void long_delay_ms(uint16_t ms)
{
	for(;ms>0;ms--) _delay_ms(1);
}


/*************************************************
**	Soundausgabe
**  Parameter: Ton-Nr., Dauer
**************************************************/
void Sound(uint8_t NoteNo, uint16_t Length)
{

	/*
	Dreiklang:
	(http://de.wikipedia.org/wiki/Dreiklang)
	unteres Intervall	oberes Intervall 	Rahmenintervall 	Dreiklangsbezeichnung 			Beispiel (Grundton-Terzton-Quintton)
	große Terz 			kleine Terz 		reine Quinte 		Dur-Dreiklang 					c-e-g
	kleine Terz 		große Terz 			reine Quinte 		Moll-Dreiklang 					c-es-g
	große Terz 			große Terz 			übermäßige Quinte 	übermäßiger Dreiklang 			c-e-gis
	kleine Terz 		kleine Terz 		Tritonus 			verminderter Dreiklang 			c-es-ges
	große Terz 			verminderte Terz	verminderte Quinte 	hartverminderter Dreiklang 		c-e-ges
	verminderte Terz 	kleine Terz 		reine Quarte 		doppeltverminderter Dreiklang 	cis-es-ges

	Frequenzen der gleichstufigen Stimmung
	(http://de.wikipedia.org/wiki/Frequenzen_der_gleichstufigen_Stimmung)
	Noten-Nr. Note  Note            Frequenz
	88 	C8 			c5 				4186,01
	87 	B7 			h4 				3951,07
	86 	A#7/Bb7 	ais4/b4 		3729,31
	85 	A7 			a4 				3520,00
	84 	G#7/Ab7 	gis4/ges4 		3322,44
	83 	G7 			g4 				3135,96
	82 	F#7/Gb7 	fis4/ges4 		2959,96
	81 	F7 			f4 				2793,83
	80 	E7 			e4 				2637,02
	79 	D#7/Eb7 	dis4/es4 		2489,02
	78 	D7 			d4 				2349,32
	77 	C#7/Db7 	cis4/des4 		2217,46
	76 	C7 			c4 				2093,00
	75 	B6 			h3 				1975,53
	74 	A#6/Bb6 	ais3/b3 		1864,66
	73 	A6 			a3 				1760,00
	72 	G#6/Ab6 	gis3/as3 		1661,22
	71 	G6 			g3 				1567,98
	70 	F#6/Gb6 	fis3/ges3 		1479,98
	69 	F6 			f3 				1396,91
	68 	E6 			e3 				1318,51
	67 	D#6/Eb6 	dis3/es3 		1244,51
	66 	D6 			d3 				1174,66
	65 	C#6/Db6 	cis3/des3 		1108,73
	64 	C6 			c3 				1046,50
	63 	B5 			h2 				987,767
	62 	A#5/Bb5 	ais2/b2 		932,328
	61 	A5 			a2 				880,000
	60 	G#5/Ab5 	gis2/as2 		830,609
	59 	G5 			g2 				783,991
	58 	F#5/Gb5 	fis2/ges2 		739,989
	57 	F5 			f 2 			698,456
	56 	E5 			e2 				659,255
	55 	D#5/Eb5 	dis2/es2 		622,254
	54 	D5 			d2 				587,330
	53 	C#5/Db5 	cis2/des2 		554,365
	52 	C5 			c2 				523,251
	51 	B4 			h1 				493,883
	50 	A#4/Bb4 	ais1/b1 		466,164
	49 	A4 			a1 Kammerton 	440,000
	48 	G#4/Ab4 	gis1/as1 		415,305
	47 	G4 			g1 				391,995
	46 	F#4/Gb4 	fis1/ges1 		369,994
	45 	F4 			f 1 			349,228
	44 	E4 			e1 				329,628
	43 	D#4/Eb4 	dis1/es1 		311,127
	42 	D4 			d1 				293,665
	41 	C#4/Db4 	cis1/des1 		277,183
	40 	C4 (Mid C) 	c1 				261,626
	39 	B3 			h 				246,942
	38 	A#3/Bb3 	ais/b 			233,082
	37 	A3 			a 				220,000

	*/

	/* Timer2 initialisieren für Sound-Frequenz-Ausgabe */
	// CS22:0=111 --> 8MHz/1024 =   7812,5Hz --> 1 tic=128µs, 256tics=32768µs --> fmin=1/(256*128µs*2)=  15Hz, fmax=   3906Hz
	// CS22:0=110 --> 8MHz/256  =  31250,0Hz --> 1 tic= 32µs, 256tics= 8192µs --> fmin=1/(256* 32µs*2)=  61Hz, fmax=  15625Hz
	// CS22:0=101 --> 8MHz/128  =  62500,0Hz --> 1 tic= 16µs, 256tics= 4096µs --> fmin=1/(256* 16µs*2)= 122Hz, fmax=  31250Hz
	// CS22:0=100 --> 8MHz/64   = 125000,0Hz --> 1 tic=  8µs, 256tics= 2048µs --> fmin=1/(256*  8µs*2)= 244Hz, fmax=  62500Hz
	// CS22:0=011 --> 8MHz/32   = 250000,0Hz --> 1 tic=  4µs, 256tics= 1024µs --> fmin=1/(256*  4µs*2)= 488Hz, fmax= 125000Hz
	// CS22:0=010 --> 8MHz/8    =1000000,0Hz --> 1 tic=  1µs, 256tics=  256µs --> fmin=1/(256*  1µs*2)=1952Hz, fmax= 500000Hz
	// COM21:0=01 --> Toggle OC2 on compare match --> OC-Pin einschalten
	// WGM21:0=10 --> CTC mode

	/* Frequenz bestimmen
	Für den output compare-Wert wird die halbe Periodendauer einer Schwingung benötigt (OC-Pin toggle!)
	T/2=1/2*f, oc=(T/2)/tic)  -> oc=1/(2*f*tic)
	*/
	switch(NoteNo)
		{
		case 37:
			TCCR2 = (1<<WGM21) | (1<<CS22) | (1<<CS20);
			OCR2=142;
			break;
		case 38:
			TCCR2 = (1<<WGM21) | (1<<CS22) | (1<<CS20);
			OCR2=134;
			break;
		case 39:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=253;
			break;
		case 40:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=239;
			break;
		case 41:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=225;
			break;
		case 42:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=213;
			break;
		case 43:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=201;
			break;
		case 44:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=190;
			break;
		case 45:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=179;
			break;
		case 46:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=169;
			break;
		case 47:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=159;
			break;
		case 48:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=150;
			break;
		case 49:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=142;
			break;
		case 50:
			TCCR2 = (1<<WGM21) | (1<<CS22);
			OCR2=134;
			break;
		case 51:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=253;
			break;
		case 52:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=239;
			break;
		case 53:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=225;
			break;
		case 54:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=213;
			break;
		case 55:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=201;
			break;
		case 56:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=190;
			break;
		case 57:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=179;
			break;
		case 58:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=169;
			break;
		case 59:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=159;
			break;
		case 60:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=150;
			break;
		case 61:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=142;
			break;
		case 62:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=134;
			break;
		case 63:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=127;
			break;
		case 64:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=119;
			break;
		case 65:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=113;
			break;
		case 66:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=106;
			break;
		case 67:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=100;
			break;
		case 68:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=95;
			break;
		case 69:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=89;
			break;
		case 70:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=84;
			break;
		case 71:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=80;
			break;
		case 72:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=75;
			break;
		case 73:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=71;
			break;
		case 74:
			TCCR2 = (1<<WGM21) | (1<<CS21) | (1<<CS20);
			OCR2=67;
			break;
		case 75:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=253;
			break;
		case 76:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=239;
			break;
		case 77:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=225;
			break;
		case 78:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=213;
			break;
		case 79:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=201;
			break;
		case 80:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=190;
			break;
		case 81:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=179;
			break;
		case 82:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=169;
			break;
		case 83:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=159;
			break;
		case 84:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=150;
			break;
		case 85:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=142;
			break;
		case 86:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=134;
			break;
		case 87:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=127;
			break;
		case 88:
			TCCR2 = (1<<WGM21) | (1<<CS21);
			OCR2=119;
			break;
		default:					// fsm Status: Fehler
			OCR2     = 10;
			break;
		}

	// Ton einschalten
	TCCR2 |= (1<<COM20);

	// Ton-Dauer warten
	long_delay_ms(Length);
		
	// Ton ausschalten
	TCCR2 &= ~(1<<COM20);
}


/*************************************************
**	eine Melodie spielen
**************************************************/
void PlaySound(int Index)
{
	uint8_t TonNr;
	uint16_t TonDauer;
	uint8_t Pause;

	do 
	{
		TonNr    = pgm_read_byte_near(&SoundCodes[Index][0]);
		TonDauer = pgm_read_byte_near(&SoundCodes[Index][1]);
		Pause    = pgm_read_byte_near(&SoundCodes[Index][2]);
		if (TonNr!=0)
		{
			Sound(TonNr,TonDauer);
		}
		else
		{
			long_delay_ms(TonDauer);
		}
		long_delay_ms(Pause);
		Index++;
	}
	// solange Ende der Sequenz nicht erreicht und keine Taste gedrückt
	while ((TonDauer!=0)&&(gKeyPressed_Time==0)&&(gKeyPressed_Start==0)&&(gKeyPressed_Option==0));
}


/*************************************************
**	eine Melodie spielen
**************************************************/
void PlaySoundSequence(uint8_t Sequence)
{
	int Index, i;
	uint8_t TonDauer;

	Index=0;  // zeigt auf Sequenz-Array
	for (i=1;i<Sequence;i++)  // die gewünschte Sequenz suchen
	{
		do
		{
			TonDauer = pgm_read_byte_near(&SoundCodes[Index][1]);
			Index++;
		}
		while (TonDauer!=0); // Ende der Sequenz erreicht
	}
	PlaySound(Index);
}


/*************************************************
**	eine Batteriespannung gefiltert auslesen
**************************************************/
uint16_t ReadUbatt(void)
{
	uint8_t i;
	uint8_t nFilter;
	uint16_t result;
	unsigned long int result_temp;
 
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);    // Frequenzvorteiler setzen (1) und ADC aktivieren (1)
 	// set ADC prescaler to 62,5kHz to get maximum resolution (ADCSRA.ADPS = 7 --> 8MHz/128=62,5kHz)

	ADMUX = UBATT_CH;         // Kanal für Ubatt waehlen
	ADMUX |= (1<<REFS0) | (1<<REFS1); // Referenzspannung = Internal 2.56V Voltage Reference with external capacitor at AREF pin
 
	/* nach Aktivieren des ADC wird ein "Dummy-Readout" empfohlen, man liest
     also einen Wert und verwirft diesen, um den ADC "warmlaufen zu lassen" */
	ADCSRA |= (1<<ADSC);              // eine ADC-Wandlung 
	while ( ADCSRA & (1<<ADSC) ) 
	{
		;     // auf Abschluss der Konvertierung warten 
	}
	result_temp = ADCW;  // ADCW muss einmal gelesen werden, sonst wird Ergebnis der nächsten Wandlung nicht übernommen.
 
	nFilter=1;
	/* Eigentliche Messung - Mittelwert aus nFilter aufeinanderfolgenden Wandlungen */
	result_temp = 0; 
	for( i=0; i<nFilter; i++ )
	{
		ADCSRA |= (1<<ADSC);           // eine Wandlung "single conversion"
		while ( ADCSRA & (1<<ADSC) ) 
		{
			;   // auf Abschluss der Konvertierung warten
		}
		result_temp += ADCW;           // Wandlungsergebnisse aufaddieren
	}
	ADCSRA &= ~(1<<ADEN);              // ADC deaktivieren (2)
 
	result = (uint16_t) (result_temp / nFilter);       // Summe durch nFilter teilen = arithm. Mittelwert
 
	return result;
}


/*************************************************
**	Status Option umschalten bei Tastendruck
**************************************************/
void CheckOptionButton(void)
{
	if (gKeyPressed_Option!=0)
	{
		gKeyPressed_Option--;
		if (gOptionSound==true)	 // Option Schwenken umschalten
		{
			gOptionSound = false;
			gLED_Option = false;
			gLED_OptionPWM = false;
		}
		else
		{
			gOptionSound = true; 
			gLED_Option = true;
			gLED_OptionPWM = false;
		}
	}
}

/*************************************************
**	Zeichen auf 7-Segmentanzeige ausgeben
**************************************************/
void Display7(int Index)
{
	if (gDisplayTypeCommonCathode)
	{
		// gemeinsame Kathode
		PORTC = 0b00111110 & pgm_read_byte_near(&AnzeigeCode[Index][0]);
		PORTB = (PORTB & 0b00001010) | (0b00000101 & pgm_read_byte_near(&AnzeigeCode[Index][1]));
	}
	else
	{
		// gemeinsame Anode
		PORTC = 0b00111110 & ~pgm_read_byte_near(&AnzeigeCode[Index][0]);
		PORTB = (PORTB & 0b00001010) | (0b00000101 & ~pgm_read_byte_near(&AnzeigeCode[Index][1]));
	}
//	PORTC = 0b00111110 & pgm_read_byte_near(&AnzeigeCode[Index][0]);
//	PORTB = 0b00000101 & pgm_read_byte_near(&AnzeigeCode[Index][1]);
}


/****************************************************************************
*****************************************************************************
*****   Interrupt Routinen                                              *****
*****************************************************************************
****************************************************************************/

/*************************************************
**	16-Bit-Timer1 Overflow Interrupt ISR, 
**	wird alle 20ms bei Erreichen des BOTTOM-Wertes ausgefuehrt
**	Zeitmessung, Usercounter, Berechnung Servoposition,
**	Tastenabfrage, Messung Batteriespannung
**************************************************/
ISR(TIMER1_OVF_vect)
{
	/* Begin des Interrupts: Debug-Pin1=1 (zur Laufzeitmessung) */
	PORTD |= (1 << DEBUGPIN1);

	/* Berechnung Servoposition */
	// OCR1A = 500...1000; // 1ms...2ms Puls

		if (OCR1A!=gServoStop)	// 	wenn Zielpos noch nicht erreicht (OCR1A<>Endposition)
		{

			if (OCR1A<gServoStop)	//		wenn Zielpos>aktuelle Pos
			{

				OCR1A+=gServoSpeed;	//	neue Position = vorige Position + Winkelgeschwindigkeit (=Steps/20ms)
				if (OCR1A>gServoStop)	//		wenn über Ziel hinaus
				{
					OCR1A=gServoStop;
					gServoStart=gServoStop;
				}
			}
			else
			{

				OCR1A-=gServoSpeed;	//	   		neue Position = vorige Position - Winkelgeschwindigkeit (=Steps/20ms)
				if (OCR1A<gServoStop)	//		wenn über Ziel hinaus
				{
					OCR1A=gServoStop;
					gServoStart=gServoStop;
				}
			}
		}
		else
		{
			gServoStart=gServoStop;
		}


	/* Zeitmessung */
	gCounter20ms++; // 20ms zählen
	if (gCounter20ms >= 50)
	{
		//PORTD ^= (1 <<	SEG7_dp); // Test: Sekundentakt LED togglen
		if (gLED_dp)
		{
			gLED_dp=false;
		}
		else
		{
			gLED_dp=true;
		}
		gCounter20ms=0;
		gSeconds++; // Sekunden hochzählen
		if (gSeconds >= 60)
		{
			gSeconds=0;
			gMinutes++; // Minuten hochzählen
			if (gMinutes > 9)
			{
				gMinutes=0; // wir zählen nur bis 9 Minuten
			}
		}
	}

	/* Usercounter */
	if (gCounterUser>0)
	{
		gCounterUserStepCNT++; // hochzählen bis Schrittweite
		if (gCounterUserStepCNT>=gCounterUserStep)
		{
			gCounterUserStepCNT=0;
			gCounterUser--; // Userzähler um Schrittweite herunterzählen
		}
	}

	/* Tastenabfrage */
	// hier wird gKeyCounter veraendert. Die übrigen Programmteile müssen diese Aenderung "sehen":
	// volatile -> aktuellen Wert immer in den Speicher schreiben
	if ( !(TASTENPORT & (1<<TASTE_Zeit)) ) 
	{
		if (gKeyCounter_Time < CNTREPEAT) 
		{
			gKeyCounter_Time++;
			if (gKeyCounter_Time == CNTDEBOUNCE) 
			{
				gKeyPressed_Time++;  // Taste wurde (ggf. erneut) kurz gedrückt --> zählen, main() muss abarbeiten = runterzählen
			}
		}
	}
	else 
	{
		gKeyCounter_Time = 0;
	}

	if ( !(TASTENPORT & (1<<TASTE_Start)) ) 
	{
		if (gKeyCounter_Start < CNTREPEAT) 
		{
			gKeyCounter_Start++;
			if (gKeyCounter_Start == CNTDEBOUNCE) 
			{
				gKeyPressed_Start++;  // Taste wurde (ggf. erneut) kurz gedrückt --> zählen, main() muss abarbeiten = runterzählen
			}
		}
	}
	else 
	{
		gKeyCounter_Start = 0;
	}

	if ( !(TASTENPORT & (1<<TASTE_Option)) ) 
	{
		if (gKeyCounter_Option < CNTREPEAT) 
		{
			gKeyCounter_Option++;
			if (gKeyCounter_Option == CNTDEBOUNCE) 
			{
				gKeyPressed_Option++;  // Taste wurde (ggf. erneut) kurz gedrückt --> zählen, main() muss abarbeiten = runterzählen
			}
		}
	}
	else 
	{
		gKeyCounter_Option = 0;
	}


	/* Messung Batteriespannung */
	//gUbatt=ReadUbatt();

	/* Ende des Interrupts: Debug-Pin1=0 (zur Laufzeitmessung) */
	PORTD &= ~(1 << DEBUGPIN1);
}


/*************************************************
**	Interrupt für LED-PWM
**	8-Bit-Timer0 Overflow Interrupt ISR
**	LEDs und Anzeige nach Vorgabevariablen setzen
**  Interrupt wird aktiviert, wenn LED-PWM aktiv sein soll
**************************************************/
ISR(TIMER0_OVF_vect)
{
	/* Begin des Interrupts: Debug-Pin2=1 (zur Laufzeitmessung) */
	//PORTD |= (1 << DEBUGPIN2);

	// wenn PWM-Phase=einschalten
	if (gLEDPWM_NEXT_ON)
	{
		// Alle LEDs und Anzeige nach Vorgabe setzen
		if (gLED_Signal)
		{
			PORTD |= (1 << LED_Signal);
		}
		else
		{
			PORTD &= ~(1 << LED_Signal);
		}

		if (gLED_Option)
		{
			PORTD |= (1 << LED_Option);
		}
		else
		{
			PORTD &= ~(1 << LED_Option);
		}

		if (gLED_dp)
		{
			PORTD |= (1 << SEG7_dp);
		}
		else
		{
			PORTD &= ~(1 << SEG7_dp);
		}

		Display7(gDisplayChr);

/*
Berechnung des nächsten Timer0-Zählerwerts für Overflow-Interrupt

    OVR                                  OVR                            OVR
     |<---------LEDPWM_TCNT0_ON---------->|<-----LEDPWM_TCNT0_OFF------->|...
	 |                                    |                              +-> LEDs aus
	 |                                    +-> LEDs aus
	 +-> LEDs ein

	gLEDPWM_TCNT0_ON:  TCNT0-Startwert (Zählerstand) für LED-on-Zeit
	gLEDPWM_TCNT0_OFF: TCNT0-Startwert (Zählerstand) für LED-off-Zeit
	LEDPWM_Lo: min. Wert für LEDPWM_TCNT0_ON (dunkel)
	LEDPWM_Hi: max. Wert für LEDPWM_TCNT0_ON (hell)
	gLEDPWM_SPEED: so oft muss der PWM-Einschalt-Interrupt ausgeführt werden, bis die Helligkeit (PWM) geändert wird (Geschwindigkeit der Änderung)
	gLEDPWM_CNT: Zähler zur PWM-Änderung (gLEDPWM_SPEED)
	gLEDPWM_DIR: gibt die Richtung der PWM-Änderung an (1=heller, 0=dunkler)

*/

		// Timer-Zählerwerte für Helligkeits-Änderung berechnen
		if (gLEDPWM_CNT>=gLEDPWM_SPEED)  // nächter Zeitpunkt der PWM-Änderung erreicht?
		{
			gLEDPWM_CNT = 0;
			// Richtung PWM-Änderung: heller oder dunkler dimmen?
			if (gLEDPWM_DIR==1)
			{
				if (gLEDPWM_TCNT0_ON<LEDPWM_Hi) // max.?
				{
					gLEDPWM_TCNT0_ON++;
					gLEDPWM_TCNT0_OFF = 256 - gLEDPWM_TCNT0_ON;
				}
				else
				{
					gLEDPWM_DIR = 0; // nächstes Mal PWM-Wert runterzählen
				}
			}
			else
			{
				if (gLEDPWM_TCNT0_ON>LEDPWM_Lo) // min.?
				{
					gLEDPWM_TCNT0_ON--;
					gLEDPWM_TCNT0_OFF = 256 - gLEDPWM_TCNT0_ON;
				}
				else
				{
					gLEDPWM_DIR = 1; // nächstes Mal PWM-Wert hochzählen
				}
			}
		}
		else
		{
			TCNT0 = gLEDPWM_TCNT0_OFF; // Timerzähler neu setzen
			gLEDPWM_CNT++;
		}
		gLEDPWM_NEXT_ON = false;  // nächste PWM-Phase=aus
	}
	else
	{
		// falls PWM-Phase aus, dann hier ausschalten (wenn PWM für das entsprechende Element aktiv)
		if (gLED_SignalPWM) PORTD &= ~(1 << LED_Signal);
		if (gLED_OptionPWM) PORTD &= ~(1 << LED_Option);
		if (gLED_dpPWM)     PORTD &= ~(1 << SEG7_dp);
		if (gDisplayPWM)    Display7(22); // Leerzeichen

		TCNT0 = gLEDPWM_TCNT0_ON; // Timerzähler neu setzen

		gLEDPWM_NEXT_ON = true;  // nächste PWM-Phase=ein
	}

	// Ende des Interrupts: Debug-Pin2=0 (zur Laufzeitmessung)
	//PORTD &= ~(1 << DEBUGPIN2);
}


/****************************************************************************
*****************************************************************************
*****   FSM Routinen                                                    *****
*****************************************************************************
****************************************************************************/


/*************************************************
**	fsm Status: Initialisierung
**************************************************/
uint8_t Init(void)
{
	uint8_t fsmExitState;
	uint8_t var;
//	uint8_t varc;

	/**** Eingangsaktionen ****/
	/* IO-Ports initialisieren */

	/* PB0	= (O) 7-Segment-Anzeige e 
	   PB1 	= (O) Servo-PWM
	   PB2	= (O) 7-Segment-Anzeige d
	   PB3 	= (S/O) ISP / Spoundpin
	   PB4 	= (S) ISP
	   PB5 	= (S) ISP
	   PB6	= (I) n.c.
	   PB7	= (I) n.c. */
	DDRB  |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3);  /* auf Ausgang schalten */

	/* PC0	= Messung Batteriespannung
	   PC1 	= (O) 7-Segment-Anzeige c
	   PC2 	= (O) 7-Segment-Anzeige a
	   PC3 	= (O) 7-Segment-Anzeige f
	   PC4 	= (O) 7-Segment-Anzeige g
	   PC5 	= (O) 7-Segment-Anzeige b
	   PC6 	= (I) n.c.
	  (PC7)				= (I) n.v. */
	DDRC  |= (1 << PC1) | (1 << PC2) | (1 << PC3) | (1 << PC4) | (1 << PC5);  /* auf Ausgang schalten */

	/* PD0 	= (O) Debug-Signal
	   PD1	= (O) LED Signal
	   PD2	= (I) Taste Zeit
	   PD3	= (I) Taste Start
	   PD4	= (I) Taste Option
	   PD5	= (O) LED Option
	   PD6	= (O) 7-Segment-Anzeige dp
	   PD7	= (O) Debug-Signal */
	DDRD  |= (1 << PD0) | (1 << PD1) | (1 << PD5) | (1 << PD6);// | (1 << PD7);  /* auf Ausgang schalten */
	PORTD |= (1 << PD2) | (1 << PD3) | (1 << PD4);  /* Pullup-Widerstände der Tasten aktivieren */

	// Anzeige-Defaults
	gLED_Signal  	= true;
	gLED_SignalPWM  = true;

	// Auswahl des Displaytyps
	if ( !(PIND & (1<<DISPLAY_TYPE_SELECT)) )
	{
		gDisplayTypeCommonCathode = true;
	}
	else
	{
		gDisplayTypeCommonCathode = false;
	}
	DDRD  |= (1 << PD7);  /* nach Einlesen des Displaytyps Debugpin auch auf Ausgang schalten */


	// Auswahl des Servotyps (Richtung)
//	if ( !(PINADC & (1<<SERVO_INVERSE_SELECT)) )
	if ( true )
	{
		// invers
		gSERVOPOS_Up = SERVOPOS_Up_I;
		gSERVOPOS_PanUp = SERVOPOS_PanUp_I;
		gSERVOPOS_PanDown = SERVOPOS_PanDown_I;
		gSERVOPOS_Down = SERVOPOS_Down_I;
	}
	else
	{
		// normal
		gSERVOPOS_Up = SERVOPOS_Up;
		gSERVOPOS_PanUp = SERVOPOS_PanUp;
		gSERVOPOS_PanDown = SERVOPOS_PanDown;
		gSERVOPOS_Down = SERVOPOS_Down;
	}


	// Sound-Sequenz auswählen
	var=eeprom_read_byte(&EE_selected_Sequence);
	// wenn EE-Daten plausibel
	if ((var>0) && (var<MAX_SOUND_SEQUENCE))
	{
		// dann übernehmen
		g_selected_Sequence = var;
	}
	else
	{
		// sonst korrigieren
		g_selected_Sequence = 1;
		eeprom_write_byte ( &EE_selected_Sequence, g_selected_Sequence );
		eeprom_write_byte ( &EE_selected_SequenceC, ~g_selected_Sequence );
	}


	// Sound-Aktivierung aus EEPROM
	// wenn EE-Daten plausibel
	if (eeprom_read_byte(&EE_Option_Sound) != eeprom_read_byte(&EE_Option_SoundC)) // bool-Vergleich
	{
		// dann übernehmen
		gOptionSound = eeprom_read_byte ( &EE_Option_Sound );
	}
	else
	{
		// sonst korrigieren
		gOptionSound    = true;
		eeprom_write_byte ( &EE_Option_Sound, gOptionSound );
		eeprom_write_byte ( &EE_Option_SoundC, !gOptionSound );
	}

	gLED_Option		= gOptionSound;
	gLED_OptionPWM  = false;

	gLED_dp  		= false;
	gLED_dpPWM  	= false;

	gDisplayPWM     = false;
	gDisplayChr     = 22; // leer

	// Brühzeit aus EEPROM
	// wenn EE-Daten plausibel
	var=eeprom_read_byte(&EE_BrewtimeSelected);
	if ((var>0) && (var<10))
	{
		// dann übernehmen
		gBrewtimeSelected = var;
	}
	else
	{
		// sonst korrigieren
		gBrewtimeSelected    = 3;
		eeprom_write_byte ( &EE_BrewtimeSelected, gBrewtimeSelected );
		eeprom_write_byte ( &EE_BrewtimeSelectedC, ~gBrewtimeSelected );
	}


	/* Anmerkungen zur Servoansteuerung mit PWM
	siehe http://www.societyofrobots.com/member_tutorials/node/231

	1. Servopuls wiederholt sich alle 20ms (gemessen an Multiples 25ms).
	   Wiederholrate des Pulses ist aber nicht relevant, kann auch kürzer (=schnelleres Servo)
	   oder länger (=langsameres Servo) sein.
	2. Pulsbreite bestimmt Servoposition: 1ms=min, 2ms=max, 1,5ms=Mitte (ggf. auch 0,8-2,2ms)
	3. Unterschied zwischen 1ms und 2ms ist 5% von 20ms, der Timer sollte die Auflösung in diesen 5% schaffen
	   Bei 8bit-Timer (256) wären das 12,8 mögliche Positionen -> weniger als ein Servo kann
	   Bei 16bit-Timer (65535) wären das 3276 mögliche Positionen -> mehr als ein Servo kann
	4. Da Servo eine Art Motorsteuerung ist wird "frequency and phase correct mode" gewählt
	5. ATmega kann 2 Servos à 16bit steuern
	6. beim 16bit-Timer kann ein "mode of operation" gewählt werden, dessen TOP-Wert an 20ms Wiederholrate angepasst werden kann
	--> 16bit Timer1

	Nach Initialisierung kann die Servostellung einfach durch Beschreiben des Registers OCR1A mit Werten zw. 500 und 1000 vorgegeben werden

	*/

	// Timer für Servo-PWM initialisieren
	#if F_CPU == 8000000
	    // Ask for 50Hz (20ms) PWM signal (ms count should be halved)
	    ICR1 = 20000/2 ;
	    // Configure timer 1 for Phase and Frequency Correct PWM mode, with 8x prescaling
	    TCCR1B = (1 << WGM13) | (1 << CS11);
	#elif F_CPU == 1000000
	    // Ask for 50Hz (20ms) PWM signal (ms count should be halved)
	    ICR1 = 20000/2 ;
	    // Configure timer 1 for Phase and Frequency Correct PWM mode, with no prescaling
	    TCCR1B = (1 << WGM13) | (1 << CS10);
	#else
		#error    No F_CPU has been set or it is an unrecognised value
	#endif

	// Timer1: zur Zeitmessung und Ereignissteuerung wird 20ms-Interrupt bei Timer1-BOTTOM verwendet:
	TIMSK = (1 << TOIE1) | (1<<TOIE0); // Interrupt bei Timer1-Overflow und Timer0-Overflow

	// Timer0 Prescaler initialisieren für LED-PWM, normal mode, , Interrupt bei Timer overflow
	TCCR0 = (1<<CS01) | (1<<CS00);	// 8MHz/  64 = 125000,0Hz --> 1 tic=  8µs  -> max.256 Steps =  2ms 
	//TCCR0 = (1<<CS02);				// 8MHz/ 256 =  31250,0Hz --> 1 tic= 32µs  -> max.256 Steps =  8ms 
	//TCCR0 = (1<<CS02) | (1<<CS00);	// 8MHz/1024 =   7812,5Hz --> 1 tic=128µs  -> max.256 Steps = 32ms 

	TCCR1A |= 2 <<  6;  // enable PWM on port B1
//	OCR1A = ICR1 * 2 /20; // 2ms pulse to left motor on PB1
//	OCR1A = 500; // 2ms pulse to left motor on PB1
	OCR1A = gSERVOPOS_Up; // 2ms pulse to left motor on PB1

	// Ziel Servoposition für Interrupt-Routine: ganz oben
	gServoStop  = gSERVOPOS_Up;
	gServoSpeed = SERVOSPEED_LIFT;


	// wenn Sound-Taste beim Einschalten gedrückt wurde, dann zur Sound-Auswahl gehen
	if ( !(TASTENPORT & (1<<TASTE_Option)) ) 
	{
		// warten bis Taste losgelassen wurde
		while ( !(TASTENPORT & (1<<TASTE_Option)) ) 
		{
			fsmExitState = FSMSTATE_SoundSelect;
		}
	}
	else
	{
		// wenn Start-Taste beim Einschalten gedrückt wurde, dann zum Setup gehen
		if ( !(TASTENPORT & (1<<TASTE_Start)) ) 
		{
			// warten bis Taste losgelassen wurde
			while ( !(TASTENPORT & (1<<TASTE_Start)) ) 
			{
				fsmExitState = FSMSTATE_Setup;
			}
		}
		else
		{
			fsmExitState = FSMSTATE_Input;
		}
	}
	
	/**** Daueraktionen ****/
//	fsmExitState = FSMSTATE_Input;

	/**** Ausgangsaktionen ****/
	/* Interrupts freigeben */
	sei();
	return fsmExitState; // Folgestatus
}


/*************************************************
**	fsm Status: Eingabe / Einstellungen
**************************************************/
uint8_t Input(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	/**** Daueraktionen ****/
	while(1)
	{
		// bei Taste "Zeit" Brühzeit erhöhen
		if (gKeyPressed_Time!=0)
		{
			gKeyPressed_Time--;
			gBrewtimeSelected++;
			if (gBrewtimeSelected>9)
			{
				gBrewtimeSelected=1;
			}
		}
		// Anzeige aktualisieren
		gDisplayChr=gBrewtimeSelected;
		gDisplayPWM=false;

		// Option Schwenken abfragen
		CheckOptionButton();

		// bei Taste "Start" geht's los
		if (gKeyPressed_Start!=0)
		{
			gKeyPressed_Start--;
			fsmExitState=FSMSTATE_Start; // Folgestatus
			break;
		}

		if (gKeyCounter_Option >= CNTREPEAT) // taste Option lange gedrückt
		{
			fsmExitState=FSMSTATE_SoundSelect; // Folgestatus
			break;
		}
	}

	/**** Ausgangsaktionen ****/
	return fsmExitState;
}

/*************************************************
**	fsm Status: Optionen
**************************************************/
uint8_t Setup(void)
{
	uint8_t fsmExitState, i;

	/**** Eingangsaktionen ****/
	gDisplayPWM=false;

	/**** Daueraktionen ****/

	// Ausgabe der Programmversion solange keine Taste gedrückt ist
	for (i=1;((gKeyPressed_Time==0)&&(gKeyPressed_Start==0)&&(gKeyPressed_Option==0));i++)
	{
		if (i==1)  // "V" für "Version"
		{
			gDisplayChr=29;
		}
		else   // Versionszeichen
		{
			gDisplayChr=pgm_read_byte_near(&Program_Version[i+8]) - '0';
		}
		long_delay_ms(1000);
		gDisplayChr=45;  // Leerzeichen
		long_delay_ms(500);
		if (i==7)
		{
			i=0;
			long_delay_ms(1500);
		}
	}

	fsmExitState=FSMSTATE_Input; // Folgestatus

	/**** Ausgangsaktionen ****/
	gKeyPressed_Start=0;
	gKeyPressed_Option=0;
	gKeyPressed_Time=0;
	return fsmExitState;

}

/*************************************************
**	fsm Status: Sound-Auswahl
**************************************************/
uint8_t SoundSelect(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	// Pause nach Ende Sound-Sequenz
	gCounterUser=0;        // 0=Sound spielen, 1=warten
	gCounterUserStep=60;   // Pausendauer x 20ms
	gCounterUserStepCNT=0; // Usercounter zurücksetzen

	/**** Daueraktionen ****/
	do
	{
		if (gCounterUser==0)
		{
			// Sound-Sequenz-Nr. anzeigen
			gDisplayChr=g_selected_Sequence;
			// gewählten Sound abspielen
			PlaySoundSequence(g_selected_Sequence);
			gCounterUser=1; // warten
		}
		// wenn Taste Zeit/Option? dann nächsten Sound auswählen und abspielen
		if (gKeyPressed_Option!=0)
		{
			gKeyPressed_Option=0;
			gCounterUser=0; // nicht warten
			gCounterUserStepCNT=0; // Usercounter zurücksetzen
			g_selected_Sequence++;
			if (g_selected_Sequence>MAX_SOUND_SEQUENCE)
			{
				g_selected_Sequence=1;  // zurück zur ersten Melodie
			}
		}
	}
	// wenn Taste Start, dann Sound übernehmen
	while(gKeyPressed_Start==0);

	/**** Ausgangsaktionen ****/
	gKeyPressed_Start=0;
	gKeyPressed_Option=0;
	gKeyPressed_Time=0;

	//	gewählte Sound-Sequenz im EEPROM merken
	eeprom_write_byte ( &EE_selected_Sequence, g_selected_Sequence );
	eeprom_write_byte ( &EE_selected_SequenceC, ~g_selected_Sequence );

	fsmExitState=FSMSTATE_Input; // Folgestatus

	return fsmExitState;

}


/*************************************************
**	fsm Status: Displaytest, ...
**************************************************/
uint8_t Test(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	/**** Daueraktionen ****/
	while(1)
	{
		fsmExitState=FSMSTATE_Setup; // Folgestatus
		break;
	}
	/**** Ausgangsaktionen ****/
	return fsmExitState;
}

/*************************************************
**	fsm Status: Startphase
**************************************************/
uint8_t Start(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	// Einstellungen mit Komplement in EEPROM merken
	eeprom_write_byte ( &EE_Option_Sound, gOptionSound );
	eeprom_write_byte ( &EE_Option_SoundC, !gOptionSound );
	eeprom_write_byte ( &EE_BrewtimeSelected, gBrewtimeSelected );
	eeprom_write_byte ( &EE_BrewtimeSelectedC, ~gBrewtimeSelected );

	//Zielposition setzen
	gServoStop  = gSERVOPOS_Down;
	gServoSpeed = SERVOSPEED_DIVE;
	cli(); // Interrupts sperren um Zeitzähler zurückzussetzen
	gCounter20ms=0;
	gSeconds=0;
	gMinutes=0;
	sei(); // Interrupts wieder freigeben

	/**** Daueraktionen ****/
	while(1)
	{
		// Display aktualisieren
		gDisplayChr=gBrewtimeSelected-gMinutes;
		gDisplayPWM=true;

		CheckOptionButton(); // ggf. nachträglich Schwenken umschalten

		//if (OCR1A>=gSERVOPOS_Down)  // Beutel eingetaucht?
		if (gServoStart==gServoStop)  // Beutel eingetaucht?
		{
			fsmExitState=FSMSTATE_Brew; // Folgestatus
			break;
		}

		if (gKeyPressed_Start!=0)  // Abbruch?
		{
			gKeyPressed_Start--;
			fsmExitState=FSMSTATE_Stop; // Folgestatus
			break;
		}
	}

	/**** Ausgangsaktionen ****/
	return fsmExitState;
}

/*************************************************
**	fsm Status: Brühphase
**************************************************/
uint8_t Brew(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	//gLED_OptionPWM = false;

	/**** Daueraktionen ****/
	while(1)
	{
		// Display aktualisieren
		gDisplayChr=gBrewtimeSelected-gMinutes;
		gDisplayPWM=true;

		CheckOptionButton(); // ggf. nachträglich Sound umschalten

		if (gSeconds==PANBEGIN)  // Zeitphase für schwenken erreicht?
		{
			fsmExitState=FSMSTATE_Pan; // Folgestatus
			break;
		}

		if (gBrewtimeSelected-gMinutes==0)  // Brühzeit erreicht?
		{
			fsmExitState=FSMSTATE_Stop; // Folgestatus
			break;
		}

		if (gKeyPressed_Start!=0)  // Abbruch?
		{
			gKeyPressed_Start--;
			fsmExitState=FSMSTATE_Stop; // Folgestatus
			break;
		}
	}

	/**** Ausgangsaktionen ****/
	return fsmExitState;
}

/*************************************************
**	fsm Status: Schwenkphase
**************************************************/
uint8_t Pan(void)
{
	uint8_t fsmExitState;
	int PanCount;

	/**** Eingangsaktionen ****/
	gLED_OptionPWM = true;
	PanCount=PANTIMES; // x mal Schwenken
	fsmExitState=FSMSTATE_Pan; // Folgestatus

	/**** Daueraktionen ****/
	while(PanCount>0)
	{
		PanCount--;
		if (PanCount==0)
		{
			fsmExitState=FSMSTATE_Brew; // Folgestatus
		}

		// neues Ziel = obere Schwenkposition
		gServoStop  = gSERVOPOS_PanUp;  
		gServoSpeed = SERVOSPEED_PAN;

		while(gServoStart!=gServoStop)
		{
			// Display aktualisieren
			gDisplayChr=gBrewtimeSelected-gMinutes;
			gDisplayPWM=true;

			CheckOptionButton(); // ggf. nachträglich Schwenken umschalten

			if (gKeyPressed_Start!=0)  // Abbruch?
			{
				gKeyPressed_Start--;
				PanCount=0; // Ende Schwenken
				fsmExitState=FSMSTATE_Stop; // Folgestatus
				break;
			}
		}

		if (fsmExitState!=FSMSTATE_Stop) // wenn zuvor nicht abgebrochen wurde
		{
			// neues Ziel = obere Schwenkposition
			gServoStop  = gSERVOPOS_PanDown;  
			gServoSpeed = SERVOSPEED_PAN;

			//while(OCR1A<gSERVOPOS_PanDown)
			while(gServoStart!=gServoStop)
			{
				// Display aktualisieren
				gDisplayChr=gBrewtimeSelected-gMinutes;
				gDisplayPWM=true;

				CheckOptionButton(); // ggf. nachträglich Schwenken umschalten

				if (gKeyPressed_Start!=0)  // Abbruch?
				{
					gKeyPressed_Start--;
					PanCount=0; // Ende Schwenken
					fsmExitState=FSMSTATE_Stop; // Folgestatus
					break;
				}
			}
		}
	}

	/**** Ausgangsaktionen ****/
	gServoStop  = gSERVOPOS_Down; // wieder zuürck zu unteren Brühposition
	gServoSpeed = SERVOSPEED_PAN;
	gLED_OptionPWM = false;
	return fsmExitState;
}

/*************************************************
**	fsm Status: Stopphase
**************************************************/
uint8_t Stop(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	gServoStop  = gSERVOPOS_Up;
	gServoSpeed = SERVOSPEED_LIFT;

	/**** Daueraktionen ****/
	while(OCR1A>gSERVOPOS_Up)  // warten bie Teebeutel oben
	{
		// Display aktualisieren
		gDisplayChr=gBrewtimeSelected-gMinutes;
		gDisplayPWM=true;
	}

	/**** Ausgangsaktionen ****/
	fsmExitState=FSMSTATE_End; // Folgestatus
	return fsmExitState;
}

/*************************************************
**	fsm Status: Endphase (Brühvorgang beendet)
**************************************************/
uint8_t End(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	// Geschwindigkeit des Lauflicht-Anzeigewechsels setzen
	gCounterUser=6;      // Anzahl Zeichen
	gCounterUserStep=4;  // Geschwindigkeit

	// am Ende des Brühvorgangs Tonfolge abspielen und jede Minute wiederholen, bis Endphase verlassen wurde.
	// Tonsignal ausgeben, wenn Soundoption gesetzt
	if (gOptionSound)
	{
		PlaySoundSequence(g_selected_Sequence);
		gMinutesOld = gMinutes; // Minutenwechsel merken
	}

	fsmExitState=FSMSTATE_End;

	/**** Daueraktionen ****/
	while(fsmExitState==FSMSTATE_End)
	{
		/**** Daueraktionen ****/
		// Display aktualisieren
		if (gCounterUser==0)
		{
			gCounterUser=6; // Schlange 2-Segment [6]
			//gCounterUser=15; // Klappen [15]
		}
		gDisplayChr=22-gCounterUser; // Schlange 2-Segment [6]
		//gDisplayChr=45-gCounterUser; // Klappen [15]
		gDisplayPWM=false;

		// einmal jede Minute Tonsignal
		if (gOptionSound)
		{
			if (gMinutesOld != gMinutes)  // neue Minute?
			{
				PlaySoundSequence(g_selected_Sequence);
				gMinutesOld = gMinutes; // Minutenwechsel merken
			}
		}

		if ((gKeyPressed_Time!=0)||(gKeyPressed_Start!=0)||(gKeyPressed_Option!=0))  // Taste gedrückt?
		{
			gKeyPressed_Time=0;
			gKeyPressed_Start=0;
			//gKeyPressed_Option=0;  // Taste Option zur Beendigung der Endphase nicht ignorieren
			fsmExitState=FSMSTATE_Input; // Folgestatus
		}
	}

	/**** Ausgangsaktionen ****/
	return fsmExitState;
}

/*************************************************
**	fsm Status: Batterie-Unterspannung
**************************************************/
uint8_t Battlow(void)
{
uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	gDisplayChr=29; // "U"
	gDisplayPWM=false;

	/**** Daueraktionen ****/
	while(fsmExitState==FSMSTATE_Battlow)
	{
		fsmExitState=FSMSTATE_Battlow; // Folgestatus
	}

	/**** Ausgangsaktionen ****/
	return fsmExitState;
}

/*************************************************
**	fsm Status: Fehler 
**************************************************/
uint8_t Error(void)
{
	uint8_t fsmExitState;

	/**** Eingangsaktionen ****/
	gDisplayChr=28; // "E"
	gDisplayPWM=false;

	/**** Daueraktionen ****/
	while(fsmExitState==FSMSTATE_Error)
	{
		fsmExitState=FSMSTATE_Error; // Folgestatus
	}

	/**** Ausgangsaktionen ****/
	return fsmExitState;
}


/****************************************************************************
*****************************************************************************
*****   Hauptprogramm                                                   *****
*****************************************************************************
****************************************************************************/
int main( void )
{
	uint8_t fsmNextState;

	fsmNextState=FSMSTATE_Init;

	while(1)  // Endlosschleife: finite sate machine
	{
		switch (fsmNextState)
			{
			case FSMSTATE_Init:			// fsm Status: Initialisierung
					fsmNextState=Init();
					break;
			case FSMSTATE_Input:		// fsm Status: Eingabe / Einstellungen
					fsmNextState=Input();
					break;
			case FSMSTATE_Setup:		// fsm Status: Optionen
					fsmNextState=Setup();
					break;
			case FSMSTATE_SoundSelect:	// fsm Status: Sound-Auswahl
					fsmNextState=SoundSelect();
					break;
			case FSMSTATE_Test:			// fsm Status: Displaytest, ...
					fsmNextState=Test();
					break;
			case FSMSTATE_Start:		// fsm Status: Startphase
					fsmNextState=Start();
					break;
			case FSMSTATE_Brew:			// fsm Status: Brühphase
					fsmNextState=Brew();
					break;
			case FSMSTATE_Stop:			// fsm Status: Stopphase
					fsmNextState=Stop();
					break;
			case FSMSTATE_End:			// fsm Status: Endphase (Brühvorgang beendet)
					fsmNextState=End();
					break;
			case FSMSTATE_Pan:			// fsm Status: Schwenkphase
					fsmNextState=Pan();
					break;
			case FSMSTATE_Battlow:		// fsm Status: Batterie-Unterspannung
					fsmNextState=Battlow();
					break;
			default:					// fsm Status: Fehler
				fsmNextState=Error();
				break;
			}
/*		if (gUbatt<UBATTMIN)
		{
			fsmNextState=FSMSTATE_Battlow;
		} */
	}
}
