#ifndef _MAIN_H_
#define _MAIN_H_

//=================================================================
//#define seiler     //für das neue Board von Richi Seiler REVOX A77+B77
#define A77 1
//#define B77 1
//#define uher 1
//#define teac 1

//#define playtimer

#define synckst 1

#include <stdint.h>
#include <avr/interrupt.h>
#include "irmp.h"
#include <TimerOne.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define tmin26 1900
#define tmax26 4200
#define tmid26 3248
#define tmin22 1720		//TEAC 1700, Braun 1680, Revox/BASF: 1650, nach 1min: 1690
#define tmax22 3450
#define tmid22 2692
#define tmin18 1050
#define tmax18 2698
#define tmid18 2044
//------------------------------------------------------------------------
#define playtime 800	//min. Zeit/U fuer play bei 38cm/s
#define t2626 1400 //1400	//1400/2800/5600 bei 38/19/9.5
#define t2222 1150	//1200/2300/4600
#define t1818  900	// 900/1800/3600
//-------------------------------------------------------------------------
//16 Bit fuer EEPROM:
//die ersten Bytes werden gespeichert bei Spannungsabfall
#define bandd		buff16[0]
#define umrabs		buff16[1]
#define oldspeed	buff16[2]
#define umlabs		buff16[3]	
#define timems		buff16[4]
#define revcalset	buff16[5]
#define tapeno  	buff16[6]
#define stpkst		buff16[7]
#define lockst		buff16[8]
#define cc			buff16[9]
#define lend		buff16[10]
#define loctime		buff16[11]
#define resttime	buff16[12]

#define minrightl	buff32
//Achtung: jede Aenderung oben, verschiebt auch buff8
//8 Bit fuer EEPROM:
#define timerflg	buff8[0]
#define endflg		buff8[1]
#define ilocn		buff8[2]
#define spule		buff8[3]	//Spulengroesse
#define ldivisor	buff8[4]
#define rdivisor	buff8[5]	
#define revplay		buff8[6]	//reverse mode
#define calpwron	buff8[7]
#define locplay		buff8[8]
#define sekunden  buff8[9]

#define ilocmax	10	//Anzahl Locations pro Band (0...9)
#define ziffnr	10	//Anzahl Ziffern
#define lwnr    11	//Anzahl Laufwerkfunktionen

//==============================================================
// Definition der Arduino-Ports:
//IR-Sensor auf A1=PC1, wird in irmpconfig.h gesetzt
#define lsensor 2 //Spulensensor links
#define rsensor 3

#ifdef seiler    //für das neue Board von Richi Seiler
  #define rectaste  12
  #define stoptaste	11
  #define playtaste	10
  #define ffwdtaste	9
  #define frewtaste	8
  #define taste2 		13  //Lerntatste
  #define fwdpin 		A3  //FFWD-Signal vom Bandgeraet
  #define rewpin 		A2  //FREW-Signal vom Bandgeraet
  #define pwrpin 		A0  //PWR-Down-Input
  //----------------------------------------------------
#else // nicht für Seiler Board
  #define taste2    10  //Lerntatste
  #define fwdpin    11  //FFWD-Signal vom Bandgeraet
  #define rewpin    A2  //FREW-Signal vom Bandgeraet
  #define pwrpin    A0  //PWR-Down-Input
#endif  //Seiler Board
  //----------------------------------------------------
  #define ffwd  4
  #define frew  5
  #define play  6
  #define tstop 7
  #ifdef A77
    #define rec   5
  #elif B77
    #define rec   0
    #define pause 1
  #endif
  
  #define relffwd() digitalWrite(ffwd, 1); 
  #define relfrew() digitalWrite(frew, 1);  
  #define relrec()  digitalWrite(rec,  1);  
  #define relplay() digitalWrite(play, 1);  
  
  #define relnffwd() digitalWrite(ffwd, 0);
  #define relnfrew() digitalWrite(frew, 0);
  #define relnrec()  digitalWrite(rec,  0);
  #define relnplay() digitalWrite(play, 0);
  
#ifdef A77  
  #define relstart() digitalWrite(tstop, 1); 
  #define relstop()  digitalWrite(tstop, 0);
  
#elif B77    //für neues IC UDP 2981
  #define relstop()  digitalWrite(tstop, 1);
  #define relstart() digitalWrite(tstop, 0); 
  #define relpause() digitalWrite(pause, 1);  
  #define relnpause() digitalWrite(pause, 0);  
#endif

//==============================================================

//IR-Codenummern fuer Laufwerk: (Reihenfolge ist wichtig, siehe irlearn)
#define	ctimein	10	//muss so stehen bleinen
#define	ctimer	11  //Schaltuhrbetrieb
#define	clocn	12	//Locater setzen
#define ctimlen 13 //Play Timer
#define cfrew 14
#define	cplay	15
#define	cffwd	16
#define	cstop	17
#define	crec	18
#define	cplayr 19
#define	crecr	20

#define iradr  taste0[ziffnr+lwnr]   //letztes Element von taste0

#define zeile1() lcd.setCursor( 0, 0 )
#define lcd_char lcd.print
#define lcd_string lcd.print
#define lcd_number lcd.print
#define lcd_clear lcd.clear
#endif // __MAIN_H_
