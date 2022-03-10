#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/* defines */
// LED placa principal
#define LED_PIO      PIOC
#define LED_PIO_ID   ID_PIOC
#define LED_IDX      8
#define LED_IDX_MASK (1 << LED_IDX)

// Botão placa principal
#define BUT_PIO      PIOA
#define BUT_PIO_ID   ID_PIOA
#define BUT_IDX  11
#define BUT_IDX_MASK (1 << BUT_IDX)

//OLED - extensão EXT1
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

#define BUT2_PIO PIOC
#define BUT2_PIO_ID ID_PIOC
#define BUT2_PIO_IDX 31
#define BUT2_PIO_IDX_MASK (1u << BUT2_PIO_IDX)

#define BUT3_PIO PIOA
#define BUT3_PIO_ID ID_PIOA
#define BUT3_PIO_IDX 19
#define BUT3_PIO_IDX_MASK (1u << BUT3_PIO_IDX)
// Flag
volatile char but1_flag;
volatile char but2_flag;
volatile char but3_flag;
volatile int n;
// Funções
void but1_callback(void)
{
	if (!pio_get(BUT1_PIO, PIO_INPUT,BUT1_PIO_IDX_MASK)){
		but1_flag = 1;
	} else{
		but1_flag = 0;
	}
	
}
void but2_callback(void)
{
	but2_flag = 1;
	
}
void pisca_led(int n, int t){
	for (int i=0;i<n;i++){
		pio_clear(LED1_PIO, LED1_PIO_IDX_MASK);
		delay_ms(t);
		pio_set(LED1_PIO, LED1_PIO_IDX_MASK);
		delay_ms(t);
	}
}
void io_init(void){
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
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT2_PIO, BUT2_PIO_IDX_MASK, 60);
	pio_handler_set(BUT2_PIO,BUT2_PIO_ID,	BUT2_PIO_IDX_MASK,PIO_IT_EDGE,but2_callback);
	pio_enable_interrupt(BUT2_PIO, BUT2_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT2_PIO);
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT2_PIO_ID, 4);
	pmc_enable_periph_clk(BUT3_PIO_ID);
	pio_configure(BUT3_PIO, PIO_INPUT, BUT3_PIO_IDX_MASK, PIO_PULLUP | PIO_DEBOUNCE);
	pio_set_debounce_filter(BUT3_PIO, BUT3_PIO_IDX_MASK, 60);
	pio_handler_set(BUT3_PIO,BUT3_PIO_ID,	BUT3_PIO_IDX_MASK,PIO_IT_EDGE,but1_callback);
	pio_enable_interrupt(BUT3_PIO, BUT3_PIO_IDX_MASK);
	pio_get_interrupt_status(BUT3_PIO);
	NVIC_EnableIRQ(BUT3_PIO_ID);
	NVIC_SetPriority(BUT3_PIO_ID, 4);
}
int main (void)
{
	board_init();
	sysclk_init();
	delay_init();
	WDT->WDT_MR = WDT_MR_WDDIS;
	io_init();
	gfx_mono_ssd1306_init();
	n = 200;
	
	
	while(1) {
		if (but1_flag){
			delay_ms(500);
			if (but1_flag){
				n+=100;
				pisca_led(30, n);
			}else{
				n-=100;
				pisca_led(30, n);
			}
			char frequencia[100];
			sprintf(frequencia, "freq: %d", n);
			gfx_mono_draw_string(frequencia, 5,16, &sysfont);
			but1_flag = 0;
			but2_flag = 0;
			but3_flag = 0;
		}
// 		/*if (but1_flag){
// 			while(!pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK)){
// 				if(but2_flag){
// 					pio_set(BUT1_PIO,BUT1_PIO_IDX_MASK);
// 					break;
// 				}
// 				for (int i = 0; i < 10000000; i++){
// 					if (pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK) && i < 1000000){
// 						n-=100; // Diminuindo a frequencia quando o toque é rapido
// 						pisca_led(30, n);
// 						break;
// 					} else if((!pio_get(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK) && i >= 1000000)){
// 						n+=100; // Aumentando a frequencia quando o toque é longo
// 						pisca_led(30, n);
// 					}
// 					
// 				}
// 				// Escrevendo a frequencia na tela:
// 				char frequencia[15];
// 				sprintf(frequencia, "freq: %d", n);
// 				gfx_mono_draw_string(frequencia, 5,16, &sysfont);
// 			}*/
			
			
		
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}
