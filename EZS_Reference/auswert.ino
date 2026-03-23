#include "main.h"
#include "irmp.h"

uint8_t del;

//-------------------------------------------------- 
//Subroutine pollt für neuen taste2-Input
//Taste 2 kann mehrfach gedrückt werden, nach jedem Druck 1 sec Zeit,
//Anzahl der Drücke in taste2on
//Annahme dass Prellen nach 16ms beendet
uint8_t sw_auswert(void)
{
	taste2on = 0;
  if( digitalRead(taste2))  //Taste nicht gedrückt = hi
		return	0;
//-----------------------------Loop--------------------------------------------
	//Taste gedrückt
	while(1)
	{		
		_delay_ms(100);	//Prellzeit abwarten
		taste2on++;		//zählen
		del=0;
    while( !(digitalRead(taste2))){}  //warten bis Taste losgelassen
    while(   digitalRead(taste2)) //warten bis Taste wieder gedrückt=hi
		{
			del++;
			_delay_ms(100);
			if (del > 10) return taste2on;		//Timeout nach 1.0 sec 
		}
	}
//-----------------------End---Loop--------------------------------------------
	return 0;
}	

//--------------------------------------------------------------------- 
//Subroutine pollt für neuen IR-Input.
//Vergleiche neue Eingabe mit gespeicherten Daten und setzt die LW-Relais
//Code wird in interne ircodenr umgewandelt und zurückgegeben
uint8_t ir_auswert(void)
{
	if ( (command = ir_get(100)) != 255 ){
		ircodenr = 0;

		//Loop vergleicht gespeicherten mit empfangenem Code
		while (taste0[ircodenr] != command)
		{
			ircodenr++;
			if (ircodenr > (lwnr+ziffnr) ) return 0;
		}
	}
	else	return 0;

//Auswertung des Commands:
//------------------------
//Code 0 bis 9 nur Zahlen, wird in menunr gespeichert,
//Code 10=timein, ab 11 Laufwerkstasten, Reihenfoge muss wie Codes in main.h sein
	if (ircodenr < ctimein){	//Menu von Code 0 bis 9, 10=creset
		menunr = z2blank = ircodenr;  //blank einschalten
    timerflg = 0; //beliebige num. Taste schaltet Timer ab
	}
	else {
		switch	(ircodenr){
			case ctimein:{	//einmalige Zeiteingabe
				time_in();
				ircodenr = 0;	//löscht Loc.-Suche wenn von IR
				break;
			}
			case ctimer:{		//Schaltuhr aktivieren
        if (!timerflg) timerflg = 2;  //wenn noch nicht aktiviert: ein aber ohne play
        else timerflg = 0;            //falls schon aktiv: löschen  				
				break;
			}
      case ctimlen:{  // Zeit für Abspiellänge
        time_len();
        break;
      }
		}
		if (!relaisbusy){
			switch	(ircodenr){
				case clocn:{
					locnflg++;
					timerflg = lenflg = 0;	//Sperren von Schaltuhrbetrieb+playtimer
					ltimer = 0;
					if (locnflg > 2) locnflg = 0;
					break;
				}
				case cfrew:{
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					ir_ffrew();
					break;
				}
				case cffwd:{
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					ir_ffwd();
					break;
				}
				case cstop:{
					ir_stop();
					break;
				}
				case cplay:{
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					ir_play(0);
					break;
				}
				case crec:{
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					ir_play(0);
					break;
				}
				case cplayr:{
			#ifdef uher
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					relplayr();		//Pauserelais ein
					ir_end();
			#elif B77	//Pause an, nach 200ms wieder aus: Maschine in Pause
						//Wenn wieder aktiviert: Maschine geht aus Pause
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					relpause();		//Pauserelais ein für 200ms
					ir_end();
			#else
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					ir_play(1);
			#endif
					break;
				}
				case crecr:{
					locnflg = 0;	//löscht Loc.-Suche wenn von IR
					ir_play(1);
					break;
				}
			}	//end switch
			//return ircodenr;
			return 0;
		}	//!relaisbusy
	}	//end if
	return 0;
}
//---------------------------------------------------------------------
void ir_ffwd(void)
{
	relffwd();
	playspeed = revflg = disptime = 0;
	ircodenr = cffwd;		//setzt Flag, falls intern aktiviert
	if (stopflg) reverse = 0;	//durchläuft manchmal nicht Stop-detection Routine
	dlytime = 400/mindiv;	//Monoflop in Stop-Detec. Routine, Anlaufzeit, wird in main  geändert
	allow = 1;	//allow ext. revpin
	//max. Drehzahl beim Umspulen: A77, 18er u. 26er: 46msec
	ir_end();
}
//---------------------------------------------------------------------
//wird auch durchlaufen, wenn ext. FREW od. Relais aktiv, 
//A77: Rew-Signal bleibt aktiv so lange Rewind läuft
void ir_ffrew(void)
{
  relstop(); _delay_ms(20); relstart();//erst STOP bevor REW, wg. ULN
  relfrew();
	playspeed = disptime = 0;
	revflg = 1;//reverse gearmed, damit Zähler bis Umkehr in richtige Richtung läuft
	if (stopflg) reverse = 1;	//durchläuft manchmal nicht Stop-detection Routine
	ircodenr = cfrew;		//falls vom FREW Pin aus main
	dlytime = 400/mindiv;	
	allow = 0;	//not allow ext. revpin
	ir_end();
}
//---------------------------------------------------------------------
//wird auch durchlaufen, wenn irstopflg aktiv
void ir_stop(void)
{
	if (stopflg){
		irstopflg = 0;
		relstop();	//nochmal auslösen zulassen, falls Sensoren defekt
		ir_end();
		return;
	}

  //ircodenr = cstop;   //sonst geht es nicht direkt in play 
	dlytime = 400/mindiv;	//falls Lauftrichtungsumkehr bei Stopfunktion

#ifdef A77
//beim 1. Test set irstopflg und FREW/FFWD, danach kein Test mehr, STOP abhängig von time0
//irstopflg = 0 wenn cstop, >0 wenn von main
	if (!irstopflg){			//=0, wenn STOP durch cstop (FB) aktiviert (oneshot)
		irstopflg++;		//STOP gearmed, wird dadurch von main aufgerufen, muss sein
		if (playspeed || (stpkst < 2) ){	//wenn Playmode: sofort STOP Relais an
			irstopflg = 0;
			relstop();
		}
		//statt STOP Laufrichtungsumkehr zur Verlangsamung der Spulgeschwindigkeit,
		//nur wenn time0 < stpkst, time0 wird nie kleiner als 80
		else if (reverse && time0 < stpkst){
			relffwd();
			//falls Drehumkehr erfolgt, darf aber nicht fälschlicherweise über STOP gehen
			revflg = 0;	
		}
		else if ( time0 < stpkst){	//frew nur wenn Band nicht zu langsam
			allow = 0;	//not allow ext. revpin
			relfrew();	//ir_ffrew wird durchlaufen durch ext. FREW
			revflg = 1;	
		}
		else relstop();	//falls alle Bedingungen nicht zutrafen
	}
	else if	(time0 >= stpkst) {	//nur wenn vorher FFWD oder FREW aktiv
		irstopflg=0;	//ir_stop nicht wiederholt aus main
		relstop();
	}
#else	//alle ausser A77
	relstop();	
#endif
	ir_end();
}
//--------------------------------------------------------------------
//ir_play wird auch vom Hauptprogramm genutzt,
//Annahme: ircodenr ist unverändert cplay oder crec
//in ir_end wird Playmode gearmed durch laufwcode=cplay/crec
//--------------------------------------------------------------------
void ir_play(uint8_t revmode)
{
	if ( revmode ){		//Umschalten von PLAY <-> PLAYR ermöglichen
		if( revplay && playspeed){
			return; //wenn bereits play: nichts machen
		}
	}
	else if (!revplay && playspeed) return;

	//bei 1. Abfrage ist laufwcode != cplay/crec: nur STOP auslösen
	if (laufwcode == cplay || laufwcode == crec || laufwcode == cplayr || laufwcode == crecr){	
		if (!stopflg || relaisbusy) return;	//=0, Band läuft, nichts machen
		sync = 0;
		revplay = 0;	//reverse play flag, nur bei TEAC
	#ifdef uher
		relplay();
	#elif teac
		if (revmode){
			relplayr();		//Playrelais immer dazu
			revplay = 1;	//Play Reverse flag
		}
		else {
			relplay();
		}
		if (ee_read8(6) != revplay){
			ee_write8(6);	//save Reverse flag
			locnfill();	//Locations füllen mit 3min Abständen nach Richtungswechsel
			_delay_ms(3000);	//da es bis 3 sec dauert bis Band anläuft
		}
	#else
		_delay_ms(500);	//sonst zu schnell vom Umspulen in PLAY nur bei A77
		relplay();
	#endif
    //Record nach Playrelais einschalten
    if (laufwcode == crec || laufwcode == crecr){
      allow = 0;  //bei Record wird gleichzeitig FREW Relais aktiv, deshalb rewpin-Abfrage verhindern
      _delay_ms(50); //Delay für Relais
      relrec();
      locmode = 0;  //damit nicht versehentlich 3min. vorgespult wird
      laufwcode = cplay; //da später nur auf cplay abgefragt wird
    }
		revflg = reverse = irstopflg = 0;
		//Anfangsdelay damit nicht STOP erkannt wird, wird in main mit playspeed neu errechnet
	#ifdef uher
		dlytime = 16000 / mindiv;	//16sec für 4.75
	#else
		dlytime = 16000 / mindiv; //wg. 9,5cm/s, früher 8000
	#endif
	}
	else ir_stop();	//beim 1. mal PLAY: erst STOP auslösen
	ir_end();	//setzt laufwcode = ircodenr für 2. Durchgang
}
//--------------------------------------------------------------------
//alle Relais aus, START/STOP Relais ein
//--------------------------------------------------------------------
void ir_end(void)
{
	//relaisbusy solange 0, bis vorheriger reltimer abgelaufen
	//kann aber dazu führen, dass ir_end mal nicht ausgeführt wird
	//muss durch aufrufendeds Programm geprüft werden
	relaisbusy = 1;	//alle Relais sicher abschalten
	laufwcode = ircodenr;
	starttime = runflg = 0;
}
//------------------------------------------------
//Zeitlocation setzen wenn Locationtaste 2x gedrückt
//------------------------------------------------
void savelocn(int tinput)
{
	locnflg = 0;	//zurücksetzen

	if (ilocn >= ilocmax) ilocn = -1;	//Speichern von vorne beginnen
	lcd_clear();
	lcd_string("Locn saved in  ");
	timelocn[++ilocn] = tinput;
	ee_timelocn(ilocn);
	lcd_char(ilocn);
}
//------------------------------------------------
//Zeitlocation lesen wenn num. Taste gedrückt
//------------------------------------------------
void readlocn(void)
{
	timesearch = timelocn[menunr];	//timelocn[10]
	if (timesearch >=0 && timesearch < 8*3600){	//0...8 Std.
    zeile1();
		lcd_string("Wind to ");
		lcd_time(timesearch, 2);
		//lcd_string("  ");
		timesearch = timesearch;		//für runlocn
		locnflg = 4;			//function runlocn
		if (timeword > timesearch) ir_ffrew();
		else ir_ffwd();
	}
	else {
		lcd_string("Locatn not valid");
		_delay_ms(3000);
	}
}
//------------------------------------------------
//zur Zeitlocation spulen
//Routine wird so lange durchlaufen, bis locnflg=0
//------------------------------------------------
void runlocn(void)
{
  zeile1();
  lcd_string("Wind to ");
  lcd_time(timesearch, 2);
	if ( nearstop(timesearch) )	locnflg = 0;	//Ziel erreicht
}
