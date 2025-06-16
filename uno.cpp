#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ----- Keypad -----
char kpLabels[4][4] = {
	'1','2','3','A',
	'4','5','6','B',
	'7','8','9','C',
	'*','0','#','D'
};
byte kpPinsRow[4] = {12,11,10,9};
byte kpPinsCol[4] = {8,7,6,5};
Keypad kp = Keypad(makeKeymap(kpLabels), kpPinsRow, kpPinsCol, 4, 4);

// ----- LCD -----
const byte lcdRows = 2, lcdCols = 16;
LiquidCrystal_I2C lcd(0x20, lcdCols, lcdRows);

// ----- DONDE estan los PINES -----
const int PIN_PIEZO = 3;
const int PIN_MOTOR = 4;
const int PIN_LIGHT = 2;
const int PIN_DOOR = A0;

// ----- Variables Microondas -----
enum cookTimesEnum { CT_FAST, CT_UNFREEZE, CT_REHEAT, CT_USER };
int cookTimes[4][3] = {
	// tiempo calentado en segundos, tiempo apagado en segundos, repeticiones
	{30,  0, 1}, // CT_FAST (coccion rapida)
	{20, 10, 5}, // CT_UNFREEZE (descongelar)
	{15,  3, 3}, // CT_REHEAT (recalentar)
	{30,  0, 1}  // CT_USER (personalizado)
};

// ----- Utilidad Microondas -----


//##################

void setup() {
	Serial.begin(9600);
	Serial.println();
	lcd.init();
	lcd.begin(lcdCols, lcdRows, LCD_5x8DOTS);
	lcd.backlight();
	lcd.cursor();
	
	pinMode(PIN_MOTOR	, OUTPUT);
	pinMode(PIN_LIGHT		, OUTPUT);
	pinMode(PIN_PIEZO	, OUTPUT); noTone(PIN_PIEZO);
	pinMode(PIN_DOOR	, INPUT);
	
	//digitalWrite(PIN_LIGHT, HIGH);
	
	Serial.println("Hola mundo!");
}
void loop() {
	Serial.println(analogRead(A0));
	
	delay(500);
	digitalWrite(PIN_MOTOR, 1);
	tone(PIN_PIEZO, 500);
	
	delay(500);
	digitalWrite(PIN_MOTOR, 0);
	noTone(PIN_PIEZO);
	// TODO
}