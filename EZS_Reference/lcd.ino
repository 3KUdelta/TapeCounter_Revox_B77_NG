void lcd_setcursor(byte p,byte z)
{
  z--;
  lcd.setCursor(p,z);
}
//Schreibt Zeit in h:mm:ss
void lcd_time( int16_t number, uint8_t len )  //len=1: h:min, len=2: h:min:sec
{
  uint8_t hr;
  uint16_t min, tmp;
  uint8_t sec;

  if (number < 0){
    number = -number;
    lcd_char( '-' );
  }
  else
    lcd_char( ' ' );

  hr  = number/3600;
  tmp = (number%3600);  //Modulo, Rest
  min = tmp/60;

  lcd_char(hr);
  lcd_char( ':' );
  lcd_char(min/10);
  lcd_char(min%10);

  if (len == 2){
    sec = tmp - min*60;
    lcd_char( ':' );
    lcd_char(sec/10);
    lcd_char(sec%10);
  }
}

