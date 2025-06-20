#include <stdlib.h>
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
#define DEBUGPRINT false
#if DEBUGPRINT==true
// truquitos print
/* prints encadenados para que sea mas facil imprimir texto
 la diferncia entre
Serial.print(1);
Serial.print(2);
Serial.print(3);
Serial.print(4);
Serial.print(5);
 y
ppln(1, 2, 3, 4, 5);
*/
template<typename... pargs>
void pp(pargs... args) {
    (Serial.print(args), ...);
}
template<typename... pargs>
void ppln(pargs... args) {
    (Serial.print(args), ...); Serial.println();
}
#else
template<typename... pargs> void pp(pargs... args) {}
template<typename... pargs> void ppln(pargs... args) {}
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
enum customChars : char {CHR_LOOP, CHR_CLOCK, CHR_TEMPLOW, CHR_TEMPHIGH};

const int // ubicacion de iconos
	tempX  = 0,
	clockX = tempX+2,
	repsX  = clockX+8; // 10
//# #99m59s #9
//0 2       10

// ----- Variables Microondas -----
enum cookTimesEnum { CT_FAST, CT_UNFREEZE, CT_REHEAT, CT_USER, CT_CFGMEM };
enum cookSegment { C_HOT, C_COLD, C_REPS, C_DONE = 2 };
int cookTimes[5][3] = {
	// tiempo calentado en segundos, tiempo apagado en segundos, repeticiones
	{30,  0, 1}, // CT_FAST (coccion rapida)
	{20, 10, 5}, // CT_UNFREEZE (descongelar)
	{15,  3, 3}, // CT_REHEAT (recalentar)
	// testeo por defecto
	{ 3,  3, 3}, // CT_USER (personalizado)
	{ 0,  0, 0}, // CT_CFGMEM (no es un programa valido: esto es para S_CONFIG)
};
char cookLabels[4][17] = {
	" Coccion rapida ",
	"   Descongelar  ",
	"   Recalentar   ",
	"    Usuario     "
};

long timeTotal = 0; // tiempo total desde que se inicio el programa
char txtBuffer[8];
enum stateEnum {
	S_IDLE,   // esperando que el usuario haga algo
	S_CONFIG, // configurando CT_USER
	S_COOKING, // cocinando (i.e. pasando el tiempo)
	S_COOKINGWAIT // esperando que el usuario cierre la puerta antes de comenzar
};
int curState = S_IDLE;
int changedState = 1; // 2: _RECIEN_ cambiamos (para el x-- al final del loop), 1: cambiamos, 0: seguimos

int chosenProgram = 0; // el programa elegido
int repOverride = -1; // S_IDLE puede iniciar CT_FAST a travez de los numeros... asi que aca estamos
long timeLeft = 0; // tiempo restante para este segmento de coccion
long curSegment = C_HOT;
int repsLeft = 0; // repeticiones restantes para la coccion
bool debugPleaseGoFaster = false; // :(

int configPhase = 0; // en que fase de la configuracion estamos?
bool configAdvance = false; // pasamos a la siguiente fase?
bool configUpdate = false; // hay que actualizar la fase actual por alguna razon?
int inputIndex = 0; // index en el cual insertamos caracteres, para txtBuffer

bool verboseTime = false; // false: solo muestra el tiempo total restante. true: muestra mas info
int updateDisplayPart = 0;
enum displayParts { // para performance
	DP_TIME = 1<<0,
	DP_REPS = 1<<1,
	DP_TEMP = 1<<2,
	DP_ICONS = 1<<3,
	
	DP_TOPROW_DYNAMIC = DP_TIME | DP_REPS | DP_TEMP,
	DP_TOPROW_STATIC = DP_ICONS,
	DP_TOPROW = DP_TOPROW_DYNAMIC | DP_TOPROW_STATIC,
	
	DP_BOTTOMROW = 1<<8,
	
	DP_ALL = DP_TOPROW | DP_BOTTOMROW,
	
	DP_CLEAR = 1<<15,
};

bool doorOpenPrev = false; // estaba la puerta abierta?
bool doorOpen = false; // esta la puerta abierta?

// ----- Utilidad Microondas -----
bool isNum(char ch) {
	// es el char entre ascii '0' y '9'?
	return (ch >= 0x30 && ch <= 0x39);
}

long getProjectedTime() {
	// calcula la cantidad de tiempo que llevara
	// completar el programa, basado en lo que queda
	// por completar, y lo devuelve.
	
	if (curSegment == C_DONE) return 0; // ya terminamos, no hay mas que esperar!
	
	int* prog = cookTimes[chosenProgram];
	
	//long total = (prog[C_HOT] + prog[C_COLD]) * prog[C_REPS] * 1000
	
	long totalFutureReps = (prog[C_HOT] + prog[C_COLD]) * repsLeft * 1000l;
	long totalThisRep = (
		curSegment == C_HOT ?
			  timeLeft + prog[C_COLD] * 1000l // HOT + COLD
			: timeLeft                       //   0 + COLD
	);
	long total = totalFutureReps + totalThisRep;
	
	ppln("pjTime:",total, " <= ([",prog[C_HOT]*1000l, "h + ", prog[C_COLD]*1000l, "c]*", repsLeft, ") + (",totalThisRep,")");
	ppln("      :",totalFutureReps," + ",totalThisRep);
	
	return total;
}

int secondsToBuffer(int secsTotal) {
	// toma segundos
	// lo exporta en formato imprimible a txtBuffer
	int secs = secsTotal % 60;
	int mins = secsTotal / 60;
	snprintf(txtBuffer, 6, "%02d:%02d", mins, secs);
	
	return mins;
}
void updateCookingLCD(int what = 0) {
	updateDisplayPart |= what;
	
	if (updateDisplayPart & DP_CLEAR) lcd.clear(); // SOLO si actualizamos todo, conviene limpiar el LCD...
	if (updateDisplayPart & DP_ICONS) {
		
		lcd.setCursor(tempX, 0);
		lcd.write(verboseTime ? CHR_TEMPHIGH : ' ');
		updateDisplayPart ^= DP_TEMP; // acabamos de hacerlo
		
		lcd.setCursor(repsX, 0);
		lcd.print(verboseTime ? (char)CHR_LOOP : (char)' ');
		lcd.print("  "); // DP_ICONS esta presente si alternamos verbose, no?
		// DP_REPS va a poner lo que corresponde, seguro
		
		lcd.setCursor(clockX, 0);
		lcd.write(CHR_CLOCK);
	}
	
	int charsWritten = 0;
	
	// tiempo restante
	if (updateDisplayPart & DP_TIME) {
		lcd.setCursor(clockX+1, 0);
		
		int timeLeftInSecs = curSegment == C_DONE ? 0 : (
			verboseTime ?
				timeLeft / 1000l : // mostrar TODO
				getProjectedTime() / 1000l // solo mostrar tiempo restante
		);
		
		int secs = timeLeftInSecs % 60;
		int mins = timeLeftInSecs / 60;
		charsWritten = snprintf(txtBuffer, 6, "%02d:%02d", mins, secs);
		
		lcd.print(txtBuffer);
		for (int i = charsWritten; i < 6; i++) lcd.print(" ");
	}
	
	// reps
	if (updateDisplayPart & DP_REPS && verboseTime) {
		lcd.setCursor(repsX+1, 0);
		charsWritten = snprintf(txtBuffer, 3, "%d", repsLeft);
		lcd.print(txtBuffer);
		for (int i = charsWritten; i < 2; i++) lcd.print(" ");
	}
	
	// segmento - temperatura, fase, como se llame xd
	if (updateDisplayPart & DP_TEMP && verboseTime) {
		lcd.setCursor(tempX, 0);
		switch (curSegment) {
			case C_HOT :
				lcd.write(CHR_TEMPHIGH); break;
			case C_COLD:
			case C_DONE:
				lcd.write(CHR_TEMPLOW ); break;
		}
	}
	
	// el display de estado ese
	if (updateDisplayPart & DP_BOTTOMROW) {
		lcd.setCursor(0,1);
		if (curSegment == C_DONE) {
			lcd.print("Coccion completa");
		} else {
			if (doorOpen) lcd.print("Cierre la puerta");
			else lcd.print(cookLabels[chosenProgram]);
		}
	}
	
	updateDisplayPart = 0;
}

void changeState(int to) {
	curState = to;
	changedState = 2;
}

//##################

void setup() {
	Serial.begin(9600);
	Serial.println();
	Serial.println("-- reset --");
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
	doorOpenPrev = doorOpen;
	doorOpen = (analogRead(A0) > 512);
	
	// viendo que la luz solo funciona en un estado y en un caso determinado,
	// es mas compacto manejarlo afuera de estos:
	digitalWrite(PIN_LIGHT, doorOpen || curState == S_COOKING && curSegment != C_DONE ? 1 : 0);
	
	char key = kp.getKey();
	bool anyKey = (key != NO_KEY);
	bool numKey = (key >= 0x30 && key <= 0x39);
	
	if (anyKey) ppln("> keypad: ", key);
	
	// S_IDLE actua como un hub para los distintos estados.
	// TODOS los estados pasan por S_IDLE en algun momento, nunca de uno al otro.
	// por ende, usar S_IDLE como limpieza de pines y variables.
	
	if (changedState) ppln("cambio de estado");
	
	if (curState == S_IDLE) {
		if (changedState) {
			lcd.clear();
			lcd.noCursor();
			lcd.print("Esperando...");
			
			noTone(PIN_PIEZO); digitalWrite(PIN_MOTOR, 0);
		}
		
		if (anyKey) {
			if (key >= 'A' && key <= 'D') {
				chosenProgram = CT_FAST + (key - 'A');
				changeState(doorOpen ? S_COOKINGWAIT : S_COOKING);
			}
			if (key >= '1' && key <= '9') {
				chosenProgram = CT_FAST;
				repOverride = (int)(key - '0'); // si (int)'1' es n, [n - n = 0]. (int)'0' es n-1, [n - (n-1) = 1]
				changeState(doorOpen ? S_COOKINGWAIT : S_COOKING);
			}
			if (key == '#') {
				changeState(S_CONFIG);
			}
		}
	}
	
	else if (curState == S_CONFIG) {
		if (changedState) {
			lcd.clear();
			lcd.noCursor();
			
			configPhase = -1; // jeje
			configAdvance = true;
			timeLeft = 3000; // ;)
		}
		
		if (configPhase >= 4) {
			timeLeft -= delta;
			if (timeLeft <= 0) changeState(S_IDLE);
		}
		
		if (configAdvance) {
			configAdvance = false;
			inputIndex = 0;
			lcd.noCursor();
			
			// si estamos avanzando, es por que terminamos con lo anterior
			// guardemos lo ingresado
			if (configPhase >= 0 && configPhase <= 2) {
				int data = (int)strtol(txtBuffer, NULL, 10); // dicen en interné que atoi es medio raro
				for (int i = 0; i <= 7; i++) pp((int)txtBuffer[i], ","); ppln(" | ",data);  // debug
				cookTimes[CT_CFGMEM][configPhase] = data;
			}
			
			for (int i = 0; i <= 7; i++) txtBuffer[i] = '\0'; // limpiar para que no quede nada raro
			configPhase++;
			
			/*
			0: segundos calentamiento
			1: segundos apagado
			2: repeticiones
			3: confirmar?
			4: OK!
			*/
			
			if (configPhase <= 2) {
				lcd.clear();
				//Ingrese segundos
				//Calentado *#####
				//Apagado   *#####
				//Repetir   *##
				
				lcd.print("Ingrese ");
				if (configPhase == 2)
					 lcd.print("cantidad");
				else lcd.print("segundos");
				
				lcd.setCursor(0,1);
				switch (configPhase) {
					case 0: lcd.print("Calentado "); lcd.print((char)CHR_CLOCK); break;
					case 1: lcd.print("Apagado   "); lcd.print((char)CHR_CLOCK); break;
					case 2: lcd.print("Repetir   "); lcd.print((char)CHR_LOOP ); break;
				}
				
				lcd.cursor();
			} else {
				if (configPhase == 3) {
					lcd.clear();
					lcd.setCursor(0,1);
					
					//lcd.print("*00:00 *00:00 *0");
					
					lcd.print((char)CHR_TEMPHIGH);
					secondsToBuffer(cookTimes[CT_CFGMEM][C_HOT]);
					lcd.print(txtBuffer);
					lcd.print(' ');
					
					lcd.print((char)CHR_TEMPLOW);
					secondsToBuffer(cookTimes[CT_CFGMEM][C_COLD]);
					lcd.print(txtBuffer);
					lcd.print(' ');
					
					lcd.print((char)CHR_LOOP);
					int reps = cookTimes[CT_CFGMEM][C_REPS];
					char represent = (char)('0'+reps);
					lcd.print(reps > 9 ? '+' : represent);
				}
				
				lcd.setCursor(0,0);
				
				if (configPhase == 3) {
					lcd.print("OK?    Apriete #");
					lcd.setCursor(15,0);
					lcd.cursor();
				}
				if (configPhase == 4) {
					for (int i = 0; i <= 2; i++) cookTimes[CT_USER][i] = cookTimes[CT_CFGMEM][i];
					int* prog = cookTimes[CT_USER];
					ppln("Programa guardado: {", prog[C_HOT], ",", prog[C_COLD], ",", prog[C_REPS], "}");
					
					lcd.print("LISTO :)        ");
				}
			}
		}
		
		if (configUpdate && configPhase <= 2) {
			configUpdate = false;
			int lastIndex = inputIndex - 1;
			lcd.setCursor(11 + lastIndex, 1); // justo despues de simbolo
			lcd.write(txtBuffer[lastIndex]);
		}
		
		if (anyKey) {
			if (isNum(key) && configPhase <= 2) {
				txtBuffer[inputIndex] = key;
				if (inputIndex < 3) inputIndex++; // no te pases
				configUpdate = true;
			}
			if (key == '*') {
				// #cancela2
				changeState(S_IDLE);
			}
			if (key == '#') {
				if (configPhase < 4) configAdvance = true;
				else changeState(S_IDLE);
			}
			// tenia pensado poner un boton estilo backspace para debugear mas facil
			// pero pense "che, mi microondas no hace eso :("
			// asi que mejor no JAJAJAJAJ
		}
	}
	
	else if (curState == S_COOKINGWAIT) {
		// estado especial: queremos cocinar,
		// pero el usuario dejo la puerta abierta
		if (changedState) {
			lcd.clear();
			lcd.print("Cierre la puerta");
			lcd.setCursor(0,1);
			lcd.print("para comenzar...");
		}
		
		if (!doorOpen) changeState(S_COOKING);
		
		if (anyKey) {
			if (key == '*') {
				changeState(S_IDLE);
			}
		}
	}
	else if (curState == S_COOKING) {
		if (changedState) {
			timeLeft = cookTimes[chosenProgram][C_HOT] * 1000l;
			repsLeft = repOverride >= 0 ? repOverride : cookTimes[chosenProgram][C_REPS];
			repOverride = -1;
			curSegment = C_HOT;
			
			//Serial.println(chosenProgram);
			updateCookingLCD(DP_ALL|DP_CLEAR); // conviene hacerlo ahora
		}
		
		if (anyKey) {
			if (key == '*') {
				changeState(S_IDLE);
				// "servira para detener el programa llevando a 0 los segundos restantes"
				// el boton cambia de estado a S_IDLE, *tecnicamente* terminando el programa.
				// cuenta?
				
				/*
				osea digamos que
				
				repsLeft = 0;
				timeLeft = 0;
				if (!timeLeft) changeState(S_IDLE);
				
				ahora el tiempo haria consecuente el cambio de estado
				jeje
				*/
			}
			if (key == '#') {
				verboseTime = !verboseTime;
				updateDisplayPart |= DP_TOPROW; // limpiar los iconos tambien
			}
			if (key == 'A') {
				debugPleaseGoFaster = !debugPleaseGoFaster; // EEEEEEEEEEEEEEEEEEEe
				ppln("nyoom? ", (int)debugPleaseGoFaster);
			}
		}
		
		//ppln("# COCINA step tl:",timeLeft); // SPAM
		// el tiempo pasa
		if (curSegment == C_DONE) {
			debugPleaseGoFaster = false;
			long tlPrev = timeLeft;
			timeLeft -= delta;
			
			if (timeLeft > 0) {
				tone(PIN_PIEZO, 400);
			} else noTone(PIN_PIEZO);
			
			if (timeLeft < -3000 || anyKey || doorOpen) {
				changeState(S_IDLE);
			}
			
			goto skipCooking; // poner varios niveles de "if" no me sienta bien.
			// podria engrupar lo siguiente en un cookStep() otra vez, peeeeerooooo...
		}
		
		if (doorOpen) {
			if (!doorOpenPrev) {
				// mostrar advertencia al usuario antes de que las microondas lo destruyan
				noTone(PIN_PIEZO);
				digitalWrite(PIN_MOTOR, 0);
				updateDisplayPart |= DP_BOTTOMROW;
			}
			
		} else { // puerta cerrada
			if (doorOpenPrev) updateDisplayPart |= DP_BOTTOMROW;
			
			if (curSegment != C_DONE) {
				int secsBefore = timeLeft / 1000l;
				timeLeft -= delta;
				if (debugPleaseGoFaster) timeLeft -= delta * 55l;
				//ppln("delta ", delta, "ms => tl:", timeLeft, "ms)"); // SPAM
				int secsNow = timeLeft / 1000l;
				if (secsBefore != secsNow) updateDisplayPart |= DP_TIME;
				
				if (timeLeft <= 0 && curSegment != C_DONE) {
					//pp("cambio de segmento: ");
					updateDisplayPart |= DP_TOPROW_DYNAMIC; // cambia temp, tiempo y reps
				
					// cambia de segmento si es apropiado hacerlo
					do {
						if (curSegment == C_HOT) {
							curSegment = C_COLD;
							//pp("C_COLD,");
						} else if (curSegment == C_COLD) {
							if (repsLeft > 0) {
								curSegment = C_HOT;
								//pp("C_HOT,");
								repsLeft--;
							} else {
								//pp("!!! C_DONE !!!");
								curSegment = C_DONE;
								timeLeft = 1000; // caso excepcional
								updateDisplayPart |= DP_BOTTOMROW;
							}
						}
						
						if (curSegment != C_DONE) timeLeft += cookTimes[chosenProgram][curSegment] * 1000l;
					} while (timeLeft <= 0 && curSegment != C_DONE);
					// repetimos el cambio de fase hasta que haya tiempo para cocinar
					// o si ya terminamos el programa.
					// en consecuencia, esto salta sobre fases de 0 segundos
					// y hace que un programa {0, 0, 999} termine en un instante.
					
					//ppln(" ntl:", timeLeft);
				}
				
				switch (curSegment) {
					case C_HOT :   tone(PIN_PIEZO,  70); digitalWrite(PIN_MOTOR, 1); break;
					case C_COLD:   tone(PIN_PIEZO,  20); digitalWrite(PIN_MOTOR, 0); break;
					case C_DONE: noTone(PIN_PIEZO)     ; digitalWrite(PIN_MOTOR, 0); break;
				}
			}
		}
		
		skipCooking:
		
		if (updateDisplayPart) updateCookingLCD();
		
	}
	
	if (changedState) changedState--;
	timeTotal = timeNow;
}