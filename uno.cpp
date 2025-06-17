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

// ----- debug -----
#define DEBUGPRINT true
#if DEBUGPRINT==true
template<typename... pargs>
void pp(pargs... args) {
    (Serial.print(args), ...); Serial.println();
} // truquito print
#else
template<typename... pargs>
void pp(pargs... args) {} // nada
#endif

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
byte chardef_templow[] = {
  B01110,
  B01010,
  B01010,
  B01010,
  B10001,
  B11111,
  B11111,
  B01110
};
byte chardef_temphigh[] = {
  B01110,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B11111,
  B01110
};
enum customChars : byte {CHR_LOOP, CHR_CLOCK, CHR_TEMPLOW, CHR_TEMPHIGH};

const int clockX = 0, repsX = 0; // ubicacion de iconos

// ----- Variables Microondas -----
enum cookTimesEnum { CT_FAST, CT_UNFREEZE, CT_REHEAT, CT_USER };
enum cookSegment { C_HOT, C_COLD, C_REPS, C_DONE = 2 };
int cookTimes[4][3] = {
	// tiempo calentado en segundos, tiempo apagado en segundos, repeticiones
	{30,  0, 1}, // CT_FAST (coccion rapida)
	{20, 10, 5}, // CT_UNFREEZE (descongelar)
	{15,  3, 3}, // CT_REHEAT (recalentar)
	// testeo por defecto
	{ 3,  3, 3}  // CT_USER (personalizado)
};
int chosenProgram = 0; // el programa elegido

long timeTotal = 0; // tiempo total desde que se inicio el programa

long timeLeft = 0; // tiempo restante para este segmento de coccion
long curSegment = C_HOT;
int repsLeft = 0; // repeticiones restantes para la coccion

bool doorOpen = false; // esta la puerta abierta?

enum stateEnum {
	S_IDLE,   // esperando que el usuario haga algo
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

bool cookAdvance() {
	pp("cookAdvance()");
	// devuelve bool: "ya termino la coccion?"
	if (curSegment == C_HOT) {
		curSegment = C_COLD;
		pp("cookAdvance: C_COLD");
	} else {
		if (repsLeft <= 0) {
			pp("cookAdvance: C_DONE!");
			curSegment = C_DONE;
			return true;
		}
		curSegment = C_HOT;
		pp("cookAdvance: C_HOT, rep--");
		repsLeft--;
	}
	timeLeft += cookTimes[chosenProgram][curSegment] * 1000;
	pp("cookAdvance: timeLeft = ", timeLeft);
	return false;
}
void cookStep(long delta) {
	pp("cookStep(", delta, ")");
	// hace pasar el tiempo
	timeLeft -= delta;
	if (timeLeft <= 0 && curSegment != C_DONE) {
		pp("cookStep: negativo!!");
		// este segmento de cocina termino
		cookAdvance();
	}
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
	lcd.createChar(2, chardef_templow);
	lcd.createChar(3, chardef_temphigh);
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
	
	if (anyKey) pp("> keypad: ", key);
	
	if (curState == S_IDLE) {
		if (changedState) {
			lcd.clear();
			lcd.print("S_IDLE");
		}
		
		if (anyKey) {
			if (key >= 'A' && key <= 'D') {
				chosenProgram = CT_FAST + (key - 'A');
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
		
		cookStep(delta);
		lcd.clear();
		lcd.print(timeLeft);
		delay(800);
		
		if (anyKey) {
			if (key == '*') {
				changeState(S_IDLE);
			}
		}
	}
	
	if (changedState) changedState--;
	timeTotal = timeNow;
}