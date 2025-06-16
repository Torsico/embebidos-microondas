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

const int clockX = 0, repsX = 0; // ubicacion de iconos

// ----- Variables Microondas -----
enum cookTimesEnum { CT_FAST, CT_UNFREEZE, CT_REHEAT, CT_USER };
enum cookSegment { C_HOT, C_COLD, C_REPS };
int cookTimes[4][3] = {
	// tiempo calentado en segundos, tiempo apagado en segundos, repeticiones
	{30,  0, 1}, // CT_FAST (coccion rapida)
	{20, 10, 5}, // CT_UNFREEZE (descongelar)
	{15,  3, 3}, // CT_REHEAT (recalentar)
	// le puse CT_FAST como default
	{30,  0, 1}  // CT_USER (personalizado)
};
int chosenProgram = 0; // el programa elegido

long timeTotal = 0; // tiempo total desde que se inicio el programa

long timeLeft = 0; // tiempo restante para este segmento de coccion
long curSegment = C_HOT;
int repsLeft = 0; // repeticiones restantes para la coccion

bool doorOpen = false; // esta la puerta abierta?

enum stateEnum {
	S_IDLE, // esperando que el usuario haga algo
	S_COOKING // cocinando (i.e. pasando el tiempo)
};
int curState = S_IDLE;
int changedState = 1; // 2: _RECIEN_ cambiamos (para el x-- al final del loop), 1: cambiamos, 0: seguimos

// ----- Utilidad Microondas -----
bool isNum(char ch) {
	// es el char entre ascii '0' y '9'?
	return (ch >= 0x30 && ch <= 0x39);
}

long getProjectedTime() {
	// calcula la cantidad de tiempo que llevara
	// completar el programa, basado en lo que queda
	// por completar, y lo devuelve.
	
	int* prog = cookTimes[chosenProgram];
	
	//long total = (prog[C_HOT] + prog[C_COLD]) * prog[C_REPS] * 1000
	
	long totalFutureReps = (prog[C_HOT] + prog[C_COLD]) * repsLeft * 1000;
	long totalThisRep = (
		curSegment == C_HOT ?
			  timeLeft + prog[C_COLD] * 1000 // HOT + COLD
			: timeLeft                       //   0 + COLD
	);
	long total = totalFutureReps + totalThisRep;
	
	return total;
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
				chosenProgram = CT_FAST;
				changeState(S_COOKING);
			}
		}
	}
	
	else if (curState == S_COOKING) {
		if (changedState) {
			lcd.clear();
			lcd.print("S_COOKING");
			
			timeLeft = cookTimes[chosenProgram][C_HOT] * 1000;
			repsLeft = cookTimes[chosenProgram][C_REPS] - 1;
			curSegment = C_HOT;
			
			lcd.setCursor(0,1);
			lcd.print(getProjectedTime());
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