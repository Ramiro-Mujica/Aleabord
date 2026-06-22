#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//   PINES  
const int PIN_JOY_X = A0;
const int PIN_JOY_Y = A1;
const int PIN_BTN    = 2;

const int PIN_R = 9;
const int PIN_G = 10;
const int PIN_B = 11;

LiquidCrystal_I2C lcd(0x27, 16, 2);

//   TIEMPOS (ms)  
const unsigned long TIEMPO_BIENVENIDA            = 5000; // mensaje de bienvenida al encender
const unsigned long TIEMPO_DETECCION_MOVIMIENTO  = 1000; // movimiento sostenido para arrancar el tiro
const unsigned long TIEMPO_DETECCION_QUIETUD     = 1000; // quietud sostenida para empezar a frenar
const unsigned long DURACION_FASE_LENTA          = 2000; // duración de la fase lenta antes de detenerse
const unsigned long TIEMPO_MOSTRAR_RESULTADO     = 3000; // mostrando el resultado final de los dados
const unsigned long TIEMPO_MOSTRAR_SUMA          = 3000; // mostrando "Jugador X salió Y"
const unsigned long INTERVALO_CUENTA_REGRESIVA   = 1000; // entre cada número de la cuenta regresiva
const unsigned long ANTIRREBOTE                  = 50;

const int UMBRAL_JOYSTICK = 150; // distancia desde el centro (512) para considerar "movimiento"

//   ESTADOS  
enum EstadoJuego { BIENVENIDA, TURNO, TIRANDO_RAPIDO, TIRANDO_LENTO, MOSTRAR_RESULTADO, MOSTRAR_SUMA, CUENTA_REGRESIVA };
EstadoJuego estadoActual = BIENVENIDA; // 

//   JUGADORES  
const int colores[4][3] = {
  {255, 0,   0},   // Rojo
  {255, 40,  0},   // Amarillo 
  {0,   255, 0},   // Verde
  {0,   0,   255}  // Azul
};
const char* nombres[4] = {"Rojo", "Amarillo", "Verde", "Azul"};
int jugadorActual = 0;

//   DADOS  
int dado1 = 1;
int dado2 = 1;

//   CONTROL DE TIEMPO  
unsigned long tiempoInicioMovimiento = 0;
unsigned long tiempoInicioQuietud    = 0;
unsigned long tiempoUltimoCambioDado = 0;
unsigned long tiempoInicioEstado     = 0;
int intervaloRandomActual = 150;
int pasoCuentaRegresiva   = 3;

//   BOTÓN (antirrebote)  
int ultimaLecturaCruda    = HIGH;
int estadoBotonConfirmado = HIGH;
unsigned long ultimoCambioBoton = 0;

void setup() {
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  randomSeed(analogRead(A2));

  setColor(colores[jugadorActual][0], colores[jugadorActual][1], colores[jugadorActual][2]);

  mostrarBienvenida();
  tiempoInicioEstado = millis();
}

void loop() {
  int magnitud = leerMagnitudJoystick();
  bool hayMovimiento = (magnitud > UMBRAL_JOYSTICK);

  // El click solo cambia de jugador en el estado TURNO (bloqueado durante el resto)
  if (estadoActual == TURNO && detectarClickBoton()) {
    cambiarJugador();
  }

  switch (estadoActual) {
    case BIENVENIDA:         manejarBienvenida(); break;
    case TURNO:               manejarTurno(hayMovimiento); break;
    case TIRANDO_RAPIDO:      manejarTirandoRapido(hayMovimiento, magnitud); break;
    case TIRANDO_LENTO:       manejarTirandoLento(); break;
    case MOSTRAR_RESULTADO:   manejarMostrarResultado(); break;
    case MOSTRAR_SUMA:        manejarMostrarSuma(); break;
    case CUENTA_REGRESIVA:    manejarCuentaRegresiva(); break;
  }
}

//   ESTADOS  

void manejarBienvenida() {
  if (millis() - tiempoInicioEstado >= TIEMPO_BIENVENIDA) {
    estadoActual = TURNO;
    mostrarPantallaTurno();
  }
}

void manejarTurno(bool hayMovimiento) {
  if (hayMovimiento) {
    if (tiempoInicioMovimiento == 0) tiempoInicioMovimiento = millis();
    if (millis() - tiempoInicioMovimiento >= TIEMPO_DETECCION_MOVIMIENTO) {
      iniciarTiro();
    }
  } else {
    tiempoInicioMovimiento = 0;
  }
}

void iniciarTiro() {
  estadoActual = TIRANDO_RAPIDO;
  tiempoInicioEstado = millis();
  tiempoInicioQuietud = 0;
  tiempoUltimoCambioDado = millis();
  intervaloRandomActual = 150;
  randomizarDados();
  mostrarDados();
}

void manejarTirandoRapido(bool hayMovimiento, int magnitud) {
  intervaloRandomActual = calcularIntervalo(magnitud);

  if (millis() - tiempoUltimoCambioDado >= (unsigned long)intervaloRandomActual) {
    randomizarDados();
    mostrarDados();
    tiempoUltimoCambioDado = millis();
  }

  if (!hayMovimiento) {
    if (tiempoInicioQuietud == 0) tiempoInicioQuietud = millis();
    if (millis() - tiempoInicioQuietud >= TIEMPO_DETECCION_QUIETUD) {
      estadoActual = TIRANDO_LENTO;
      tiempoInicioEstado = millis();
      intervaloRandomActual = 350;
    }
  } else {
    tiempoInicioQuietud = 0;
  }
}

void manejarTirandoLento() {
  if (millis() - tiempoUltimoCambioDado >= (unsigned long)intervaloRandomActual) {
    randomizarDados();
    mostrarDados();
    tiempoUltimoCambioDado = millis();
  }

  if (millis() - tiempoInicioEstado >= DURACION_FASE_LENTA) {
    randomizarDados();
    mostrarDados();
    estadoActual = MOSTRAR_RESULTADO;
    tiempoInicioEstado = millis();
  }
}

void manejarMostrarResultado() {
  if (millis() - tiempoInicioEstado >= TIEMPO_MOSTRAR_RESULTADO) {
    mostrarSuma();
    estadoActual = MOSTRAR_SUMA;
    tiempoInicioEstado = millis();
  }
}

void manejarMostrarSuma() {
  if (millis() - tiempoInicioEstado >= TIEMPO_MOSTRAR_SUMA) {
    pasoCuentaRegresiva = 3;
    mostrarCuentaRegresiva(pasoCuentaRegresiva);
    estadoActual = CUENTA_REGRESIVA;
    tiempoInicioEstado = millis();
  }
}

void manejarCuentaRegresiva() {
  if (millis() - tiempoInicioEstado >= INTERVALO_CUENTA_REGRESIVA) {
    pasoCuentaRegresiva--;
    tiempoInicioEstado = millis();

    if (pasoCuentaRegresiva <= 0) {
      estadoActual = TURNO;
      tiempoInicioMovimiento = 0;
      tiempoInicioQuietud = 0;
      mostrarPantallaTurno();
    } else {
      mostrarCuentaRegresiva(pasoCuentaRegresiva);
    }
  }
}

//   JOYSTICK  

int leerMagnitudJoystick() {
  int x = analogRead(PIN_JOY_X);
  int y = analogRead(PIN_JOY_Y);
  return max(abs(x - 512), abs(y - 512));
}

int calcularIntervalo(int magnitud) {
  int intervalo = map(magnitud, UMBRAL_JOYSTICK, 512, 200, 60);
  return constrain(intervalo, 60, 200);
}

bool detectarClickBoton() {
  int lecturaActual = digitalRead(PIN_BTN);
  bool click = false;

  if (lecturaActual != ultimaLecturaCruda) {
    ultimoCambioBoton = millis();
  }

  if ((millis() - ultimoCambioBoton) > ANTIRREBOTE) {
    if (lecturaActual != estadoBotonConfirmado) {
      estadoBotonConfirmado = lecturaActual;
      if (estadoBotonConfirmado == LOW) click = true;
    }
  }

  ultimaLecturaCruda = lecturaActual;
  return click;
}

//   JUGADOR / COLOR  

void cambiarJugador() {
  jugadorActual = (jugadorActual + 1) % 4;
  setColor(colores[jugadorActual][0], colores[jugadorActual][1], colores[jugadorActual][2]);
  mostrarPantallaTurno();
}

void setColor(int r, int g, int b) {
  analogWrite(PIN_R, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_B, b);
}

//   DADOS  

void randomizarDados() {
  dado1 = random(1, 7);
  dado2 = random(1, 7);
}

//   PANTALLA  

void mostrarBienvenida() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Bienvenido a");
  lcd.setCursor(0, 1); lcd.print("Aleabord!!!");
}

void mostrarPantallaTurno() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Jugador");
  lcd.setCursor(0, 1); lcd.print(nombres[jugadorActual]);
}

void mostrarDados() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Dado1: "); lcd.print(dado1);
  lcd.setCursor(0, 1); lcd.print("Dado2: "); lcd.print(dado2);
}

void mostrarSuma() {
  int suma = dado1 + dado2;
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Jugador "); lcd.print(nombres[jugadorActual]);
  lcd.setCursor(0, 1); lcd.print("Salio: "); lcd.print(suma);
}

void mostrarCuentaRegresiva(int numero) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Proximo tiro en:");
  lcd.setCursor(0, 1); lcd.print(numero);
}