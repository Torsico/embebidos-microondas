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

// ----- Algunas constantes -----
const int PIN_PIEZO = 3;
const int PIN_MOTOR = 4;
const int PIN_LIGHT = 2;
const int PIN_DOOR = A0;

byte chardef_loop[] = {
  B00111,
  B10110,
  B10101,
  B10001,
  B10001,
  B10101,
  B01101,
  B11100
};
byte chardef_clock[] = {
  B01110,
  B10101,
  B10101,
  B10111,
  B10001,
  B10001,
  B10001,
  B01110
};

// ----- Variables Microondas -----
enum cookTimesEnum { CT_FAST, CT_UNFREEZE, CT_REHEAT, CT_USER };
int cookTimes[4][3] = {
	// tiempo calentado en segundos, tiempo apagado en segundos, repeticiones
	{30,  0, 1}, // CT_FAST (coccion rapida)
	{20, 10, 5}, // CT_UNFREEZE (descongelar)
	{15,  3, 3}, // CT_REHEAT (recalentar)
	{30,  0, 1}  // CT_USER (personalizado)
};

long timeTotal = 0; // tiempo total desde que se inicio el programa
long timeLeft = 0; // tiempo restante para la coccion
int repsLeft = 0; // repeticiones restantes para la coccion

bool doorOpen = false; // esta la puerta abierta?

enum stateEnum {
	S_IDLE, // esperando que el usuario haga algo
	S_COOKING // cocinando (i.e. pasando el tiempo)
};
int curState = S_IDLE;
int changedState = 1; // para reinicializar algunas cosas en cambio de estado

// ----- Utilidad Microondas -----
bool isNum(char ch) {
	// es el char entre ascii '0' y '9'?
	return (ch >= 0x30 && ch <= 0x39);
}

void changeState(int to) {
	curState = to;
	changedState = 2;
}

//##################

void setup() {
	Serial.begin(9600);
	Serial.println();
	lcd.init();
	lcd.begin(lcdCols, lcdRows, LCD_5x8DOTS);
	lcd.createChar(0, chardef_loop);
	lcd.createChar(1, chardef_clock);
	lcd.backlight();
	
	pinMode(PIN_MOTOR, OUTPUT);
	pinMode(PIN_LIGHT, OUTPUT);
	pinMode(PIN_PIEZO, OUTPUT); noTone(PIN_PIEZO);
	pinMode(PIN_DOOR ,  INPUT);
	
	//digitalWrite(PIN_LIGHT, HIGH);
	
	Serial.println("Hola mundo!");
}
void loop() {
	delay(50);
	
	long timeNow = millis();
	long delta = timeNow - timeTotal;
	doorOpen = (analogRead(A0) > 512);
	
	char key = kp.getKey();
	bool anyKey = (key != NO_KEY);
	bool numKey = (key >= 0x30 && key <= 0x39);
	
	if (curState == S_IDLE) {
		if (changedState) {
			lcd.clear();
			lcd.print("S_IDLE");
		}
		
		if (anyKey) {
			Serial.println("key");
			if (key == 'A') {
				changeState(S_COOKING);
			}
		}
	}
	
	else if (curState == S_COOKING) {
		if (changedState) {
			lcd.clear();
			lcd.print("S_COOKING");
		}
		
		if (anyKey) {
			Serial.println("key");
			if (key == '*') {
				changeState(S_IDLE);
			}
		}
	}
	
	if (changedState) changedState--;
	timeTotal = timeNow;
}