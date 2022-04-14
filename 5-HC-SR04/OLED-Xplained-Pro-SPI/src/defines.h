
#ifndef DEFINES_H_
#define DEFINES_H_

// Pino echo verde:
#define ECHO_PIO PIOA
#define ECHO_PIO_ID ID_PIOA
#define ECHO_PIO_IDX 6
#define ECHO_PIO_IDX_MASK (1 << ECHO_PIO_IDX)

// Pino trig amarelo:
#define TRIG_PIO PIOD
#define TRIG_PIO_ID ID_PIOD
#define TRIG_PIO_IDX 30
#define TRIG_PIO_IDX_MASK (1 << TRIG_PIO_IDX)

//Botão OLED:
#define BUT_PIO1				PIOD
#define BUT_PIO1_ID				ID_PIOD
#define BUT_PIO1_IDX			28
#define BUT_PIO_1_IDX_MASK		(1u << BUT_PIO1_IDX)

// Volatile:
volatile char echo_flag = 0;
volatile float freq = (float) 1/(0.000058*2);
double pulses;
volatile char but_flag = 0;
volatile char erro_flag = 0;

// RTT struct:
typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;

#endif /* DEFINES_H_ */