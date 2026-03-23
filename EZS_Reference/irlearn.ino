#include "main.h"
#include "irmp.h"


char txtir[][21] =  {" '0' "," '1' "," '2' "," '3' "," '4' ",
					 " '5' "," '6' ", " '7' ", " '8' ", " '9' ",
				#ifdef uher
					 "T-INP","TIMER","LOCAT", "T-LEN","FREW ","PLAY ","FFWD ","STOP ","REC  ","PAUSE","RECR "};
				#elif B77
					 "T-INP","TIMER","LOCAT", "T-LEN","FREW ","PLAY ","FFWD ","STOP ","REC  ","PAUSE","RECR "};
				#else
           "T-INP","TIMER","LOCAT", "T-LEN","FREW ","PLAY ","FFWD ","STOP ","REC  ","PLAYR","RECR "};
//ircodenr = 		10		11		12		     13		14		      15		16		  17		 18		    19     20
				#endif

//IR-Codes lernen: LW- oder Ziffern-Eingabe
//---------------------------------------------
void ir_learn(void)
{
  #ifdef playtimer
     #define skip 99
  #else
     #define skip 13
  #endif
	uint8_t	 i, ptr, r25 = 0;

	relstop();		//Laufwerk stop
	zeile1();

	if (taste2on == 1)
	{
		lcd_string("Drive control:  ");
		ptr = 10;		//ab Text Laufwerksteuerng
		r25 = ziffnr+lwnr;

		#ifdef uher
			r25--;	//zusätzlich Pause Abfrage
		#elif B77
			r25--;	//zusätzlich Pause Abfrage
		#elif A77
				r25 -= 2;	//keine PLAYR/RECR Abfrage
		#endif
	}
	else if (taste2on == 2)
	{
		lcd_string("Learning no 0..9");
		ptr = 0;		//Text Ziff 0..9
		r25 = ziffnr;
	}
	else goto ir_end;		//falsche Eingabe

	zeile2();
	lcd_string("Button for ");

	for (i=ptr; i<r25; i++)
	{
    if (i == skip)    //überspringt T-LEN Abfrage wenn nicht playtimer
      taste0[i] = 0;
    else {
  		lcd_setcursor( 11, 2 );
  		lcd_string(txtir[i]);	//Lerntext Ausgabe
  		tmp16 = ir_get(5100);	//hole IR-Code
  		if (tmp16 == 255 ) goto ir_end;	//kein neuer Code, exit
  		else taste0[i] = tmp16;			//speichere neuen Code
    }
	}
	ee_writelw(); //alle Laufwerksdaten auf EEPROM schreiben
	lcd_string("Codes saved     ");

ir_end:
	taste2on = 0;	//zurücksetzen
	relstart();
}	//end ir_learn

//IR-Code lesen
//---------------------------------------------
uint8_t ir_get(int16_t del)
{
	IRMP_DATA irmp_data;
	irtimer = 0;	//delay counter wenn keine Eingabe
	
	while (!(irmp_get_data(&irmp_data)) || irmp_data.flags)	//flags=1=repeat
		if (irtimer > del) return 255;	//warte 5 sec

	if (del == 5100) 	//Learnmode
		iradr = irmp_data.address+irmp_data.protocol;
	else
		if (irmp_data.address+irmp_data.protocol != iradr) return 255;	//falsche FB

	return irmp_data.command;	//kann auch 0 sein, daher 255 als Fehlercode
}
