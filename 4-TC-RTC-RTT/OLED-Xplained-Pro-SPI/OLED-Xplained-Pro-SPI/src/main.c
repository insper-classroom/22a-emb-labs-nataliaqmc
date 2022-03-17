#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

//Defines placa:
#define LED_PIO PIOC
#define LED_PIO_ID ID_PIOC
#define LED_PIO_IDX 8
#define LED_PIO_IDX_MASK (1 << LED_PIO_IDX)

//OLED
#define LED1_PIO PIOA
#define LED1_PIO_ID ID_PIOA
#define LED1_PIO_IDX 0
#define LED1_PIO_IDX_MASK (1 << LED1_PIO_IDX)

#define LED2_PIO PIOC
#define LED2_PIO_ID ID_PIOC
#define LED2_PIO_IDX 30
#define LED2_PIO_IDX_MASK (1 << LED2_PIO_IDX)

#define LED3_PIO PIOB
#define LED3_PIO_ID ID_PIOB
#define LED3_PIO_IDX 2
#define LED3_PIO_IDX_MASK (1 << LED3_PIO_IDX)

#define BUT1_PIO PIOD
#define BUT1_PIO_ID ID_PIOD
#define BUT1_PIO_IDX 28
#define BUT1_PIO_IDX_MASK (1u << BUT1_PIO_IDX)

//Variaveis globais:
volatile char flag_tc = 0;
volatile char flag_rtc_alarm = 0;
volatile char flag_rtc_count = 0;
volatile char flag_rtt_alarm = 0;
volatile char but1_flag;

//Struct do calendario:
typedef struct  {
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;

//Inicializando as funções:
void LED_init(int estado);
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type);
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource);
void io_init(void);
void pin_toggle(Pio *pio, uint32_t mask);
void pisca_led(int n, int t);
void escrita(void);

void but1_callback(void)
{
	if (!pio_get(BUT1_PIO, PIO_INPUT,BUT1_PIO_IDX_MASK)){
		but1_flag = 1;
		} else{
		but1_flag = 0;
	}
	
}


void TC0_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 0);
	pin_toggle(LED_PIO, LED_PIO_IDX_MASK);
}
void TC1_Handler(void) {
	volatile uint32_t status = tc_get_status(TC0, 1);
	pin_toggle(LED1_PIO, LED1_PIO_IDX_MASK);  
}
void RTT_Handler(void) {
	uint32_t ul_status;
	ul_status = rtt_get_status(RTT);
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		flag_rtt_alarm = 1;
	}
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
	}

}
void RTC_Handler(void) {
	uint32_t ul_status = rtc_get_status(RTC);
	
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		flag_rtc_count = 1;
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		flag_rtc_alarm = 1;
	}

	rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}

void LED_init(int estado) {
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_set_output(LED1_PIO, LED1_PIO_IDX_MASK, estado, 0, 0);
};
void LED2_init(int estado) {
	pmc_enable_periph_clk(LED2_PIO_ID);
	pio_set_output(LED2_PIO, LED2_PIO_IDX_MASK, estado, 0, 0);
};
void LED3_init(int estado) {
	pmc_enable_periph_clk(LED3_PIO_ID);
	pio_set_output(LED3_PIO, LED3_PIO_IDX_MASK, estado, 0, 0);
};
/**
* @Brief Inverte o valor do pino 0->1/ 1->0
*/
void pin_toggle(Pio *pio, uint32_t mask) {
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}

void pisca_led (int n, int t) {
	for (int i=0;i<n;i++){
		pio_clear(LED3_PIO, LED3_PIO_IDX_MASK);
		delay_ms(t);
		pio_set(LED3_PIO, LED3_PIO_IDX_MASK);
		delay_ms(t);
	}
}
static float get_time_rtt(){
	uint ul_previous_time = rtt_read_timer_value(RTT);
}
static void RTT_init(float freqPrescale, uint32_t IrqNPulses, uint32_t rttIRQSource) {

	uint16_t pllPreScale = (int) (((float) 32768) / freqPrescale);
	
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);
	
	if (rttIRQSource & RTT_MR_ALMIEN) {
		uint32_t ul_previous_time;
		ul_previous_time = rtt_read_timer_value(RTT);
		while (ul_previous_time == rtt_read_timer_value(RTT));
		rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);
	}

	/* config NVIC */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 4);
	NVIC_EnableIRQ(RTT_IRQn);

	/* Enable RTT interrupt */
	if (rttIRQSource & (RTT_MR_RTTINCIEN | RTT_MR_ALMIEN))
	rtt_enable_interrupt(RTT, rttIRQSource);
	else
	rtt_disable_interrupt(RTT, RTT_MR_RTTINCIEN | RTT_MR_ALMIEN);
	
}
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	/* Configura o PMC */
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  freq hz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura NVIC*/
	NVIC_SetPriority(ID_TC, 4);
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);
}
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type) {
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 4);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
}
void io_init(void){
	pmc_enable_periph_clk(LED2_PIO_ID);
	pio_configure(LED2_PIO, PIO_OUTPUT_1, LED2_PIO_IDX_MASK, PIO_DEFAULT);
	// Configurando botao e led:
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_configure(LED1_PIO, PIO_OUTPUT_0, LED1_PIO_IDX_MASK, PIO_DEFAULT);
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT1_PIO, BUT1_PIO_IDX_MASK, 60);
	pio_handler_set(BUT1_PIO,BUT1_PIO_ID,	BUT1_PIO_IDX_MASK,PIO_IT_EDGE,but1_callback);
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT1_PIO);
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4);
}
void escrita(void) {
	uint32_t now_hour, now_min, now_sec;
	rtc_get_time(RTC, &now_hour, &now_min, &now_sec);
	char time_display[12];
	sprintf(time_display, "%2d:%2d:%2d", now_hour, now_min, now_sec);
	gfx_mono_draw_string(time_display, 25, 16, &sysfont);
}


int main (void)
{
	board_init();
	sysclk_init();
	io_init();
	delay_init();
	
  // Init OLED
	gfx_mono_ssd1306_init();
	WDT->WDT_MR = WDT_MR_WDDIS;
	//LED PLACA:
	TC_init(TC0, ID_TC0, 0, 5);
	tc_start(TC0, 0);
	//LED 1:
	LED_init(1);
	TC_init(TC0, ID_TC1, 1, 4);
	tc_start(TC0, 1);
	//LED 2:
	LED2_init(1);
	RTT_init(4, 4, RTT_MR_ALMIEN); 
	//LED 3:
	LED3_init(1);
	
	
	/** Configura RTC */
	calendar rtc_initial = {2022, 3, 17, 13, 13, 50 ,10};
	RTC_init(RTC, ID_RTC, rtc_initial, RTC_IER_ALREN);
	
	
  /* Insert application code here, after the board has been initialized. */
	while(1) {
		if(but1_flag){
			uint32_t current_hour, current_min, current_sec;
			uint32_t current_year, current_month, current_day, current_week;
			rtc_get_time(RTC, &current_hour, &current_min, &current_sec);
			rtc_get_date(RTC, &current_year, &current_month, &current_day, &current_week);
			rtc_set_date_alarm(RTC, 1, current_month, 1, current_day);
			if (current_sec + 20 >= 60) {
				rtc_set_time_alarm(RTC, 1, current_hour, 1, current_min+1, 1, current_sec + 20 - 60);
			}
			else {
				rtc_set_time_alarm(RTC, 1, current_hour, 1, current_min, 1, current_sec + 20);
			}
		}
		if (flag_rtc_alarm) {
			pin_toggle(LED3_PIO, LED3_PIO_IDX_MASK);
			delay_ms(100);
			pin_toggle(LED3_PIO, LED3_PIO_IDX_MASK);
		}
		if (flag_rtc_count) {
			escrita();
		}
		if (flag_rtt_alarm) {
			pin_toggle(LED2_PIO, LED2_PIO_IDX_MASK);
			RTT_init(1, 4, RTT_MR_ALMIEN);
		}
		but1_flag = 0;
		flag_rtc_alarm = 0;
		flag_rtc_count = 0;
		flag_rtt_alarm = 0;
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
		
		
			
	}
}
