#include "main.h"
//#include <avr/eeprom.h>

//----EEPROM-Data---------------------------------------------------------
//EEPROM sichert Zähler und Länge nach Ausschalten
//darf nicht in main.h, sonst "multiple declaration of elenbuff"

//Daten für EEPROM müssen alle uint.._t sein, 1024 Bytes Platz bei ATMEGA 328
uint32_t ebuff32	;				//  4, Summe=61+300
uint16_t ebuff16[13] ;				// 26
uint16_t etimelocn[ilocmax];	//15*10*2
uint8_t	iredata[lwnr+ziffnr+1] ;	// 21 Bytes
uint8_t  ebuff8[10]  ;				// 10
/*
void ee_readlw(void)
{
	eeprom_read_block (taste0, iredata, sizeof(iredata));
}
*/
void ee_read(void)	//alles einlesen
{
	eeprom_read_block (buff16, &ebuff16, sizeof(buff16));
	eeprom_read_block (buff8, &ebuff8, sizeof(buff8));
	eeprom_read_block (taste0, &iredata, sizeof(iredata));
	eeprom_read_block (timelocn, &etimelocn, sizeof(timelocn));
	minrightl = eeprom_read_dword (&ebuff32);
}
uint8_t ee_read8(uint8_t adr)
{
	 return eeprom_read_byte (&ebuff8[adr]);
}
void ee_writetim(void)	//nur bei Spannungsabfall
{
	eeprom_write_block (buff16, &ebuff16, sizeof(buff16));
	eeprom_write_block (buff8, &ebuff8, sizeof(buff8));	//wg. autosflg für Schaltuhrbetrieb
	ee_write32();
}
void ee_writelw(void)	//nur aus irlearn
{
	eeprom_update_block (taste0, &iredata, sizeof(iredata));
}
void ee_write_tl(void){
	eeprom_update_block (timelocn, &etimelocn, sizeof(etimelocn));
}
void ee_write16(int8_t adr)
{
	eeprom_update_word (&ebuff16[adr], buff16[adr]);
}
void ee_timelocn(int8_t adr)
{
	eeprom_update_word (&etimelocn[adr], timelocn[adr]);
}
void ee_write8(int8_t adr)
{
	eeprom_write_byte (&ebuff8[adr], buff8[adr]);
}
void ee_write32(void)
{
	eeprom_update_dword (&ebuff32, buff32);
}

