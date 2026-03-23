/*
 EZS UNO Version 1.2
 Mai 2020: Wie EZS_Float_5.2
 Vers. 1.3: Erst Abfrage FREW, dann (else) FFWD
 In Menü 9 LCD-Reset eingebaut, da LCD manchmal spinnt
 In main.h t2626 auf 1400 gesetzt, da bei 22er Spulen manchmal 26 angezeigt wurde.
 In ir_play delay von 20ms entfernt, da bei Schaltuhrfunktion manchmal REC und sofort FREW aktiv wurde
 Damit nicht bei REC am Gerät das FREW Signal erkannt wird, rewpin auf Revoxstecker Pin 9 (FREW) legen, statt auf REC -> Pin 5 
 Vers. 1.4: Zusätzliche Taste für Abspiellänge T-LEN.
 Bei Zielzeit-Eingabe und Abspiellänge (playtime): Sek.-Eingabe entfernt
 Das neue Board von Richi Seiler zusätzlich eingefügt, in main.h definiert
 Vers. 1.5: PWR down Routine verbessert, Sensor-Error-Detection verschärft,
 Neue Funktion auf extra Taste: Playtime. Eingabe einer vorgegebenen Abspielzeit. Kann in main.h freigeschaltet werden. 
 Anzeigen verbessert bei playtime und Wind to..., das Flimmern aller Anzeigen reduziert
 In Menü 8 Timer Eingabe wählen in Min. und Sek. oder nur in Min.,
 In Menü 9 Banddicken-Vorgabe möglich. Nur wenn 0: Kalibration,

*/
#include "main.h"

LiquidCrystal_PCF8574 lcd(0x27);

#if A77
	char verstxt[]	= "   Revox A77    ";
#elif B77
	char verstxt[]	= "   Revox B77    ";
#endif
  char verstxt2[]   = "EZS UNO Vers 1.5";
char savetxt[]  = "   cm/s";
char stattxt[]  =	"            cm/s";
char schaltuhr[] =	"Record at pwr on";

uint8_t enablel, enabler, menunr, stopflg, irstopflg, laufwcode,
		adresse, command, taste2on, ircodenr, allow, mindiv, lefterr, righterr, nearflg,
		minuten, locnflg, lenflg, irlocnr, locmode, sync, relaisbusy, endtape, endtdsp, calflg, playspeed;
volatile uint8_t  runflg, reverse, tleftx, trightx, revflg;
//tmpm16 wird in INT -Routine genutzt, deshalb darf dort nicht tmp16 genutzt werden
int16_t	tmp16, lasttim, timesearch, desttime, timeword, abstime, lockstx,
  tleftold, oldt0, midtime, oldleftmw, oldrightmw, oldright;
volatile int16_t tleft[8], tright[8], stoptime, disptime, starttime,
		 maxtime, mintime, dlytime, irtimer, ltimer, reltimer, senstmr, tmpm16, time0, tleftmw, trightmw;
int32_t	tmp32, alltimer, laufzeit;

int8_t  buff8[10];
int16_t buff16[13];
int32_t buff32;
int16_t  timelocn[ilocmax];
uint8_t taste0[ziffnr+lwnr+1];	//Speicher fuer IR-, LW-, Divisor- und Nummern-Codes

//Lokale Variable:
uint8_t xblink, spdchange = 0, initflg = 0, z2blank;
int16_t oldlabs, oldrabs, idelay=0;

//--------------------------------------------------------------------------------------------
//Timer 1 output compare A interrupt service routine, called every 1/15000 sec
void timer1interrupt()  //alle 66usec, INT wird gesperrt
{
#define rtime 5033/66  //5 msec
	idelay++;
	if (idelay > rtime){	//alle 5 msec
		idelay = 0;	//Reihenfolge wichtig: erst delay=0, dann runmode
		runmode();
	}
	else irmp_ISR();	// call irmp ISR
}
//-----------------------Main--------------------------------------
//int main(void)
void setup(){
  pinMode(lsensor, INPUT_PULLUP);   //INT0, linker Sensor
  pinMode(rsensor, INPUT_PULLUP);   //INT1, rechter Sensor
  pinMode(rewpin, INPUT_PULLUP);
  pinMode(fwdpin, INPUT_PULLUP);
  pinMode(taste2, INPUT_PULLUP);  //Lern-Taste
  pinMode(ffwd, OUTPUT);
  pinMode(frew,  OUTPUT);   //A77: REC+FREW
  pinMode(rec,  OUTPUT);
  pinMode(play, OUTPUT);
  pinMode(tstop, OUTPUT);   //START/STOP
  pinMode(pwrpin, INPUT_PULLUP);  //PWR-Down-Input
  relstop();   //Stoprelais ein, B77: Rel. aus
#ifdef seiler    //für das neue Board von Richi Seiler
  pinMode(rectaste,  INPUT_PULLUP);
  pinMode(stoptaste, INPUT_PULLUP);
  pinMode(playtaste, INPUT_PULLUP);
  pinMode(ffwdtaste, INPUT_PULLUP);
  pinMode(frewtaste, INPUT_PULLUP);
#endif
//-----------------------------------------------------------------
  Wire.begin();
  Wire.beginTransmission(0x27);
  lcd.begin(16,2);
  lcd.setBacklight(255);  //hat keinen Einfluss
//--------------------------------------------------------------------
//---------------------------------------------------------------------
//TIMER2:
//TIMER0 wird vom Arduino genutzt, deshalb hier TIMER2 nutzen
//Interrupt-Routine fuer Timer2 (1/16MHz)*Teiler bei compare match A, 1ms: time0k=+250
  TCCR2A = (1<<WGM21);  //CTC mode (clear timer on compare match)
  TCCR2B = (1<<CS22); //Teiler 64 -> 4usec
  OCR2A  = 250;   //timer2 Konstante (250=1ms)
  TIMSK2 = (1<<OCIE2A); //Interrupt bei Timer Overflow

//Interrupt-Routine fuer 16 Bit-Timer1 fuer IRMP
  Timer1.initialize(1000000 / F_INTERRUPTS);
  Timer1.attachInterrupt(timer1interrupt);
  attachInterrupt(digitalPinToInterrupt(2), links, FALLING);
  attachInterrupt(digitalPinToInterrupt(3), rechts, FALLING);
//-----------------------------------------------------------------
  ee_read();  //liest aus EEPROM alle Daten
  
#ifdef A77
		revplay = revcalset = 0;
#elif B77
		stpkst = 0;
		revplay = revcalset = 0;
#endif
	relstop();		//STOP-Relais aktiv
//--------------------------------------------------------------------
	lcd.print(verstxt);  //Revox A77
	zeile2();
	lcd.print(verstxt2); //EZS UNO Vers 5.2

	runflg = enablel = enabler = laufwcode = reverse = revflg = 0; stopflg = 255;
	ircodenr = irstopflg = disptime = starttime = tleft[0] = tright[0] = reltimer = spdchange = 0;
	lefterr = righterr = calflg = nearflg = alltimer = lenflg = 0;
	endtape = 1;	//Endtape gearmed
	locmode = 0;//255;  //Locationmode
	menunr = 99;
//-----------------------------------------------------------------
	//Init Werte wenn EEPROM leer bzw. nach Programmierung
	midtime = tmid26;
  if      (ldivisor < 1 || ldivisor > 9){ ldivisor = rdivisor = 4; initflg =1;}
  else if (rdivisor < 1 || rdivisor > 9){ rdivisor = ldivisor = 4; initflg =1;}
	if (lend < 20 || lend > 60){ lend = 35; initflg =1;}
  if (initflg){
    initflg = 0;
	#ifdef A77
		stpkst = 220;
	#else
		stpkst = 0;
	#endif
		lockst = 40;
		tapeno = timerflg = timeword = calflg = 0;
		oldspeed = 190;
		spule = 26;
		cc = 1000;
    if (loctime > 3600 || loctime < 1) loctime = 180;  //wenn falsch dann 3 Min.
    if (calpwron > 1  || calpwron < 0) calpwron = 0;   //wenn falscher Wert
	}
//-----------------------------------------------------------------
  if (ldivisor < rdivisor) mindiv = ldivisor; //kleinsten Wert erhoeht Delay
  else       mindiv = rdivisor;
  dlytime  = 8300/mindiv;   //Delay fuer 9.5cm/sec bis STOP erkannt oder Disp. Update erfolgt
  stoptime = 9900/mindiv;   //nach Reset sofort STOP
  #ifdef uher
    dlytime *= 2; //wg. 4.75cm/s
    stoptime *= 2;  //wg. 4.75cm/s
  #endif
		
  if (oldspeed > 100){
    tmp16=oldspeed/10;  //Anzeige der letzten bekannten Playspeed
    savetxt[0]  = ' ';
    savetxt[1]  = '0' + tmp16/10; //z.B. 38/10
    savetxt[2]  = '0' + tmp16%10;
  }
  else {
    savetxt[0]  = '0' + oldspeed/10; //z.B. 95/10
    savetxt[1]  = '.';
    savetxt[2]  = '0' + oldspeed%10;
  }
  stattxt[9]  = savetxt[0];
  stattxt[10] = savetxt[1];
  stattxt[11] = savetxt[2];
  stattxt[6] = '0' + spule/10;  //Spulengroesse
  stattxt[7] = '0' + spule%10;

	tleftx = 1;	//Init
	trightx = 1;
	allow = 1;	//allow ext. rewpin

	irmp_init();

	_delay_ms(2000);	//warten bis Motor und Spannung stabil, sonst PWR down Detection
  
  if (timerflg){
    _delay_ms(3000); //warten bis Motor hochgelaufen
    timerflg = 1;
  }
	relaisbusy = z2blank = 1;
  sei();
}
//============================================================================================
/*-----------------main startloop------------------------------------------------------------
Prueft zuerst, ob Lerntatste gedrueckt, dann ob relaisbusy.
Wenn Relais nicht aktiv, testen ob STOP oder Play aktiviert werden sollen.
Wenn nicht relaisbusy und reltimer abgelaufen, dann alle Relais zuruecksetzen.
Danach letzte IR-Eingabe im Loop durchfuehren.
*/
void loop()
{
	xblink++;
	#if teac
		if (revplay) stattxt[4] = '<';
		else		 stattxt[4] = ' ';
	#endif

  //ob und wie oft Lerntaste gedrueckt ?
  if (sw_auswert() )  //wenn Learnswitch = 1 o. 2
  { 
		ir_learn();
		zeile1();
		lcd_string(stattxt);	//Lerntext ueberschreiben
	}

	if (!relaisbusy){	//=0=kein Relais aktiv
		if (irstopflg) ir_stop();	//STOP mehrfach pruefen, da evtl. PLAY wartet
		else if (laufwcode == cplay  || laufwcode == crec ) ir_play(0);
   
  /*Die Pins werden gesetzt, egal ob von der EZS oder am Gerät bedient.
	Rewind Signal vom Bandgeraet, aber nur wenn FREW noch nicht aktiv, sonst dead loop.
  Das FREW-Relais ist gleichzeitig REC-Relais.*/
		#ifdef uher
      if ( allow && !(digitalRead(rewpin)) ) ir_ffrew(); //high active
		#elif teac
			if (laufwcode == cplayr || laufwcode == crecr) ir_play(1);	//nur bei reverse
		#else
      if      ( allow && !digitalRead(rewpin) ) ir_ffrew();  //low active, setzt revflg
      else if (          !digitalRead(fwdpin) ) ir_ffwd();   //low active, loescht revflg
    #endif
  //Zusätzliche Laufwerkstasten am Board für Version Richi Seiler
  #ifdef seiler
    if     ( !digitalRead(rectaste) )  {ir_play(0); locnflg = 0; ircodenr = crec;}
    else if( !digitalRead(stoptaste) ) ir_stop();
    else if( !digitalRead(playtaste) ) {ir_play(0); locnflg = 0; ircodenr = cplay;}
    else if( !digitalRead(ffwdtaste) ) ir_ffwd();
    else if( !digitalRead(frewtaste) ) ir_ffrew();
  #endif
	}
//----------------------------------------------------------------------------------------
	else if (reltimer > 200){		//alle Relais stromlos bis auf Start/Stop-Relais
		relaisbusy = reltimer = 0;
		relnffwd();
		relnfrew();
		#ifndef A77
			relnpause();	//Pause fuer B77, reverse für TEAC
		#endif
		relnplay();
		relnrec();
		relstart();		//Stoprelais ein, B77: Rel. aus
	}
//----------------------------------------------------------------------------------------
	//poll IR Input und setze Relais oder Menu-Flags
	//falls keine Eingabe bleibt die letzte Flag aktiv
	ir_auswert();		//IR-Eingabe ist erfolgt in menunr
//----------------------------------------------------------------------------------------
  if (timerflg){
    locnflg = lenflg = 0;  //Sperren der Locater+playtime Funktion
    lcd_string(schaltuhr);
    if (timerflg == 1){
  		timerrout();  //Sprung zur Schaltuhr-Routine, nur beim 1. Mal
      timerflg = 2;  //dadurch wird timerrout nur einmal durchlaufen
    }
	}
  else if (lenflg) time_chk();  //bei playtime
//----------------------------------------------------------------------------------------
	if (!calflg) {
		switch (locnflg){ //0, 1...4
			case 1: {
				if (ltimer > 1000){ //wenn nach 1 sec nicht wieder gedrueckt
					locnflg = 0;
					menunr = 99;	//ersten readlocn ungueltig setzen
					locmode = ~locmode;	//num. Tasten fuer Locations oder Menu
				}
				break;
			}
			case 2: {
				savelocn(timeword);
				_delay_ms(1000);
				break;
			}
      case 4: {
        runlocn();
        break;
      }
		}
	}	//calflg

	if (locmode && menunr != 99) {
		readlocn();
		timeaus();	//Zeitausgabe
	}
	else  {
    if (z2blank){ //nur beim 1. Mal blank
      lcd_setcursor( 0, 2 );
      lcd_string("                ");
    }
		//Testen auf Menue-Code, Anzeige teilweise auf beiden Zeilen
		switch (menunr)
		{
			case 1:{
				dispspd();	//display speed
				break;
			}
			case 2:{
				disptim();	//Umdrehungszeit der linken und rechten Spule,
				break;		//mit Mittelwert time0
			}
			case 3:{
				dispz3();	//absolute time data
				break;
			}
			case 4:{
				dispz4();	//umlabs/right, dlytime
				break;
			}
			case 5:{
				dispz5();
				break;
			}
			case 6:{
				dispz6();	//alle Locations anzeigen
				break;
			}
			case 7: {		//aenderung der Bandnummer,
				senserr();	//und Location-Abstand in Sek.,
				break;		//Sensor Fehler anzeigen
			}
			case 8:{		//L/R Divisor aedern
				fdivisor();	//Kalibrierung bei jedem Einschalten
				break;		
			}
			case 9:{
				stopvorlauf();	//Vorlaufkonstante fuer STOP und 
				break;			//Vorlauf bei Location anfahren
			}
			default:
        timeaus(); //Zeitausgabe
		}	//switch
	}
  z2blank=0;

	//oldmenu = menunr;		//vorherige Menuenr.
  restlauf(); //Kalibration, Bandlaengen- und Restzeitberechnung
  lenrout();  //Laengen- und Zeitberechnung

	//2018, kein blinken wenn menu angezeigt
	if (sync <= synckst && (xblink & 4) && (menunr > 10 || menunr == 0)){ 
		lcd_setcursor( 10, 2 );	//10.Stelle ist ':'
		lcd_char(' ');	//2018
	}
//------------------------------------------------------------------
	zeile1();
  if (!timerflg && !locnflg && !lenflg && menunr != 3) lcd_string(stattxt);
  
  laufzeit = alltimer;
  alltimer = 0;
}	//Ende mainstart-loop

//========Interrupt(ISR)-Routine====================================================================
//-----------------------STOP-Detection-Routine------------------------------------------------------
//Routine prueft nur an Hand der Spulendrehung, ob Band steht, fuehrt keinen STOP aus.
//mit Verzoegerung von einer vollen Umdrehung, da Spulen langsam drehen 
//und runflg zwischendurch 0 wird.
//Wenn STOP aus playmode: kleines Stopdelay wenn ir_stop, ir_ffwd oder ir_frew
///--------------------------------------------------------------------------------------------
void runmode(void)	//Laufzeit zw. 16 (stop) und 208usec (play in sync)
{  
  sei();  //freigeben fuer Sensor INTs

  if (!runflg ){	//=0, wird in subroutine gesetzt so bald die Spulen sich bewegen
		//am Beginn: stoptime=9900/mindiv, delaytime=8300/mindiv
		if (stoptime > dlytime)	//dlytime wird bei FFWD/FREW/Play an Drehgeschw. angepasst
		{
		#if B77
			if (laufwcode == cplayr)	//Pausetaste
				strncpy(stattxt,"PAUSE",5);
			else
		#endif
    	for (tmpm16=1; tmpm16 <= ldivisor; tmpm16++){
				tleft[tmpm16]=0;
				tright[tmpm16]=0;
			}
			playspeed = disptime = sync = calflg = nearflg = 0;
			oldlabs = umlabs;
			oldrabs = umrabs;
			reverse = revflg;	//aus frew oder ffwd wenn stop erreicht
			stopflg = 255;		//echter STOP, kein Update in Timer0_OVF
			if (starttime > 1000){//falls Umspulwechsel und nach 1 sec STOP:
        strncpy(stattxt,"STOP",4);
				//loescht Flaggs falls am Geraet bedient,
				//mit stoptime=0 wird Stoproutine nur noch alle dlytime durchlaufen
				reverse = stoptime = locnflg = senstmr = 0;	// = revflg
				allow = 1;
        dlytime = 8800 / mindiv;  //falls Play vom Geraet bedient
			}
		}
		return;	//keine Pruefung so lange Band steht
	}	//runflg
//--------------------------------------------------------------------------------------------
//ab hier kein STOP mehr, Berechnung der Zeit des Umlaufs der Spulen
	stopflg = stoptime = starttime = runflg = 0;
//--------------------------------------------------------------------------------------------
  tleftmw = trightmw = 0;
	for (tmpm16=0; tmpm16 < ldivisor; tmpm16++){
		if (tleftx > ldivisor) tleftx = 1;	//Index Ueberlauf
		tleftmw  += tleft[tleftx++];
	}
	for (tmpm16=0; tmpm16 < rdivisor; tmpm16++){
		if (trightx > rdivisor) trightx = 1;	//Index Ueberlauf
		trightmw += tright[trightx++];
	}
	time0 = (tleftmw+trightmw) / 2;	// /2-> da doppelter Wert
 
  //Versuch lockst zu berechnen aus der aktuellen Umspulgeschwindigkeit nach 1 vollen U einer Spule
  if ( abs(umlabs - oldlabs) > mindiv || abs(umrabs - oldrabs) > mindiv && ircodenr != cstop){  //ändern so lange kein stop aktiviert
    if (time0 > stpkst) lockstx = 8;
    else lockstx = lockst;
  }
/*
  lockstx = lockst;
*/
  
	//Testen ob ploetzliches Umspulen nach Play, z.B. durch Direktbedienung am Geraet,
	if (sync && trightmw < oldright -5){ 	//soll Reset in Kal.Routine verhindern
		sync = disptime = 0;
	}
  oldright = trightmw;
//--------------------------------------------------------------------------------------------
/*	Diese Routine displayed Text abhaengig vom Status ffwd, frew und play.
	dlytime (400/mindiv) wird zuerst in ir_play, ir_ffwd oder ir_frew gesetzt
	Falls ohne FB aktiviert und beim Stoppen, da time0 noch klein ist, zuerst Status nach 
	FB-Code pruefen, ist schneller als ueber time0.
	time0 dient zum Status pruefen fuer: ffwd, frew und play.
	So lange dlytime nicht erreicht wird, wird play/ffwd/frew geprueft und dann direkt zu mainled
*/
  if (disptime < dlytime){
		#ifdef teac
    if (laufwcode == 99 || laufwcode == cplayr){ //Play oder Rev.Play
		#else
    if (laufwcode == 99){  //Play
		#endif
			reverse = 0;	//fuer Sensortest
			strncpy(stattxt,"PLAY ",5);	//Play sofort setzen
			endtape = 1;	//wieder armen bei play
	  }
    else if (time0 >= playtime){  //playtime=800;
      laufwcode = 99;    //falls vom Geraet  bedient
		}
		strncpy(stattxt+9,savetxt,7); //Geschw. Anzeige
		return;
	}
	else disptime = dlytime;	//disptime laeuft weiter, aber keine weitere Pruefung mehr
//--------------------------------------------------------------------------------------------
	//Beim Wechsel von FFWD/FREW nach Play kann playtime zu klein sein, deshalb Abfrage nach play
  if (laufwcode == cffwd || laufwcode == cfrew || (time0 < playtime && laufwcode != cplay)){
		calflg = sync = 0;	//2019 sync dazu
		dlytime = 400/mindiv;		//setzt delytime fuer evtl. spaeteres play
		if (reverse){
			strncpy(stattxt,"FREW ",5);
			laufwcode = cfrew;	//falls am Geraet bedient
		}
		else {
			strncpy(stattxt,"FFWD ",5);
			laufwcode = cffwd;	//falls am Geraet bedient
		}
	}
//--Bandende-----------------------------------------------------------------------------------
	//Testen ob Bandende erreicht (nur beim Umspulen, da sonst Abschaltung bei Kal.):
	//wird nur durch Reset o. Play wieder aktiv, damit Band nach Abschalten weiter laufen kann
	//Tape-End-Abschaltung immer (Jan. 2019)
	if (!calflg && endtape && !lefterr && !righterr){ //Bandende fuer FREW und FFWD
		tmpm16 = stopkst();	//Stopzeit holen abhaengig von playspeed
		if (revcalset && ((laufwcode==cfrew && resttime < tmpm16*2)||(laufwcode==cffwd && abstime < tmpm16))){
			ir_stop();
			strcpy(savetxt,stattxt+9);	//falls nicht 19cm/s
			strncpy(stattxt+9,"TapeEnd",7);
			endtape = locnflg = 0;	//keine Wiederholung nach einem Tapend
			return;
		}
		else if (!revcalset && ((laufwcode==cffwd && resttime < tmpm16*2)||(laufwcode==cfrew && abstime < tmpm16))){
			ir_stop();
			strcpy(savetxt,stattxt+9);
			strncpy(stattxt+9,"TapeEnd",7);
			endtape = locnflg = 0;	//keine Wiederholung nach einem Tapend
			return;
		}
	}
//----------------------------------------------------------------------------------
//ab hier Status Play
//Ermitteln der Geschwindigkeit
  if (laufwcode != 99 ) return; //keine Pruefung, da kein playmode
	if		  (time0 > 7000)	playspeed = 4;
	else if (time0 > 3500)	playspeed = 9;
	else if (time0 > 1700)	playspeed = 19;
	else					          playspeed = 38;

	if (playspeed != oldspeed/10 ){	//Geschw. Aenderung entdeckt (Achtung: 95 statt 90)
		sync = 0;
		spdchange = 1;
		stattxt[5] = ' ';	//Sensor-Fehler loeschen
		stattxt[8] = ' ';
		lefterr = righterr = 0;
    if      (playspeed == 9) oldspeed = 95;
    else if (playspeed == 4) oldspeed = 48;
    else  oldspeed = playspeed * 10;  //nur fuer 19 und 38
	}
	else if (spdchange && sync > synckst){	//nicht >=, da zu frueh
    //wenn playspeed ok, kein timeword update, deshalb hier wenn spdchange und in sync
		spdchange = 0;
    //lenrout();  //fuer neues abstime
    timeword = abstime;
	}
	//im Playmode neues Delaytime fuer STOP-Erkennung
	dlytime = 76000 / playspeed / mindiv;	//1sec bei 19cm/s wenn mindiv=4

	//Test ob grosse Abweichung bei timeleft/right, z.B. wenn kein Puls vorhanden
	//nicht geeignet bei Pulsprellung, wird in INT0/1_vect getestet
	//sync und Reset in STOP kann nicht genutzt werden, da unzuverlaessig wenn Schwankungen
	if (sync > synckst+1){
		if (oldleftmw  < tleftmw-10   || oldleftmw  > tleftmw+10){
			lefterr++;
		}
		if (oldrightmw < trightmw-10 || oldrightmw > trightmw+10){
			righterr++;
		}
	}	

	oldleftmw  = tleftmw;		//update counter
	oldrightmw = trightmw;
	if (lefterr)  stattxt[5] = '^'; else stattxt[5] = ' ';
	if (righterr) stattxt[8] = '^'; else stattxt[8] = ' ';

	//eine volle Umdrehung links und rechts abwarten, old wird auch in STOP-Routine gesetzt
  if ( abs(umlabs - oldlabs) > mindiv && abs(umrabs - oldrabs) > mindiv ){
		sync++; 
		oldlabs = umlabs;
		oldrabs = umrabs;
	}
	else	return;
//----------------------------------------------------------------------------------

//Ermitteln der Spulengroesse:
	tmpm16 = 38 / playspeed;	//1, 2, 4, 9
	if (tmpm16 > 8) tmpm16 = 8;	//fuer 4,75cm/s

	if ( time0 > tmpm16 * t2626 ) {	//1400 bei 38cm/s
		stattxt[6] = '2';
		stattxt[7] = '6';
		midtime = tmid26;
		mintime = tmin26;
		maxtime = tmax26;
		spule = 26;
	}
	else if (time0 > tmpm16 * t2222){	//1150
		stattxt[6] = '2';
		stattxt[7] = '2';
		midtime = tmid22;
		mintime = tmin22;
		maxtime = tmax22;
		spule = 22;
	}
	else {//(time0 > tmpm16 * t1818){	//900
		stattxt[6] = '1';
		stattxt[7] = '8';
		midtime = tmid18;
		mintime = tmin18;
		maxtime = tmax18;
		spule = 18;
	}

	if (playspeed == 4){
		stattxt[9] = '4';
		stattxt[10] = '.';
		stattxt[11] = '7';
		maxtime *= 4;
		mintime *= 4;
	}
	else if (playspeed == 9){
		stattxt[9] = '9';
		stattxt[10] = '.';
		stattxt[11] = '5';
		maxtime *= 2;
		mintime *= 2;
	}
	else if (playspeed == 38){
		stattxt[9] = ' ';
		stattxt[10] = '3';
		stattxt[11] = '8';
		maxtime /= 2;
		mintime /= 2;
	}
	else {
		stattxt[9] = ' ';
		stattxt[10] = '1';
		stattxt[11] = '9';

	}
	strncpy(savetxt,stattxt+9,7);	//neuer Wert falls Geschw.aenderung
//-----------------------------------------------------------------
}	//Ende Loop
