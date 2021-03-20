/*
	TeaTimer.h        (c) 2008 Frank Scholl

	Programm zur Steuerung des Teebeutel-Automaten
	für ATmega168/ATmega8 mit Servo und 7-Segmentanzeige


Fusebits:
	in Ponyprog alle Clksel deaktiviert = 1111 = ext. Quarz
	Wenn AVRISP mkII nicht geht, kann es daran liegen, dass ein neuer (unprogrammierter) 
	ATmega8 mit 1MHz intern läuft, die Programmierung muss aber 1/4 der Taktfrequenz sein!

*/

#ifndef F_CPU
#define F_CPU 8000000UL     /* Quarz Frequenz 8.0000 Mhz  */
#endif


#ifndef BYTE
typedef unsigned char BYTE;
#endif

#define	 SEG7_e      PB0
#define	 SERVO_PWM   PB1
#define	 SEG7_d      PB2
#define	 UBATT	     PC0
#define	 UBATT_CH    0  // ADC-Channel 0 für Ubatt
#define	 SEG7_c      PC1
#define	 SEG7_a      PC2
#define	 SEG7_f      PC3
#define	 SEG7_g      PC4
#define	 SEG7_b      PC5

#define TASTENPORT   PIND  	// Port für Tasten
#define DEBUGPIN2    PD0  	// Debug-Signal
#define SOUNDPIN     PB3  	// Frequenz für Lautsprecher
#define LED_Signal   PD1  	// LED Signal
#define TASTE_Zeit   PD2  	// = "Taste1"
#define TASTE_Start  PD3  	// = "Taste2"
#define TASTE_Option PD4  	// = "Taste3"
#define LED_Option   PD5  	// LED Option
#define SEG7_dp      PD6  	// 7-Segment-Anzeige dp
#define DEBUGPIN1    PD7  	// Debug-Signal
#define CNTDEBOUNCE  5	 	// Schwellwerte Tasten-Entprellung
#define CNTREPEAT    200	// "Taste lange gedrueckt:"
#define ButtonRepeatDelayms 100 // Tastenverzögerung bei Dauertaste in ms
#define DISPLAY_TYPE_SELECT  PD7   // Auswahl des 7-Segment-Typs
#define SERVO_INVERSE_SELECT  ADC7   // Auswahl des Servo-Typs


// Stati des Zustandsautomaten:
#define FSMSTATE_Init			0		// fsm Status: Initialisierung
#define FSMSTATE_Input			1		// fsm Status: Eingabe / Einstellungen
#define FSMSTATE_Setup			2		// fsm Status: Optionen
#define FSMSTATE_SoundSelect	3		// fsm Status: Sound-Auswahl
#define FSMSTATE_Test			4		// fsm Status: Displaytest, ...
#define FSMSTATE_Start			5		// fsm Status: Startphase
#define FSMSTATE_Brew			6		// fsm Status: Brühphase
#define FSMSTATE_Stop			7		// fsm Status: Stopphase
#define FSMSTATE_End			8		// fsm Status: Endphase (Brühvorgang beendet)
#define FSMSTATE_Pan			9		// fsm Status: Schwenkphase
#define FSMSTATE_Battlow		10		// fsm Status: Batterie-Unterspannung
#define FSMSTATE_Error			99		// fsm Status: Fehler


/* ----------------------- begin globale Variablen ------------------------------- */
/* beachte: volatile! --> damit wird bei jedem Zugriff auf die Variable aus dem Speicher gelesen, 
  sonst würde Compiler wegoptimieren und Datenaustausch mit Interruptroutine funktioniert nicht! */
volatile uint8_t gKeyCounter_Time   = 0; // hier kann das Hauptprogramm sehen, ob eine Taste aktuell gedrückt ist und wie lange
volatile uint8_t gKeyPressed_Time   = 0; // hier kann das Hauptprogramm sehen, ob die Taste zuvor gedrückt wurde und wie oft (kurz)
volatile uint8_t gKeyCounter_Start  = 0; // hier kann das Hauptprogramm sehen, ob eine Taste aktuell gedrückt ist und wie lange
volatile uint8_t gKeyPressed_Start  = 0; // hier kann das Hauptprogramm sehen, ob die Taste zuvor gedrückt wurde und wie oft (kurz)
volatile uint8_t gKeyCounter_Option = 0; // hier kann das Hauptprogramm sehen, ob eine Taste aktuell gedrückt ist und wie lange
volatile uint8_t gKeyPressed_Option = 0; // hier kann das Hauptprogramm sehen, ob die Taste zuvor gedrückt wurde und wie oft (kurz)

volatile uint8_t gCounter20ms = 0;       // Zähler für 20ms-Interrupts
volatile uint8_t gSeconds     = 0;     	 // Zähler für Sekunden
volatile uint8_t gMinutes     = 0;     	 // Zähler für Minuten
volatile uint8_t gMinutesOld  = 0;     	 // Vergleichsmerker für neue Minute

volatile uint8_t gCounterUser = 0;       // globaler Zähler für Lauflicht usw.
volatile uint8_t gCounterUserStep = 0;   // globaler Zähler Schrittweite in 20ms
volatile uint8_t gCounterUserStepCNT = 0;   // Schrittweitenzähler

volatile uint8_t gBrewtimeSelected;		// gewählte Brühzeit in Minuten
volatile bool    gOptionSound;				// Option für Tonsignal
volatile uint8_t g_selected_Sequence;	// Merker gewählte Sound-Sequenz

volatile uint8_t gDisplayChr  = 0;     	 // Nr. des Codes, des im 7-Segment-Display angezeigt werden soll
volatile bool    gDisplayPWM  = false;     	 // false=statisch hell, true=PWM-Dimmen
volatile bool    gDisplayTypeCommonCathode  = true;  	// Typ des 7-Segment-Displays: true= gemeinsame Kathode, false= gemeinsame Anode

volatile bool	 gLED_Signal  		= false;   	// gewünschter LED_Signal Zustand
volatile bool	 gLED_SignalPWM 	= false;    // false=statisch hell, true=PWM-Dimmen
volatile bool 	 gLED_Option  		= false;   	// gewünschter LED_Signal Zustand
volatile bool	 gLED_OptionPWM 	= false;    // false=statisch hell, true=PWM-Dimmen
volatile bool	 gLED_dp  			= false;	// gewünschter LED_Signal Zustand
volatile bool	 gLED_dpPWM 		= false;    // false=statisch hell, true=PWM-Dimmen

// normal:
#define SERVOPOS_Up       500  // Servoposition ganz oben  (Beutel heraus)
#define SERVOPOS_PanUp    750  // Servoposition Schwenken oben
#define SERVOPOS_PanDown  900  // Servoposition Schwenken unten
#define SERVOPOS_Down    1000  // Servoposition ganz unten (Beutel eingetaucht)
// invers:
#define SERVOPOS_Up_I      1000  // Servoposition ganz oben  (Beutel heraus)
#define SERVOPOS_PanUp_I    750  // Servoposition Schwenken oben
#define SERVOPOS_PanDown_I  600  // Servoposition Schwenken unten
#define SERVOPOS_Down_I     500  // Servoposition ganz unten (Beutel eingetaucht)

volatile uint16_t gSERVOPOS_Up      = SERVOPOS_Up;       // Merker für Servoposition (normal/invers)
volatile uint16_t gSERVOPOS_PanUp   = SERVOPOS_PanUp;    // Merker für Servoposition (normal/invers)
volatile uint16_t gSERVOPOS_PanDown = SERVOPOS_PanDown;  // Merker für Servoposition (normal/invers)
volatile uint16_t gSERVOPOS_Down    = SERVOPOS_Down;     // Merker für Servoposition (normal/invers)

volatile uint16_t gServoStart = SERVOPOS_Up;     // OCR-Wert für Start Servobewegung
volatile uint16_t gServoStop  = SERVOPOS_Up;     // OCR-Wert für Ende Servobewegung
volatile uint8_t  gServoSpeed = 10;    	 // max. Winkelgerschwindigkeit des Servos (=Steps/20ms)

#define PANBEGIN 20 // Beginn Schwenk-Zeitphase in Sekunden in einer Minute
#define PANTIMES 2  // Anzahl Schwenkbewegungen pro Minute
#define SERVOSPEED_DIVE  4  // Servogeschwindigkeit eintauchen
#define SERVOSPEED_LIFT  3  // Servogeschwindigkeit herausholen
#define SERVOSPEED_PAN   2  // Servogeschwindigkeit Schwenken

#define LEDPWM_Lo 1  // min. Wert für LEDPWM_TCNT0_ON (dunkel)
#define LEDPWM_Hi 200 // max. Wert für LEDPWM_TCNT0_ON (hell)
volatile uint8_t gLEDPWM_TCNT0_ON; // TCNT0-Startwert (Zählerstand) für LED-on-Zeit
volatile uint8_t gLEDPWM_TCNT0_OFF; // TCNT0-Startwert (Zählerstand) für LED-off-Zeit
volatile uint8_t gLEDPWM_OC     = LEDPWM_Lo; // Output compare Wert für LED-PWM
volatile uint8_t gLEDPWM_DIR    = 1;     	 // gibt die Richtung der PWM-Änderung an (1=heller, 0=dunkler)
volatile uint8_t gLEDPWM_SPEED  = 2;     	 // so oft muss der PWM-Einschalt-Interrupt ausgeführt werden, bis die Helligkeit (PWM) geändert wird (Geschwindigkeit der Änderung)
volatile uint8_t gLEDPWM_CNT    = 1;     	 // Zähler zur PWM-Änderung (gLEDPWM_SPEED)
volatile bool    gLEDPWM_NEXT_ON= true;    	 // Schalter für LED-PWM: soll im Int. als nächstes aus- oder eingeschaltet werden?

volatile uint8_t gUbatt     = 200;     	 // (gefilterter) Wert der Batteriespannung
#define  UBATTMIN 1   // Grenzwert Unterspannung

/* ------------------------- end globale Variablen ------------------------------- */

/* ------------------------ begin EEPROM-Variablen ------------------------------- */
// die im EEPROM abgelegten Einstellungen
static bool    EE_Option_Sound;			// Merker letzte Einstellung Schwenkoption
static bool    EE_Option_SoundC;			// Merker letzte Einstellung Schwenkoption Komplement
static uint8_t EE_BrewtimeSelected;		// Merker letzte Einstellung Brühzeit
static uint8_t EE_BrewtimeSelectedC;	// Merker letzte Einstellung Brühzeit Komplement
static uint8_t EE_selected_Sequence;	// Merker gewählte Sound-Sequenz
static uint8_t EE_selected_SequenceC;	// Merker gewählte Sound-Sequenz Komplement


/* -------------------------- end EEPROM-Variablen ------------------------------- */

/* ------------------------- begin FLASH-Konstanten ------------------------------ */

/* Codes für 7-Segment-Anzeige: das 1.Byte ist der Code für PC, das 2. für PB */
static const PROGMEM BYTE AnzeigeCode[46][2] =  //neu
{            // (Index)
// Ziffern [10]
{0x2E,0x05}, // (0) "0"
{0x22,0x00}, // (1) "1"
{0x34,0x05}, // (2) "2"
{0x36,0x04}, // (3) "3"
{0x3A,0x00}, // (4) "4"
{0x1E,0x04}, // (5) "5"
{0x1E,0x05}, // (6) "6"
{0x26,0x00}, // (7) "7"
{0x3E,0x05}, // (8) "8"
{0x3E,0x04}, // (9) "9"

// Schlange 1-Segment [6]
{0x04,0x00}, // (10)  ^
{0x20,0x00}, // (11)   '
{0x02,0x00}, // (12)   ,
{0x00,0x04}, // (13)  _
{0x00,0x01}, // (14) ,
{0x08,0x00}, // (15) '

// Schlange 2-Segment [6]
{0x24,0x00}, // (16)  ^'
{0x22,0x00}, // (17)   |
{0x02,0x04}, // (18)  _,
{0x00,0x05}, // (19) ,_
{0x08,0x01}, // (20) |
{0x0C,0x00}, // (21) '^

// Balken [4]
{0x00,0x00}, // (22)  leer
{0x04,0x00}, // (23)  ^
{0x10,0x00}, // (24)  -
{0x00,0x04}, // (25)  _

// alpahnumerisch [4]
{0x00,0x00}, // (26) leer
{0x3E,0x01}, // (27) "A"
{0x1C,0x05}, // (28) "E"
{0x2A,0x05}, // (29) "U"

// Klappen [15]
{0x04,0x00}, // (30)  ^
{0x24,0x00}, // (31)  ^'
{0x20,0x00}, // (32)   '
{0x30,0x00}, // (33)  -'
{0x12,0x00}, // (34)  -,
{0x02,0x00}, // (35)   ,
{0x02,0x04}, // (36)  _,
{0x00,0x04}, // (37)  _
{0x00,0x05}, // (38) ,_
{0x00,0x01}, // (39) ,
{0x10,0x01}, // (40) ,-
{0x10,0x00}, // (41)  -
{0x18,0x00}, // (42) '-
{0x08,0x00}, // (43) '
{0x0C,0x00}, // (44) '^

{0x00,0x00}  // (45) leer
};

// Sound-Seuqenzen
#define MAX_SOUND_SEQUENCE  5   // die höchste auswählbare Sound-Sequenz
static const PROGMEM BYTE SoundCodes[145][3] =  //neu
// {Ton-Nr., Ton-Dauer, Pausendauer}
{
// Sound-Seuqenz Nr.1 [4 Feldeinträge] einfacher 3-Klang
{52+12, 100, 0},
{56+12, 100, 0},
{59+12, 100, 0},
{0,0,0},
// Sound-Seuqenz Nr.2 [8 Feldeinträge] McDonalds-Spiel
{52, 200, 0},
{64, 100, 0},
{ 0, 100, 0},
{64, 100, 0},
{76, 100, 0},
{71, 100, 0},
{64, 100, 0},
{0,0,0},
// Sound-Seuqenz Nr.3 [6 Feldeinträge] Tataa
{64, 200, 5},
{67, 200, 0},
{67, 200, 0},
{67, 200, 0},
{67, 200, 5},
{0,0,0},
// Sound-Seuqenz Nr.4 [32 Feldeinträge] Smoke on the water
// http://reference.findtarget.com/search/Smoke%20on%20the%20Water/
{59, 100, 200},
{62, 100, 200},
{64, 200, 0},
{64, 200, 0},
{64, 200, 0},
{64, 100, 0},
{64, 100, 100},
{59, 100, 200},

{62, 100, 200},
{65, 100, 200},
{64, 200, 0},
{64, 200, 0},
{64, 200, 0},
{64, 200, 0},
{64, 200, 0},
{64, 100, 100},

{59, 100, 200},
{62, 100, 200},
{64, 200, 0},
{64, 200, 0},
{64, 200, 0},
{64, 100, 0},
{64, 100, 100},
{62, 100, 200},

{59, 200, 0},
{59, 200, 0},
{59, 200, 0},
{59, 200, 0},
{59, 200, 0},
{59, 200, 0},
{59, 200, 0},
{59, 200, 5},

{0,0,0},
// Sound-Seuqenz Nr.5 [94 Feldeinträge] Guten Abend, gut' Nacht
// Noten von http://www.lieder-archiv.de/lieder/show_song.php?ix=300030
// Leerzeile nach jedem Takt
// 1. Zeile
{57, 195, 5},
{57, 195, 5},

{60, 200, 0},
{60, 200, 0},
{60, 195, 5},
{57, 195, 5},
{57, 200, 0},
{57, 195, 5},

{60, 200, 0},
{60, 195, 5},
{ 0, 200, 0},
{57, 195, 5},
{60, 195, 5},

{65, 200, 0},
{65, 195, 5},
{64, 200, 0},
{64, 200, 0},
{64, 195, 5},
{62, 195, 5},

// 2. Zeile
{62, 200, 0},
{62, 195, 5},
{60, 200, 0},
{60, 195, 5},
{55, 195, 5},
{57, 195, 5},

{58, 200, 0},
{58, 195, 5},
{55, 200, 0},
{55, 195, 5},
{55, 195, 5},
{57, 195, 5},

{58, 200, 0},
{58, 195, 5},
{ 0, 200, 0},
{55, 195, 5},
{58, 195, 5},

{64, 195, 5},
{62, 195, 5},
{60, 200, 0},
{60, 195, 5},
{64, 200, 0},
{64, 195, 5},

// 3. Zeile
{65, 200, 0},
{65, 195, 5},
{ 0, 200, 0},
{53, 195, 5},
{53, 195, 5},

{65, 200, 0},
{65, 200, 0},
{65, 200, 0},
{65, 195, 5},
{62, 195, 5},
{58, 195, 5},

{60, 200, 0},
{60, 200, 0},
{60, 200, 0},
{60, 195, 5},
{57, 195, 5},
{53, 195, 5},

// 4. Zeile
{58, 200, 0},
{58, 195, 5},
{60, 200, 0},
{60, 195, 5},
{62, 200, 0},
{62, 195, 5},

{60, 200, 0},
{60, 200, 0},
{60, 200, 0},
{60, 195, 5},
{53, 195, 5},
{53, 195, 5},

{65, 200, 0},
{65, 200, 0},
{65, 200, 0},
{65, 195, 5},
{62, 195, 5},
{58, 195, 5},

// 5. Zeile
{60, 200, 0},
{60, 200, 0},
{60, 200, 0},
{60, 195, 5},
{57, 195, 5},
{53, 195, 5},

{58, 200, 0},
{58, 195, 5},
{57, 200, 0},
{57, 195, 5},
{55, 200, 0},
{55, 195, 5},

{53, 200, 0},
{53, 200, 0},
{53, 200, 0},
{53, 195, 5},

{0,0,0}
};


/* Programm-Version im Flash */
const char Program_Version[] PROGMEM = "TeaTimer V091110";


/* --------------------------- end FLASH-Konstanten ------------------------------ */

