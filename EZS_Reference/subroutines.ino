#include "main.h"
#include "irmp.h"

//Deklarationen für alle folgenden Funktionen
//Achtung: Deklaration in einer Funktion wird bei jedem Aufruf neu initialisiert

#define pi 3.141592654
#define ummax 30  //Anzahl Umdrehungen fuer Kalibration
int8_t i, calallow=1, lcnt=0, locs=0, umflg=1, divisor, lfalling=1, rfalling=1;
int16_t umabs, leftmw, rightmw, urold, loctimer=2000, u16l, lastu16, first=1, tnos, lendx, ukold, oldtime=-1;
double nn, len0q, len1q, lastlen, lenword, tmpdbl, lendiff, timekorr;
volatile uint16_t tlkurz, trkurz;
uint8_t pcnt;
//-----------------------------------------------------------------
void zeile2(void)
{
  lcd_setcursor( 0, 2 );
  //lcd_string("                "); //16 blanks, führt zum flimmern
  //lcd_setcursor( 0,  2 );
}
//-----------------------------------------------------------------
void setrev()
{
	if (revcalset){
		divisor = ldivisor;
		umabs = umlabs;
	}
	else {
		divisor = rdivisor;
		umabs = umrabs;
	}
	if (revplay){
		leftmw = trightmw;
		rightmw = tleftmw;
	}
	else {
		leftmw = tleftmw;
		rightmw = trightmw;
	}
}
//-----------------------------------------------------------------
int16_t vornum(uint8_t cnt, uint8_t xrun)	//cnt = Anzahl der zu erwarteten Ziffern
{
	int16_t tmplong = 0, first = -1;
	locnflg = timerflg = 0;		//müssen neu aktiviert werden

	//if (stopflg){	//Eingabe nur in STOP möglich, sonst werden IR-Daten als Var gesehen
	if (stopflg || xrun){	//Eingabe in STOP/RUN möglich, sonst werden IR-Daten als Var gesehen
		lcd_string(" new: ");

		for (i=0; i<cnt; i++)
		{
			if ( (command = ir_get(3000)) == 255 ) return (first<0) ? -1 : tmplong;//3sec warten
			ircodenr = first = 0;
			//Loop vergleicht gespeicherten mit empfangenem Code
			while (taste0[ircodenr] != command)
			{
				ircodenr++;
				if (ircodenr > 9) return -2;//falsche Eingabe, Menu von Code 0 bis 9, 10=ctimein
			}
			lcd_char(ircodenr);
			tmplong *= 10;
			tmplong += ircodenr;	//speichere neuen Zifferncode
		}
	}
	else tmplong = -1;

	_delay_ms(500);	//Anzeigedauer
	ir_auswert();	//IR-Eingabe und Steuerung, muss sein
	_delay_ms(500);	//Anzeigedauer

	return tmplong;
}
int16_t vorlauf(uint16_t var, uint8_t cnt)	//cnt = Anzahl der zu erwarteten Ziffern
{
	zeile2();
	lcd_string("act:");
	lcd_number(var);
	return vornum(cnt, 0);
}

int16_t vortime(uint8_t cnt, uint8_t xrun)	//cnt = Anzahl der zu erwarteten Ziffern
{
	zeile2();
	//lcd_time(var,2);
	return vornum(cnt, xrun);
}

//-----------------------------------------------------------------
//Schaltuhrbetrieb: startet nach Einschalten immer in Record
void timerrout()
{
	if (revplay) ircodenr = crecr;	//gearmed
	else ircodenr = laufwcode = crec;
  ir_play(revplay);
}
//-----------------------------------------------------------------
/*
Bis Ziel spulen, kurz davor anhalten. 
Es sollte kein Richtungswechsel erfolgen wg. Schlaufenbildung
Stopvorlauf bei A77: stpkst=220 bei allen Spulengroessen
*/
uint8_t nearstop(int16_t number)	//number ist die Zeit an der gestoppt werden soll
{
int16_t  diff, kst;
	if (relaisbusy) return 0;	//wenn reltime noch nicht abgelaufen

	kst = stopkst(); //Location-pretime 30Sek. bei playspeed 19cm/s, 15Sek. bei 38cm/s
	if (midtime != tmid26)	kst = (lockst*2) / 3;	// 1/3 kleiner als 26er

	diff = timeword - number;	//number ist die Zeit an der gestoppt wird
	if (first && abs(diff) < kst) return 1;	//keine weitere Aktion, da zu nahe am Ziel

	first = 0;
	allow = 0;	//do not allow ext. revpin

	//immer Umspulen aktiv:
	if (abs(diff) < kst){	//wenn auf 30Sek. angenähert: STOP auslösen
		ir_stop();
		if (stopflg){	//nur wenn Band steht
			if(revplay)	laufwcode = cplayr;	//immer auf play gehen
			else		laufwcode = cplay;
			first = 1;	//Stop aktiv
			return 1;
		}
	}
	//else if (nearflg) ir_stop();	//zu weit gespult
	return 0;
}
//-----------------------------------------------------------------
uint16_t stopkst(void)	//wird auch in main.c genutzt
{
	//bei 19cm/s kst = 30 Sek. vor Ziel mit Bremsen anfangen, da Spielzeit schneller erreicht wird
	// bei Uher mit 26er Metallspulen: kst=600 sec
	tmp16 = lockst;
	if      (oldspeed == 380) tmp16 /= 2;		//15 Sek vor Ziel mit Bremsen anfangen
	else if (oldspeed == 95)  tmp16 *= 2;	//1min vor Ziel mit Bremsen anfangen
	else if (oldspeed == 48)  tmp16 *= 4;	//2min vor Ziel mit Bremsen anfangen
	return tmp16;
}
//---------Taste 1--------------------------------------------------------
// Berechnung der Geschwindigkeit:
// V = len/time0, damit auch Speed beim Umspulen angezeigt wird
void dispspd()
{
	zeile2();
	if (stopflg) tmpdbl = 0.;	//keine Anzeige bei stop
	else	tmpdbl = lendiff / (double)rightmw;	//e0
	tmp16 = tmpdbl / 10.;	//ja nicht aufrunden
	lcd_number(tmp16);
	lcd_char('.');
	i = (int16_t)tmpdbl % 10;	//Modulo
	lcd_char(i);
	lcd_string("cm/s ");

	tmp16 = lenword / 1000.;	//mm, Länge in m
	lcd_number(tmp16);
	lcd_char('m');
}
//----------Taste 2-------------------------------------------------------
//Umdrehungszeit der linken und rechten Spule, mit Mittelwert time0
void disptim(void)
{
	zeile2();
	lcd_number(tleftmw);
	lcd_setcursor( 6, 2 );
	lcd_number(time0);
	lcd_char('t');
	lcd_setcursor( 12, 2 );
  lcd_number(trightmw);
}
//-------Taste 3----------------------------------------------------------
// Anzeige Absolutwerte (seit Bandanfang) von Zeit und Umdrehungen
void dispz3(void)
{
	if (z2blank) lcd_clear();  //wegen flimmern
  lcd_setcursor(0, 1 );
	lcd_number(cc);
	lcd_setcursor(5, 1 );
	lcd_number(lend);
	lcd_setcursor(8, 1 );
	lcd_time(abstime, 2);
	lcd_setcursor(8, 1 );
	lcd_char('c');	//calculated
  
	zeile2();
  lcd_setcursor( 0, 2 );
  lcd_time(resttime,2);
  lcd_setcursor( 8, 2 );
	lcd_time(timeword,2);
}
//--------Taste 4---------------------------------------------------------
// Anzeige Umdrehungen links und rechts und delaytime
void dispz4(void)
{
	zeile2();
  lcd_char('U');
	lcd_number(-umlabs);
	lcd_char(' ');
	lcd_number(umrabs);
	lcd_char(' ');
  lcd_char('d');
  lcd_number(dlytime);
}
//-------Taste 5----------------------------------------------------------
// Anzeige CC und Banddicke

void dispz5(void)
{
  zeile2();
  //lcd_string("ospd "); lcd_number(oldspeed);
  lcd_number(trightmw);
  lcd_char(' '); lcd_number(mintime);
  lcd_char(' '); lcd_char(umflg);
  lcd_char(' '); lcd_char(calallow);
  //lcd_char(' '); lcd_number(laufzeit);
}
//------Taste 6-----------------------------------------------------------
// Ausgabe der gespeicherten Locations
void dispz6(void)
{
	zeile1();
	if (loctimer > 1500){	//dadurch weiterhin Geräte-Steuerung möglich
		if (lcnt < ilocmax){	//
			zeile2();
			lcd_string("Loc.: ");
			lcd_char(lcnt);
			if ((tmp16 = timelocn[lcnt]) >=0){	//timelocn[10]
				lcd_setcursor( 8, 2 );
				lcd_time(tmp16, 2);
				lcnt++;
				loctimer = 0;
			}
			else {
				lcd_setcursor( 3, 2 );
				lcd_string("no more locns");
				_delay_ms(2000);	//keine Steuerung möglich
				locend();
			}
		}
		else locend();
	}
}
void locend(){		//damit nächste Location-Liste bei 0 anfängt
	menunr = 99;
	lcnt = 0;
	loctimer = 2000;
}
//----------------------Taste 7-------------------------------------------
/*  Zeitlichen Location-Abstand in Sek.,
	Eingabe: Schaltuhrbetrieb.
	Sensor Fehler anzeigen.
*/
void senserr(void)
{
	menunr = 99;		//sonst wird Subroutine wiederholt aus main
  lcd_clear();
  lcd_string("Loc interval sec");
  tmp16 = vorlauf(loctime, 3);
  if ( tmp16 > 0 ){
    loctime = tmp16;
    ee_write16(11); //loctime
    locnfill();
  }

	lcd_clear();
	lcd_string("Sensor error:   ");
	zeile2();
	lcd_string("L: ");
	lcd_number(lefterr);
	lcd_string("  R: ");
	lcd_number(righterr);
	_delay_ms(2000);	//Anzeigedauer
}
//---------Taste 8-------------------------------------------------------
/* L/R Divisor ändern, d.h. Pulse pro Umdrehung,
   Kalibrierung bei Bandanfang oder nur bei jedem Einschalten
   Timer-Eingabe in Min. oder in Min. und Sek.
*/
void fdivisor(void)
{
	menunr = 99;		//sonst wird Subroutine wiederholt aus main
	lcd_clear();
	lcd_string("Pulse/U left:   ");
	tmp16 = vorlauf(ldivisor, 1);
	if ( tmp16 > 0 ){	//1 Stelle = max. 9 erlaubt
		ldivisor = tmp16;
		ee_write8(4);
	}
	lcd_clear();
	lcd_string("Pulse/U right:  ");
	tmp16 = vorlauf(rdivisor, 1);
	if ( tmp16 > 0 ){	//1 Stelle = max. 9 erlaubt
		rdivisor = tmp16;
		ee_write8(5);
	}
	if (ldivisor < rdivisor) mindiv = ldivisor;	//kleinsten Wert erhöht Delay
	else					 mindiv = rdivisor;

  lcd_clear();
  lcd_string("Cal onl at pwron");
  tmp16 = vorlauf(calpwron, 1);
  if ( tmp16 >= 0 ){
    calpwron = tmp16;
    ee_write8(7);
  }
  
  lcd_clear();
  lcd_string("Time-in with sec");
  tmp16 = vorlauf(sekunden, 1);
  if ( tmp16 >= 0 ){
    sekunden = tmp16;
    ee_write8(9);
  }
}
//----------Taste 9-------------------------------------------------------
/* Stop Vorlaufzeit eingeben (bezogen auf time0), wenn STOP ausgelöst wird,
   Stop Vorlaufzeit eingeben, wenn Location angefahren wird,
   LCD-Reset eingebaut, falls Display abstürzt
   Banddicke vorgeben, bei 0: Kalibration starten
*/
void stopvorlauf(void)
{
	menunr = 99;		//sonst wird Subroutine wiederholt aus main
  lcd.begin(16,2); //reset LCD
	lcd_clear();
#ifdef A77	//nur bei A77
	lcd_string("STOP-pretime: ");
	tmp16 = vorlauf(stpkst, 3);
	if ( tmp16 > 0 ){
		stpkst = tmp16;
		if (stpkst < 100) stpkst=100;	//min. Wert
		ee_write16(7);
	}
#endif
  lcd_clear();
  lcd_string("Location-pretime ");//für Loc und Tape-End
  tmp16 = vorlauf(lockst, 3);
  if ( tmp16 > 0 ){ //0 nicht erlaubt
    lockst = tmp16; //Wert für 19cm/s in Sekunden
    ee_write16(8);
  }
  lcd_clear();
  lcd_string("Banddicke in um "); //Banddicke eingeben
  tmp16 = vorlauf(bandd, 2);
  if ( tmp16 >= 0 ){
    bandd = tmp16;
    ee_write16(0);
	}
}
//-----------------------------------------------------------------
void locnfill()	//Locations füllen mit x Min. Abständen, nur für Tapeno. 0
{
	int16_t kst;

	if (revplay) kst = -loctime;
	else kst = loctime;

	tmp16 = 0;	//erster Wert
	for (i=0; i<ilocmax; i++){
		timelocn[i] = tmp16;
		ee_timelocn(i);	//save in EEPROM
		tmp16 += kst;	//+ x Minuten
	}
}
//-----------------------------------------------------------------
//Normale Ausgabe auf LCD, Zeile 2, Restbandanzeige und Laufzeit
void timeaus(void)
{
  	zeile2(); 
  	tmp16 = umabs/divisor;
  	if (calflg && tmp16 < ummax){
  		lcd_string("Cal ");	//Kalibration sequence
  		lcd_number(ummax - tmp16);
      lcd_string("   ");
      lcd_setcursor( 8, 2 );
      lcd_time(timeword, 2);  //2: -+h:mm:ss
  	}
  	else{
    		if (resttime >= 0)	lcd_time(resttime, 1);	//1:_h:mm
    		else				lcd_time(0, 1); //Null ausgeben
        lcd.print("    ");  //alte Zeichen löschen
    		lcd_setcursor( 0, 2 );
    		lcd_char('R');			//nachträglich R auf Pos. 0
      	lcd_setcursor( 8, 2 );
      	lcd_time(timeword, 2);	//2: -+h:mm:ss
    }      
  if (locmode){
    lcd_setcursor( 6, 2 );
    lcd_char('L');;
  }
  locend();	//Reset für nächste Location-Liste
}
//----------Taste einmalige Zeiteingabe-------------------------------------------------------
//Eingabe einer Bandposition in Zeit und dort hin spulen
void time_in(void){
  menunr = 99;    //sonst wird Subroutine wiederholt aus main
  timesearch = input_t(-1, 1);  //Zeiteingabe
  if (timesearch >= 0){   //nur bei gültiger Eingabe spulen
    timerflg = lenflg = 0;  //Sperren der Timer+playtimer Funktion
    zeile1();
    lcd_string("Wind to ");
    lcd_time(timesearch, 2);
    locnflg = 4;  //function runlocn
    if (timeword > timesearch) ir_ffrew();
    else ir_ffwd();
  }
  else locnflg = 0;
}
//----------Taste Eingabe Spieldauer-------------------------------------------------------
//Eingabe einer Spieldauer, wenn erreicht: STOP
void time_len(void){
  menunr = 99;    //sonst wird Subroutine wiederholt aus main
  lenflg = ~lenflg;  //toggle
  if (lenflg){
    timesearch = input_t(-1, 1);  //Zeiteingabe
    if (timesearch > 0){   //nur bei gültiger Eingabe
      locnflg = timerflg = 0;  //Sperren der Locater+Timer Funktion
      desttime = timesearch + timeword;
      zeile1();
      if (desttime <= resttime){
        lcd_string("Play to ");
        lcd_time(desttime, 2);
      }
      else{
        lcd_string("not enough tape");
        _delay_ms (2000);
        lenflg = 0;
      }
    }
    else lenflg = 0;  //bei ungültiger Eingabe
  } //lenflg
}
void time_chk(void){
  zeile1();
  lcd_string("Play to ");
  lcd_time(desttime, 2);
  if (timeword >= desttime){
    lenflg = ircodenr = 0;
    ir_stop(); 
  }
}
//-----------------------------------------------------------------
//liest Min und Sek ein, tno=tape no.
int input_t(int tno, uint8_t xrun)
{
  uint8_t i, max, cnt, fak=60;
	char tt[][4] = {"Min", "Sec"};
	int itime=0;
  if (sekunden) max=2;
  else          max=1;
	lcd_clear();
	for (i=0; i<max; i++){
		zeile1();
		if (tno < 0){	//einmalige Eingabe der Bandpos.
			lcd_string("Time inp in ");
			lcd_string(tt[i]);
			tmp16 = 0;	//wg. akt. Ausgabe
		}
		else {
      lcd_number(tno);
			lcd_string("/L");
			lcd_char(locs+'0');
			lcd_string(" Time ");
			lcd_string(tt[i]);
			lcd_setcursor(0,1);
			lcd_char('T');
			tmp16 = timelocn[tno];	//alten Wert anzeigen
			if (tmp16 < 0) tmp16 = 0;		//falscher gespeicherterWert
		}
		if (i==0) cnt = 3; else cnt = 2;		//Min. 3-stellig, Sek. 2-stellig
		tmp16 = vortime(cnt, xrun);		//Ausgabe akt. Zeit
		if (tmp16 == -2) return -2;
		if (i==0 && tmp16 < 0) return -1;	//Abbruch wenn keine Eingabe bei Min.
		else if (tmp16 < 0) tmp16 = 0;		//wenn keine Eingabe bei Sek.
		if (i==1 && tmp16 > 59) tmp16 = 59;	//Sek. Eingabe
		tmp16 *= fak;
		itime += tmp16;
		fak /= 60;
	}
	return itime;
}
//-----------------------------------------------------------------
//Subroutine Restzeitanzeige und Banddicken-Berechnung
//Beim Umspulen wird Rest = Rest + letztes timeword - aktuelles timeword
void restlauf(void)
{
	setrev();	//für Reverse Betrieb
	if (sync < synckst){		//Anlauf oder Umspulen, +1 macht Kal. wiederholbarer
		if ( !lasttim) lasttim = timeword;	//beim ersten Aufruf (oneshot)

		if (revplay != revcalset) resttime -= lasttim - timeword;
		else 		 resttime += lasttim - timeword;	//beim Spulen
		lasttim = timeword;			//neue Diff.
	}
	else {	//in sync = Berechnung im Playmode:
		lasttim = 0;
		//kalibrieren nach PWR on (durch calallow=1 nur beim Einschalten)
		//sonst erst wieder freischalten wenn Band weit genug gelaufen
		//mintime bei 19/38 - 26er: 1950/975, 22er: 1720/860

		if (!calpwron && rightmw > mintime+50) calallow = 1;

    //Reset immer bei Bandanfang, auch ohne Kal.
    //------------------------------------------
		if (umflg && ((!revplay && trightmw < mintime) || (revplay && tleftmw < mintime))){
			umflg = timeword = umlabs = umrabs = 0;
			revcalset = revplay;	//Richtung festlegen
			setrev();	//wg. rightmw, kann trightmw oder tleftmw sein
		}
    
		if (rightmw > mintime) umflg = 1;//damit bei rightmw<mintime nicht dauernd resetet wird

		//hier beginnt Kalibration, oneshot, Kal. beginnt nach 2U
		if (!calflg && (trightmw < mintime) && calallow && sync < synckst+1){
      if ( !bandd ) calflg = 1; //Kal. nur wenn bandd=0
      else lend = bandd;
			urold = calallow = lefterr = righterr = resttime = 0;
			//Geschw. unabhängig, Länge in um
			minrightl = (long)trightmw * oldspeed;// 366700
      len0q = (double)minrightl;	//Länge µm
			len0q /= pi *2.;	//Radius r0 = 58362.
			len0q *= len0q;	//r0² = 3406 e6
      //Initialisierung:
      cc = 1000;
      timekorr = 1000.;
		}
		//da umright sich in beide Richtung ändern kann,
		//deshalb nach Kalibration keine weitere Berechnung
		//Kalibration läuft bis Ablauf von ummax
		if (calflg){	//alles nur für rechte Spule berechnen, da Cal. nicht bei reverse
			if (umrabs != urold){	//nur bei Änderung, 1/4 U
			urold = umrabs;
				//1675*190= 318000e-3 bei 1. Windung
				//einmalige Berechnung nach Kalibration
				if (umrabs >= ummax*rdivisor) {	//nach xx U
          //calflg = loctime = 0;
          calflg = 0;
					tmpdbl = (double)rightmw * oldspeed - (double)minrightl;	//6080 µm
					tmpdbl /= pi * 2.;	//Radius = 967.66
					tmpdbl += (umrabs/rdivisor) / 2;	//aufrunden
					tmpdbl /= umrabs/rdivisor;	//32
					lend = tmpdbl;
				}
      }
		}	
		//Kalibrierung abgelaufen, ab jetzt Restzeit berechnen nur wenn in Sync
		else if (umrabs/rdivisor != urold){	//nur bei Änderung, volle U
			urold = umrabs/rdivisor;
      if (bandd) lend = bandd;
			//neue Restzeitberechnung, siehe Readme.txt:
			len0q = (double)minrightl;	//Länge µm
			len0q /= pi *2.;	//Radius r0 = 58362.
			len0q *= len0q;	//r0² = 3406 e6
			len1q = (double)(leftmw) * oldspeed;	//Länge in µm
			len1q /= pi *2.;	//Radius r2
			len1q *= len1q;		//r2²
			timekorr = (double)cc / 1000.;
			tmpdbl = pi * (len1q - len0q) / (lend*timekorr);	//Restlänge in µm
			tmpdbl /= oldspeed;
			resttime = tmpdbl / 1000.;	//ms -> Sek.
/*		Nach reset werden alle 10 Loc. mit mit Bandlaufzeit /10 gefüllt	
       if (!loctime){		//=0, noch nicht gefällt
				loctime = resttime / 600;	//in Min., dann durch 10 für 10 Speicherstellen
				loctime *= 60;				//ganze Min. in Sek.
				ee_write16(11);	//loctime
				locnfill();
			}
*/
		}
	}	//sync
}
//-----------------------------------------------------------------
/*	Subroutine zur Längenberechnung aus den Umdrehungen
	Länge = pi*u * (d0 + bandd*u) oder Gesamtlänge = u * (lenr0 + u*lend*pi)
	Achtung beim Umspulen gibt es bis zu 40ms/U, kürzer als der Programmlauf von ca. 100ms
	timeword wird durch INT hochgezählt, Genauigkeit ist vom Quarz abhängig,
	abstime wird durch die Umdrehungen errechnet
*/
void lenrout(void)
{
	//nur rechnen wenn Änderung von umabs, auch in Kal-Modus
	//bei jedem Puls, sonst dauert Anzeige zu lang
	if (umabs != u16l){
		setrev();	//für Reverse Betrieb
		u16l = umabs;	//=u: kann >2000 werden
		timekorr = (double)cc / 1000.;
		tmpdbl = (double)lend*pi*timekorr;
		tmpdbl *= u16l;	//Gesamtlänge in µm
		tmpdbl /= divisor;
		tmpdbl += (double)minrightl;
		tmpdbl *= u16l;
		tmpdbl /= divisor;
		lenword = tmpdbl / 1000.;	//Gesamtlänge seit Anfang in mm

		//nur für Geschw. Berechnung:
		//lendiff ist Länge einer U, beim Umspulen kann mehr als 1U abgelaufen sein
		lendiff = fabs(lastlen - tmpdbl);	//=lenword e-3
		lendiff *= divisor;
		lendiff += fabs(u16l-lastu16)/2.;	//Aufrunden
		lendiff /= fabs(u16l-lastu16);	//wg. Umspulen, diff für 1 U
		lastu16 = u16l;
		lastlen = tmpdbl;

		//Errechnete Spielzeit
		abstime = (lenword+(double)oldspeed/2.) / oldspeed;

		//synchronisieren wenn nicht in play:
    if ( laufwcode != 99 )timeword = abstime; //not playmode
    //if ( laufwcode != cplay )timeword = abstime; //not playmode
		//nur bei Änderung, Berechnn. alle 1 U, max. 2000
		else if (umrabs/divisor != ukold){
			ukold = umrabs/divisor;
			if (abstime+1 < timeword){
			  cc++;
        if (cc > 1100) cc= 1100;  //max. Wert
			}
			else if (abstime-1 > timeword){
			  cc--;
        if (cc < 900) cc=900;   //min. Wert
			}
		}
	}
}
//-----------------linke Spule------------------------------------
void links()
{
	runflg++;	//muss ausserhalb bleiben, sonst wird tleft0 nicht mehr upgedated
/*  
  //Berechnung beginnt nach Pulsdauer, muss min. 1-2ms lang sein (tlkurz >= 1)
  if (!lfalling){   //INT0 bei steigender Flanke: Pulsende
    lfalling=1;
    attachInterrupt(digitalPinToInterrupt(2), links, FALLING);  //setze fallende Flanke
    //tlkurz ist die Totzeit in ms zw. 2 Pulsen wg. prellen
    if ((tlkurz >= 1) || (tlkurz == 0 && laufwcode != 99)){ //0ms wg. Umspulen
      if (tleftx > ldivisor) tleftx = 1;  //Index Überlauf
      tleft[tleftx++] = tleft[0];
      tleft[0] = 0;
      if (reverse || (revplay && laufwcode == 99)){
        umlabs++;
      }
      else{
        umlabs--;
      }
    }
  }
  else {  //INT bei fallender Flanke
    //EICRA |= 1<<ISC00;  //setze steigende Flanke
    lfalling = 0;
    attachInterrupt(digitalPinToInterrupt(2), links, RISING);  //setze steigende Flanke
    tlkurz = 0;
  }
*/
	//Berechnung beginnt nach Pulsdauer, muss min. 1-2ms lang sein (tlkurz >= 1)
  if (EICRA & 1<<ISC00){    //INT bei steigender Flanke: Pulsende
    EICRA &= ~(1<<ISC00); //setze fallende Flanke
    //tlkurz ist die Totzeit in ms zw. 2 Pulsen wg. prellen
    if ((tlkurz >= 1) || (tlkurz == 0 && laufwcode != 99)){ //0ms wg. Umspulen
      if (tleftx > ldivisor) tleftx = 1;  //Index �berlauf
      tleft[tleftx++] = tleft[0];
      tleft[0] = 0;
      if (reverse || (revplay && laufwcode == 99)){
        umlabs++;
      }
      else{
        umlabs--;
      }
    }
  }
  else {  //INT bei fallender Flanke
    EICRA |= 1<<ISC00;  //setze steigende Flanke
    tlkurz = 0;
  }
}
//-----------------rechte Spule------------------------------------
void rechts()
{
  runflg++;	//muss ausserhalb bleiben, sonst deadlock
/*
  if (!rfalling){   //INT1 bei steigender Flanke: Pulsende
    rfalling=1;
    attachInterrupt(digitalPinToInterrupt(3), rechts, FALLING); //setze fallende Flanke
    if ((trkurz >= 1) || (trkurz == 0 && laufwcode != 99)){ //0ms wg. Umspulen
      if (trightx > rdivisor) trightx = 1;  //Index Überlauf
      tright[trightx++] = tright[0];
      tright[0] = 0;
      if (reverse || (revplay && laufwcode == 99)){
        umrabs--;
      }
      else{
        umrabs++;
      }   
    }
  }
  else {  //INT bei fallender Flanke
    //EICRA |= 1<<ISC10;  //setze steigende Flanke
    rfalling = 0;
    attachInterrupt(digitalPinToInterrupt(3), rechts, RISING); //setze steigende Flanke
    trkurz = 0;
  }
*/ 
  if (EICRA & 1<<ISC10){    //INT bei steigender Flanke
    EICRA &= ~(1<<ISC10); //setze fallende Flanke
    if ((trkurz >= 1) || (trkurz == 0 && laufwcode != 99)){ //0ms wg. Umspulen
      if (trightx > rdivisor) trightx = 1;  //Index �berlauf
      tright[trightx++] = tright[0];
      tright[0] = 0;
      if (reverse || (revplay && laufwcode == 99)){
        umrabs--;
      }
      else{
        umrabs++;
      }
    }
  }
  else {  //INT bei fallender Flanke
    EICRA |= 1<<ISC10;  //setze steigende Flanke
		trkurz = 0;
	}
}
//------------------------------------------------------------------------
// Timer2, wird alle 1ms durchlaufen und Timer gesetzt
//------------------------------------------------------------------------
ISR(TIMER2_COMPA_vect)
{
	//wenn Versorgungsspannung abfällt: pwrpin = low
	//wird aber erst aktiv, wenn Puls länger als 2 ms ist, um Fehlpuls zu unterdrücken
  //if (!digitalRead(pwrpin) ){  //0 = power down
  if (analogRead(pwrpin) < 700){  //3,5V = power down
  //if ( !PIND & (1<<pwrpin) ){  //0 = power down, schneller als dig/ana read
  		pcnt++;
  		if (pcnt > 2) {
  			ee_writetim();	//Zählwerkstand sichern
  			relstop();		//immer STOP auslösen ohne Logik
  			lcd_clear();
  			lcd_string("PWR down detectd");
  		  while(1){};		//Endlos-Loop
  		}
	}
	else pcnt = 0;

	alltimer++;		//allgemeiner Timer
	irtimer++;		//Delay bei IR-Einagbe, wird resettet in ir_learn
	ltimer++;		//Delay bei Location-Einagbe, wird resettet in auswert
	if (relaisbusy) reltimer++;		//Abschalttimer aller Relais
	if (lcnt) loctimer++;

	if ( !stopflg)	//=0, wenn Band nicht steht
	{
		timems++;	//Diff. zw. den errechneten Zeiten timeword
		if ( timems > 1000 ){
			if (revplay != revcalset) timeword--;
			else timeword++;      // alle 1000ms -> +1 sec
			timems = 0;
		}
		tleft[0]++;
		tright[0]++;
		tlkurz++;	//darf nicht auf 0 gesetzt werden
		trkurz++;

		if (stoptime < 32000) stoptime++;	//ms, keinen Überlauf zulassen
		disptime++;
		senstmr++;
	}
	else if (starttime < 32000) starttime++;	//wenn Stopflg
}
