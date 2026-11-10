#include <stdint.h>
#include "stm32f4xx.h"

/*
 *
 *   FILAS (catodo, activo en LOW, con resistencia 330 ohm en serie):
 *     Fila 1 (pin fisico 9)  -> PA0
 *     Fila 2 (pin fisico 14) -> PA1
 *     Fila 3 (pin fisico 8)  -> PA2
 *     Fila 4 (pin fisico 12) -> PA3
 *     Fila 5 (pin fisico 1)  -> PA4
 *     Fila 6 (pin fisico 7)  -> PA5
 *     Fila 7 (pin fisico 2)  -> PA6
 *     Fila 8 (pin fisico 5)  -> PA7
 *
 *   COLUMNAS -Anodo
 *     Columna 1 (pin fisico 13) -> PE8
 *     Columna 2 (pin fisico 3)  -> PE9
 *     Columna 3 (pin fisico 4)  -> PE10
 *     Columna 4 (pin fisico 10) -> PE11
 *     Columna 5 (pin fisico 6)  -> PE12
 *     Columna 6 (pin fisico 11) -> PE13
 *     Columna 7 (pin fisico 15) -> PE14
 *     Columna 8 (pin fisico 16) -> PE15
 *
*/
/* ---------------- Variables globales ---------------- */
volatile uint32_t ms_ticks = 0;
volatile char tecla_pulsada = '\0';
volatile uint8_t matrix_buffer[8] = {0}; // 1 byte por fila, bit=1 -> LED encendido

/* ---------------- Configuracion de la clave ---------------- */
#define CLAVE_LONGITUD 4
const char CLAVE_CORRECTA[CLAVE_LONGITUD] = {'0','6','0','9'};
char clave_ingresada[CLAVE_LONGITUD];
uint8_t clave_index = 0;

/* ---------------- Maquina de estados del teclado (antirrebote) ---------------- */
typedef enum {
    STATE_IDLE,
    STATE_DEBOUNCE_PRESS,
    STATE_PRESSED,
    STATE_DEBOUNCE_RELEASE
} KeypadState_t;

KeypadState_t current_state = STATE_IDLE;
uint32_t debounce_timer = 0;
#define DEBOUNCE_DELAY_MS 20
char last_valid_key = '\0';

/* ---------------- Animaciones (frames de 8 bytes = matriz 8x8) ----------------
 * Cada frame: bit0 = columna 1 ... bit7 = columna 8 (orden logico,
 * la traduccion al pin fisico la hace Matrix_Refresh automaticamente).
 * Si al probar ves la figura "espejeada", invierte el orden de los
 * bits dentro de cada byte del frame.
 * ------------------------------------------------------------------------- */

const uint8_t IMAGES[16][8] = {
  { // 0
    0b00111100,
	0b01000110,
	0b01011010,
	0b01011010,
	0b01011010,
	0b01011010,
	0b01000010,
	0b00111100
  },
  { // 1
    0b00100010,
    0b01100110,
    0b10101010,
    0b00100010,
    0b11111111,
    0b01100110,
	0b00100010,
    0b01110111
  },
  { // 2
		  0b00000001,
		  0b01111110,
		  0b01000110,
		  0b01001010,
		  0b01010010,
		  0b01100010,
		  0b01111110,
		  0b10000000
  },
  { // 3
		  0b00000000,
		  0b01111110,
		  0b01000100,
		  0b00001000,
		  0b00011000,
		  0b00001000,
		  0b01000100,
		  0b01111110
  },
  { // 4
		  0b01111110,
		  0b10001101,
		  0b10010101,
		  0b10100101,
		  0b11000101,
		  0b11111111,
		  0b10000101,
		  0b01111110
  },
  { // 5
		  0b00000000,
		  0b01111110,
		  0b01000010,
	      0b01000000,
	      0b01111110,
   	      0b00000010,
		  0b00000010,
		  0b01111110
  },
  { // 6
		  0b01111100,
		  0b01000100,
		  0b01000000,
		  0b01000000,
		  0b01111100,
		  0b01000100,
		  0b01000100,
		  0b01111100
  },
  { // 7
		  0b00000000,
		  0b11111110,
		  0b11000110,
		  0b11001100,
		  0b00011000,
		  0b00110000,
		  0b01100000,
		  0b11000000
  },
  { // 8
    0b00000000,
    0b01111110,
    0b01100110,
    0b01100110,
    0b00011000,
    0b01100110,
    0b01100110,
    0b01111110
  },
  { // 9
		  0b01111110,
		  0b01000110,
		  0b01000110,
		  0b01111110,
		  0b00001100,
		  0b00011000,
		  0b00110000,
		  0b01100000
  },
  { // A
		  0b00001000,
		  0b00010100,
		  0b00100010,
		  0b01000001,
		  0b01111111,
		  0b01111111,
		  0b01000001,
		  0b01000001
  },
  { // B
		  0b00111100,
		  0b01000100,
		  0b01001000,
		  0b01010000,
		  0b01010000,
		  0b01001000,
		  0b01000100,
		  0b01111100
  },
  { // C
		  0b00111100,
		  0b01000010,
		  0b01000010,
		  0b01000000,
		  0b01000000,
		  0b01000010,
		  0b01000010,
		  0b00111100
  },
  { // D
		  0b00111000,
		  0b01000100,
		  0b01000010,
		  0b01000010,
		  0b01000010,
		  0b01000010,
		  0b01000010,
		  0b00111100
  },
  { // *
		  0b10011001,
		  0b01011010,
		  0b00111100,
		  0b11111111,
		  0b11111111,
		  0b00111100,
		  0b01011010,
		  0b10011001
  },
  { // #
		  0b00101000,
		  0b00010000,
		  0b10101000,
		  0b01000101,
		  0b10100010,
		  0b00010101,
		  0b00001000,
		  0b00010100
  }
};

// Imagen de ESPERA: un candado. Indica que el sistema esta listo para recibir
// la clave; se muestra al encender, al soltar cada tecla y tras ACCESO/ERROR.
const uint8_t IMG_VACIA[8] = {
		0b00000000,
		0b00011000,
	    0b00100100,
	    0b00100100,
		0b01111110,
		0b01000010,
		0b01011010,
		0b01000010
};

// Traduce el caracter de la tecla al indice dentro de IMAGES[]. -1 si no aplica.
int8_t Obtener_Indice_Imagen(char tecla) {
    if (tecla >= '0' && tecla <= '9') return tecla - '0';       // 0-9  -> indices 0-9
    switch (tecla) {
        case 'A': return 10;
        case 'B': return 11;
        case 'C': return 12;
        case 'D': return 13;
        case '*': return 14;
        case '#': return 15;
        default:  return -1;
    }
}

// Imagen de ACCESO PERMITIDO: una carita feliz (clave correcta)
const uint8_t IMG_OK[8] = {
		0b00000000,
		0b00100100,
		0b00100100,
		0b00100100,
		0b00100100,
		0b10000001,
		0b01000010,
		0b00111100
};

// Imagen de ERROR / ACCESO DENEGADO: una carita triste (clave incorrecta)
const uint8_t IMG_ERROR[8] = {
		0b00000000,
		0b00100100,
		0b00100100,
		0b00100100,
		0b00000000,
		0b00111100,
		0b01000010,
		0b10000001
};

/* ---------------- Reproductor de animaciones (NO bloqueante) ---------------- */
typedef struct {
    const uint8_t (*frames)[8];
    uint8_t total_frames;
    uint8_t frame_actual;
    uint32_t frame_delay_ms;
    uint32_t ultimo_cambio;
    uint8_t activa;
} Animacion_t;

Animacion_t anim_actual = {0};

void Animacion_Iniciar(const uint8_t frames[][8], uint8_t total, uint32_t delay_ms) {
    anim_actual.frames = frames;
    anim_actual.total_frames = total;
    anim_actual.frame_actual = 0;
    anim_actual.frame_delay_ms = delay_ms;
    anim_actual.ultimo_cambio = ms_ticks;
    anim_actual.activa = 1;

    // Mostrar el primer cuadro de inmediato (no esperar al primer retardo)
    for (int i = 0; i < 8; i++) {
        matrix_buffer[i] = frames[0][i];
    }
}

// Muestra una imagen fija (no animada) de inmediato, cancelando cualquier
// animacion de varios cuadros que estuviera en curso.
void Mostrar_Imagen_Estatica(const uint8_t img[8]) {
    anim_actual.activa = 0;
    for (int i = 0; i < 8; i++) matrix_buffer[i] = img[i];
}

void Animacion_Actualizar(void) {
    if (!anim_actual.activa) return;

    if ((ms_ticks - anim_actual.ultimo_cambio) >= anim_actual.frame_delay_ms) {
        anim_actual.ultimo_cambio = ms_ticks;
        anim_actual.frame_actual++;

        if (anim_actual.frame_actual >= anim_actual.total_frames) {
            anim_actual.activa = 0; // termino: se queda mostrando el ultimo cuadro
        } else {
            for (int i = 0; i < 8; i++) {
                matrix_buffer[i] = anim_actual.frames[anim_actual.frame_actual][i];
            }
        }
    }
}

/* ---------------- Inicializacion de la matriz LED 8x8 ---------------- */

// Mascaras precalculadas segun el pinout real:
//   Filas   -> PA0-PA7  (8 bits consecutivos, bit0..bit7 de GPIOA)
//   Columnas-> PE8-PE15 (8 bits consecutivos, bit8..bit15 de GPIOE)
#define ROWS_MASK 0x00FFU // PA0-PA7
#define COLS_MASK 0xFF00U // PE8-PE15

void Matrix_Init(void) {
    RCC->AHB1ENR |= (1 << 0) | (1 << 4); // Reloj para GPIOA (bit0) y GPIOE (bit4)

    // Filas: PA0-PA7 como salida
    GPIOA->MODER &= ~(0x0000FFFFU);
    GPIOA->MODER |=  0x00005555U;

    // Columnas: PE8-PE15 como salida
    GPIOE->MODER &= ~(0xFFFF0000U);
    GPIOE->MODER |=  0x55550000U;

    // Filas en HIGH (catodo inactivo = LED apagado)
    GPIOA->BSRR = ROWS_MASK;
    // Columnas en LOW (anodo inactivo = LED apagado)
    GPIOE->BSRR = ((uint32_t)COLS_MASK << 16);
}

// Se llama cada 1ms desde SysTick_Handler -> multiplexado de la matriz
void Matrix_Refresh(void) {
    static uint8_t fila_actual = 0;

    // 1. Apagar todo antes de pasar a la siguiente fila
    GPIOA->BSRR = ROWS_MASK;                     // filas en HIGH (inactivas)
    GPIOE->BSRR = ((uint32_t)COLS_MASK << 16);   // columnas en LOW (inactivas)

    // 2. Encender columnas de la fila actual: mapeo directo (columna i -> PE(8+i)),
    //    ya no hace falta partir el byte en dos grupos.
    uint8_t datos = matrix_buffer[fila_actual];
    GPIOE->BSRR = ((uint32_t)datos) << 8; // columnas en HIGH = encendidas (anodo activo)

    // 3. Activar (LOW) la fila actual: PA0 + fila_actual (0..7)
    GPIOA->BSRR = (1U << (fila_actual + 16));

    fila_actual = (fila_actual + 1) & 0x07; // 0..7
}

/* ---------------- Teclado 4x4 ---------------- */
void Keypad_Init(void) {
    RCC->AHB1ENR |= (1 << 3); // Reloj GPIOD (bit 3)

    GPIOD->MODER &= ~(0x0000FFFF);
    GPIOD->MODER |=  0x00000055; // PD0-PD3 salida (01), PD4-PD7 entrada (00 ya lo es)

    GPIOD->PUPDR &= ~(0x0000FF00);
    GPIOD->PUPDR |=  0x00005500; // Pull-up en PD4-PD7
}

char Keypad_Scan(void) {
    const char keys[4][4] = {
        {'1','2','3','A'},
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
    };

    for (int row = 0; row < 4; row++) {
        GPIOD->BSRR = 0x000F;
        GPIOD->BSRR = (1 << (row + 16));

        for (volatile int i = 0; i < 50; i++); // pequeno retardo de estabilizacion

        uint16_t cols = (GPIOD->IDR >> 4) & 0x0F;

        if (!(cols & 0x01)) return keys[row][0];
        if (!(cols & 0x02)) return keys[row][1];
        if (!(cols & 0x04)) return keys[row][2];
        if (!(cols & 0x08)) return keys[row][3];
    }
    return '\0';
}

/* ---------------- SysTick: temporizador de 1ms ---------------- */
void SysTick_Init(void) {
    SysTick->LOAD = 16000 - 1; // Para reloj de 16 MHz -> 1ms
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Handler(void) {
    ms_ticks++;
    Matrix_Refresh(); // multiplexado de la matriz, cada 1ms se refresca una fila
}

/* ---------------- Eventos del teclado (desacopla el driver de la logica) ---------------- */
typedef enum {
    KEY_EVENT_NONE,
    KEY_EVENT_PRESSED,   // se confirmo una pulsacion (antirrebote superado)
    KEY_EVENT_RELEASED   // se confirmo que la tecla fue soltada
} KeyEventType_t;

typedef struct {
    KeyEventType_t type;
    char key;
} KeyEvent_t;

/* ---------------- Escaneo del teclado con antirrebote (no bloqueante) ----------------
 * Devuelve un KeyEvent_t: la FSM del teclado NO conoce nada sobre la
 * clave ni sobre la matriz, solo informa "se presiono" / "se solto".
 * ------------------------------------------------------------------------------- */
KeyEvent_t Keypad_Task(void) {
    KeyEvent_t evento = { KEY_EVENT_NONE, '\0' };
    char tecla_actual = Keypad_Scan();

    switch (current_state) {
        case STATE_IDLE:
            if (tecla_actual != '\0') {
                last_valid_key = tecla_actual;
                debounce_timer = ms_ticks;
                current_state = STATE_DEBOUNCE_PRESS;
            }
            break;

        case STATE_DEBOUNCE_PRESS:
            if (tecla_actual == '\0') {
                current_state = STATE_IDLE; // fue ruido, no una pulsacion real
            } else if ((ms_ticks - debounce_timer) >= DEBOUNCE_DELAY_MS) {
                tecla_pulsada = last_valid_key;
                evento.type = KEY_EVENT_PRESSED;
                evento.key = last_valid_key;
                current_state = STATE_PRESSED;
            }
            break;

        case STATE_PRESSED:
            if (tecla_actual == '\0') {
                debounce_timer = ms_ticks;
                current_state = STATE_DEBOUNCE_RELEASE;
            }
            break;

        case STATE_DEBOUNCE_RELEASE:
            if (tecla_actual != '\0') {
                current_state = STATE_PRESSED; // seguia presionada
            } else if ((ms_ticks - debounce_timer) >= DEBOUNCE_DELAY_MS) {
                evento.type = KEY_EVENT_RELEASED;
                evento.key = last_valid_key;
                current_state = STATE_IDLE; // liberacion confirmada
            }
            break;
    }
    return evento;
}

/* ---------------- FSM de la aplicacion (ESPERA / INGRESANDO / VALIDANDO / ACCESO / ERROR) ----------------
 * Esta es la maquina de estados del "Sistema / Contrasena" del diagrama
 * de arquitectura. Solo se comunica con el resto mediante el KeyEvent_t
 * que recibe y las funciones Mostrar_Imagen_*() / Animacion_Iniciar().
 * ------------------------------------------------------------------------------- */
typedef enum {
    SYS_ESPERA,
    SYS_INGRESANDO,
    SYS_VALIDANDO,
    SYS_ACCESO,
    SYS_ERROR
} SystemState_t;

SystemState_t sistema_estado = SYS_ESPERA;
uint32_t resultado_timer = 0;
#define RESULTADO_DURACION_MS 3000 // ACCESO/ERROR visibles ~3s (no bloqueante)

void Sistema_FSM_Update(KeyEvent_t evento) {
    switch (sistema_estado) {

        case SYS_ESPERA:
        case SYS_INGRESANDO:
            if (evento.type == KEY_EVENT_PRESSED) {
                // Estado 2 - INGRESANDO: mostrar la imagen de la tecla detectada
                int8_t idx = Obtener_Indice_Imagen(evento.key);
                if (idx >= 0) Mostrar_Imagen_Estatica(IMAGES[idx]);

                if (evento.key >= '0' && evento.key <= '9' && clave_index < CLAVE_LONGITUD) {
                    clave_ingresada[clave_index++] = evento.key;
                }
                sistema_estado = SYS_INGRESANDO;

            } else if (evento.type == KEY_EVENT_RELEASED) {
                if (clave_index >= CLAVE_LONGITUD) {
                    sistema_estado = SYS_VALIDANDO; // 4 digitos completos -> validar
                } else {
                    Mostrar_Imagen_Estatica(IMG_VACIA); // al soltar la tecla vuelve el candado (espera)
                }
            }
            break;

        case SYS_VALIDANDO: {
            uint8_t correcta = 1;
            for (int i = 0; i < CLAVE_LONGITUD; i++) {
                if (clave_ingresada[i] != CLAVE_CORRECTA[i]) { correcta = 0; break; }
            }

            if (correcta) {
                Mostrar_Imagen_Estatica(IMG_OK);
                sistema_estado = SYS_ACCESO;
            } else {
                Mostrar_Imagen_Estatica(IMG_ERROR);
                sistema_estado = SYS_ERROR;
            }
            resultado_timer = ms_ticks;
            clave_index = 0; // listo para el proximo intento
            break;
        }

        case SYS_ACCESO:
        case SYS_ERROR:
            // Se queda mostrando ACCESO/ERROR exactamente RESULTADO_DURACION_MS,
            // sin bloquear: la matriz y el teclado se siguen refrescando normalmente.
            if ((ms_ticks - resultado_timer) >= RESULTADO_DURACION_MS) {
                Mostrar_Imagen_Estatica(IMG_VACIA); // pasados 3 s vuelve el candado (espera)
                sistema_estado = SYS_ESPERA;
            }
            break;
    }
}

/* ---------------- Programa principal (Maquina de estados MAESTRA) ---------------- */
int main(void) {
    Keypad_Init();
    Matrix_Init();
    SysTick_Init();

    Mostrar_Imagen_Estatica(IMG_VACIA); // Estado 1 - ESPERA: inicia mostrando el candado

    while (1) {
        KeyEvent_t evento = Keypad_Task();  // FSM Teclado: detecta pulsacion/liberacion
        Sistema_FSM_Update(evento);         // FSM Sistema: ESPERA/INGRESANDO/VALIDANDO/ACCESO/ERROR
        Animacion_Actualizar();             // FSM Matriz LED: avanza animaciones de varios cuadros (si hay alguna activa)
    }
}