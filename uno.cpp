#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// -------Keypad-------
char kpLabels[4][4] = {
	'1','2','3','A',
	'4','5','6','B',
	'7','8','9','C',
	'*','0','#','D'
};
byte kpPinsRow[4] = {9,8,7,6};
byte kpPinsCol[4] = {5,4,3,2};
Keypad kp = Keypad(makeKeymap(kpLabels), kpPinsRow, kpPinsCol, 4, 4);

// -------LCD-------
const byte lcdRows = 2, lcdCols = 16;
LiquidCrystal_I2C lcd(0x20, lcdCols, lcdRows);

// DONDE estan los PINES
const int PIN_PIEZO = 11;
const int PIN_MOTOR = 10;
const int PIN_LUZ = A1;
const int PIN_PUERTA = A0;

//##################

void setup() {
	Serial.begin(9600);
	lcd.init();
	lcd.begin(lcdCols, lcdRows, LCD_5x8DOTS);
	lcd.backlight();
	lcd.cursor();
	
	// TODO
}
void loop() {
	delay(50);
	
	// TODO
}