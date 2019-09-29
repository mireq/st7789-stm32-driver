#include <stdbool.h>
#include <stdlib.h>

#include <st7789.h>
#include <stm32f10x.h>
#include <svc.h>
#include <ugui.h>


void setupPrescaler(int pllmul) {
	// enable high speed external oscillator
	RCC->CR = (RCC->CR & ~RCC_CR_HSEBYP) | RCC_CR_HSEON;
	while (!(RCC->CR & RCC_CR_HSERDY)) {
		;
	}

	RCC->CFGR = (pllmul-2) << 18 | RCC_CFGR_PLLSRC | RCC_CFGR_PPRE1_DIV2;
	RCC->CR |= RCC_CR_PLLON;

	while (!(RCC->CR & RCC_CR_PLLRDY)) {
		;
	}

	if (RCC->APB2ENR & RCC_APB2ENR_USART1EN) {
		// if usart1 is clocked wait to drain possible debugging infos.
		while ((USART1->SR & (USART_SR_TXE | USART_SR_TC)) != (USART_SR_TXE | USART_SR_TC)) {
			;
		}
	}

	// switch sysclock to pll
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	// wait for ack
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
		;
	}
}


void st7789_GPIOInit(void) {
	// Enable GPIOA and SPI1 clock
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;
	// Enable DMA1 channel
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;

	// Enable SPI
	ST7789_SPI->SR = 0;
	// Reverse polarity?
	ST7789_SPI->CR1 = \
		SPI_CR1_SSM | \
		SPI_CR1_SSI | \
		SPI_CR1_MSTR | \
		SPI_CR1_CPOL | \
		SPI_CR1_BIDIMODE | \
		SPI_CR1_BIDIOE;
	ST7789_SPI->CR1 |= SPI_CR1_SPE;
	
	// DC and RST signals
	// Maximum output speed
	ST7789_DC_PORT->CRH |= GPIO_CRH_MODE8;
	ST7789_RST_PORT->CRH |= GPIO_CRH_MODE9;
	// Output push pull
	ST7789_DC_PORT->CRH &= ~(GPIO_CRH_CNF8);
	ST7789_RST_PORT->CRH &= ~(GPIO_CRH_CNF9);

	// SPI pins
	// Maximum output speed on PA5/PA7
	GPIOA->CRL |= GPIO_CRL_MODE5;
	GPIOA->CRL |= GPIO_CRL_MODE7;
	// Alternate mode on PA5/PA7
	GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF5)) | (GPIO_CRL_CNF5_1);
	GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF7)) | (GPIO_CRL_CNF7_1);
}


void UG_Driver_PushPixel(UG_COLOR color) {
	st7789_WriteDMA(&color, 2);
	st7789_WaitForDMA();
}


void UG_Driver_SetPixel(UG_S16 x, UG_S16 y, UG_COLOR color) {
	st7789_SetWindow(x, y, x, y);
	st7789_WriteDMA(&color, 2);
	st7789_WaitForDMA();
	st7789_SetWindow(0, 0, ST7789_LCD_WIDTH, ST7789_LCD_HEIGHT);
}


UG_RESULT UG_Driver_FillFrame(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR color) {
	st7789_FillArea(color, x1, y1, x2, y2);
	return UG_RESULT_OK;
}


void(*UG_Driver_FillArea(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2))(UG_COLOR) {
	st7789_SetWindow(x1, y1, x2, y2);
	return UG_Driver_PushPixel;
}


int main(void) {
	setupPrescaler(16);

	st7789_GPIOInit();
	st7789_Reset();
	st7789_Init_1_3_LCD();

	UG_GUI gui;
	UG_Init(&gui, UG_Driver_SetPixel, ST7789_LCD_WIDTH, ST7789_LCD_HEIGHT);
	UG_SelectGUI(&gui);

	//UG_DriverRegister(DRIVER_DRAW_LINE, (void *) UG_Driver_DrawLine);
	UG_DriverRegister(DRIVER_FILL_FRAME, (void *) UG_Driver_FillFrame);
	UG_DriverRegister(DRIVER_FILL_AREA, (void *) UG_Driver_FillArea);
	//UG_DriverEnable(DRIVER_DRAW_LINE);
	UG_DriverEnable(DRIVER_FILL_FRAME);
	UG_DriverEnable(DRIVER_FILL_AREA);

	UG_FillFrame(0, 0, ST7789_LCD_WIDTH, ST7789_LCD_HEIGHT, C_BLACK);
	/* Draw text with uGUI */
	UG_FontSelect(&FONT_8X14);
	UG_ConsoleSetArea(0, 0, ST7789_LCD_WIDTH, ST7789_LCD_HEIGHT);
	UG_ConsoleSetBackcolor(C_BLACK);
	UG_ConsoleSetForecolor(C_RED);
	UG_ConsolePutString("Beginning System Initialization...\n");
	UG_ConsoleSetForecolor(C_GREEN);
	UG_ConsolePutString("System Initialization Complete\n");

	for(;;) {
	}
	return 0;
}
