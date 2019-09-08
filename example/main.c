#include <stm32f10x.h>
#include <stdbool.h>
#include <svc.h>
#include <stdlib.h>
#include <st7789.h>


typedef int float_t;
#define FLOAT_BITS 14
#define FLOAT_FACT ((float_t)1 << FLOAT_BITS)
#define MANDELBROT_MAXITER 1024

typedef int float_fast_t;
#define FLOAT_FAST_BITS 13
#define FLOAT_FAST_FACT ((float_t)1 << FLOAT_FAST_BITS)
#define MANDELBROT_MAXITER_FAST 64


#define PIXEL_BUFFER_LINES 20
#define PIXEL_BUFFER_SIZE (ST7789_LCD_WIDTH * PIXEL_BUFFER_LINES)


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


void setupTimer(void) {
	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	TIM1->PSC = 0x3200 - 1;
	// TIM1->CR1 |= TIM_CR1_ARPE;
	TIM1->CR1 |= TIM_CR1_CEN;
}


void svcLogTime() {
	static char buf[12];
	itoa(TIM1->CNT / 10, buf, 10);
	char *bufPtr = buf;
	while (*bufPtr != 0) {
		bufPtr++;
	}
	*bufPtr = '\n';
	bufPtr++;
	*bufPtr = '\0';
	svcWrite0(buf);
}


void svcLogTimeReset() {
	TIM1->CNT = 0;
}


void svcWriteNumber(int number) {
	static char buf[12];
	itoa(number, buf, 10);
	char *bufPtr = buf;
	while (*bufPtr != 0) {
		bufPtr++;
	}
	*bufPtr = '\n';
	bufPtr++;
	*bufPtr = '\0';
	svcWrite0(buf);
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


void demoCycleColors(void) {
	for (uint8_t color = 0; color < 248; color += 8) {
		st7789_Clear(st7789_RGBToColor(color, color, color));
	}
	for (uint8_t color = 248; color > 0; color -= 8) {
		st7789_Clear(st7789_RGBToColor(255, color, color));
	}
	for (uint8_t color = 0; color < 248; color += 8) {
		st7789_Clear(st7789_RGBToColor(255, color, 0));
	}
	for (uint8_t color = 248; color > 0; color -= 8) {
		st7789_Clear(st7789_RGBToColor(color, 255, 0));
	}
	for (uint8_t color = 0; color < 248; color += 8) {
		st7789_Clear(st7789_RGBToColor(0, 255, color));
	}
	for (uint8_t color = 248; color > 0; color -= 8) {
		st7789_Clear(st7789_RGBToColor(0, color, 255));
	}
	for (uint8_t color = 248; color > 0; color -= 8) {
		st7789_Clear(st7789_RGBToColor(0, 0, color));
	}
}


void demoCheckboardDisplay(uint16_t checkboardSize, uint16_t startX, uint16_t startY) {
	st7789_StartMemoryWrite();
	bool xPolarity = (startX / checkboardSize) & 1;
	bool yPolarity = (startY / checkboardSize) & 1;
	bool initialXPolarity = (startX / checkboardSize) & 1;
	startX = startX % checkboardSize;
	startY = startY % checkboardSize;
	uint16_t lineBuffer[ST7789_LCD_WIDTH * 2];
	uint16_t *buf = (uint16_t *)(&lineBuffer);
	uint16_t xCounter = startX;
	uint16_t yCounter = startY;

	for (uint16_t line = 0; line < ST7789_LCD_HEIGHT + 1; ++line) {
		uint16_t *frontBuffer = ((line & 1 )== 0) ? (buf + ST7789_LCD_WIDTH) : buf;
		uint16_t *backBuffer = ((line & 1) == 0) ? buf : (buf + ST7789_LCD_WIDTH);
		xPolarity = initialXPolarity;
		if (line > 0) {
			st7789_WriteDMA(frontBuffer, ST7789_LCD_WIDTH * 2);
		}
		if (line < ST7789_LCD_HEIGHT) {
			xCounter = startX;
			for (uint16_t column = 0; column < ST7789_LCD_WIDTH; ++column) {
				backBuffer[column] = (xPolarity ^ yPolarity) ? 0xffff : st7789_RGBToColor(line, column, 255 - line);
				xCounter++;
				if (xCounter == checkboardSize) {
					xPolarity = !xPolarity;
					xCounter = 0;
				}
			}
		}
		if (line > 0) {
			st7789_WaitForDMA();
		}
		yCounter++;
		if (yCounter == checkboardSize) {
			yPolarity = !yPolarity;
			yCounter = 0;
		}
	}
}


void demoCheckboard(void) {
	uint16_t size = 1;
	uint16_t posX = 0;
	uint16_t posY = 0;
	for (size_t i = 0; i < 50; ++i) {
		size++;
		demoCheckboardDisplay(size, posX, posY);
	}
	for (size_t i = 0; i < 50; ++i) {
		posX += 3;
		demoCheckboardDisplay(size, posX, posY);
	}
	for (size_t i = 0; i < 50; ++i) {
		posX += 3;
		posY += i / 10;
		demoCheckboardDisplay(size, posX, posY);
	}
	for (size_t i = 0; i < 100; ++i) {
		posX += 3 + i / 25;
		posY += 5 + i / 10;
		demoCheckboardDisplay(size, posX, posY);
	}
	for (size_t i = 0; i < 20; ++i) {
		posX += 7;
		posY += 15;
		demoCheckboardDisplay(size, posX, posY);
	}
	for (size_t i = 0; i < 100; ++i) {
		posX += 7 * (100 - i) / 100;
		posY += 15 * (100 - i) / 100;
		demoCheckboardDisplay(size, posX, posY);
	}
}


uint16_t demoMandelbrotCalculateFast(float_fast_t realInit, float_fast_t imagInit, uint16_t maxiter) {
	float_fast_t realq, imagq, real, imag;
	uint16_t iter;

	real = realInit;
	imag = imagInit;
	for (iter = 0; iter < maxiter; ++iter) {
		realq = (real * real) >> FLOAT_FAST_BITS;
		imagq = (imag * imag) >> FLOAT_FAST_BITS;
		if ((realq + imagq) > (float_fast_t) 4 * FLOAT_FAST_FACT) {
			break;
		}
		imag = ((real * imag) >> (FLOAT_FAST_BITS - 1)) + imagInit;
		real = realq - imagq + realInit;
	}
	return iter;
}


void demoMandelbrotDisplayFast(float_fast_t realmin, float_fast_t imagmin, float_fast_t realmax, float_fast_t imagmax, uint16_t maxiter) {
	st7789_StartMemoryWrite();
	uint16_t lineBuffer[ST7789_LCD_WIDTH * 2];
	uint16_t *buf = (uint16_t *)(&lineBuffer);
	uint16_t colormap[MANDELBROT_MAXITER_FAST];
	for (size_t i = 0; i < maxiter; ++i) {
		colormap[i] = st7789_RGBToColor(
			(maxiter - i - 1) * 256 / maxiter,
			i * 256 / maxiter,
			0
		);
	}

	float_fast_t stepReal, stepImag, real, imag;
	uint8_t column, line;

	stepReal = (realmax - realmin) / ST7789_LCD_WIDTH;
	stepImag = (imagmax - imagmin) / ST7789_LCD_HEIGHT;

	imag = imagmax;
	for (line = 0; line < ST7789_LCD_HEIGHT; ++line) {
		uint16_t *frontBuffer = ((line & 1 )== 0) ? (buf + ST7789_LCD_WIDTH) : buf;
		uint16_t *backBuffer = ((line & 1) == 0) ? buf : (buf + ST7789_LCD_WIDTH);
		if (line > 0) {
			st7789_WriteDMA(frontBuffer, ST7789_LCD_WIDTH * 2);
		}

		real = realmin;
		for (column = 0; column < ST7789_LCD_WIDTH; ++column) {
			uint16_t color = demoMandelbrotCalculateFast(real, imag, maxiter);
			if (color == maxiter) {
				color = 0;
			}
			else {
				color = colormap[color];
			}
			backBuffer[column] = color;
			real += stepReal;
		}
		imag -= stepImag;

		if (line > 0) {
			st7789_WaitForDMA();
		}
		if (line == ST7789_LCD_HEIGHT - 1) {
			st7789_WriteDMA(backBuffer, ST7789_LCD_WIDTH * 2);
			st7789_WaitForDMA();
		}
	}
}


uint16_t demoMandelbrotCalculate(float_t realInit, float_t imagInit) {
	float_t realq, imagq, real, imag;
	uint16_t iter;

	real = realInit;
	imag = imagInit;
	for (iter = 0; iter < MANDELBROT_MAXITER; ++iter) {
		realq = (real * real) >> FLOAT_BITS;
		imagq = (imag * imag) >> FLOAT_BITS;
		if ((realq + imagq) > (float_t) 4 * FLOAT_FACT) {
			break;
		}
		imag = ((real * imag) >> (FLOAT_BITS - 1)) + imagInit;
		real = realq - imagq + realInit;
	}
	return iter;
}


void demoMandelbrotDisplay(float_t realmin, float_t imagmin, float_t realmax, float_t imagmax) {
	st7789_StartMemoryWrite();
	uint16_t lineBuffer[ST7789_LCD_WIDTH * 2];
	uint16_t *buf = (uint16_t *)(&lineBuffer);
	uint16_t colormap[MANDELBROT_MAXITER];
	for (size_t i = 0; i < MANDELBROT_MAXITER; ++i) {
		colormap[i] = st7789_RGBToColor(
			(MANDELBROT_MAXITER - i - 1) * 256 / MANDELBROT_MAXITER,
			(i * 16) * 256 / MANDELBROT_MAXITER,
			(i * 4) * 256 / MANDELBROT_MAXITER
		);
	}

	float_t stepReal, stepImag, real, imag;
	uint8_t column, line;

	stepReal = (realmax - realmin) / ST7789_LCD_WIDTH;
	stepImag = (imagmax - imagmin) / ST7789_LCD_HEIGHT;

	imag = imagmax;
	for (line = 0; line < ST7789_LCD_HEIGHT; ++line) {
		uint16_t *frontBuffer = ((line & 1 )== 0) ? (buf + ST7789_LCD_WIDTH) : buf;
		uint16_t *backBuffer = ((line & 1) == 0) ? buf : (buf + ST7789_LCD_WIDTH);
		if (line > 0) {
			st7789_WriteDMA(frontBuffer, ST7789_LCD_WIDTH * 2);
		}

		real = realmin;
		for (column = 0; column < ST7789_LCD_WIDTH; ++column) {
			uint16_t color = demoMandelbrotCalculate(real, imag);
			if (color == MANDELBROT_MAXITER) {
				color = 0;
			}
			else {
				color = colormap[color];
			}
			backBuffer[column] = color;
			real += stepReal;
		}
		imag -= stepImag;

		if (line > 0) {
			st7789_WaitForDMA();
		}
		if (line == ST7789_LCD_HEIGHT - 1) {
			st7789_WriteDMA(backBuffer, ST7789_LCD_WIDTH * 2);
			st7789_WaitForDMA();
		}
	}
}


void demoMandelbrot() {
	float realMin = -2.0;
	float imagMin = -1.35;
	float realMax = 0.7;
	float imagMax = 1.35;

	const float realFinalMin = -0.75;
	const float imagFinalMin = 0.12;
	const float realFinalMax = -0.73;
	const float imagFinalMax = 0.14;

	for (size_t i = 8; i < 24; ++i) {
		demoMandelbrotDisplayFast(realMin * FLOAT_FAST_FACT, imagMin * FLOAT_FAST_FACT, realMax * FLOAT_FAST_FACT, imagMax * FLOAT_FAST_FACT, i);
		st7789_WaitNanosecs(20000);
	}
	for (size_t i = 0; i < 24; ++i) {
		realMin = (realMin * 9 + realFinalMin) / 10;
		realMax = (realMax * 9 + realFinalMax) / 10;
		imagMin = (imagMin * 9 + imagFinalMin) / 10;
		imagMax = (imagMax * 9 + imagFinalMax) / 10;
		demoMandelbrotDisplayFast(realMin * FLOAT_FAST_FACT, imagMin * FLOAT_FAST_FACT, realMax * FLOAT_FAST_FACT, imagMax * FLOAT_FAST_FACT, 32);
	}

	demoMandelbrotDisplay(realFinalMin * FLOAT_FACT, imagFinalMin * FLOAT_FACT, realFinalMax * FLOAT_FACT, imagFinalMax * FLOAT_FACT);
	st7789_WaitNanosecs(1000000);
}


void demoPixmap() {
	uint16_t line = 0;
	uint16_t batch = 0;
	uint16_t pixels[PIXEL_BUFFER_SIZE];

	st7789_Clear(0x0000);
	st7789_StartMemoryWrite();
	while (line < ST7789_LCD_HEIGHT) {
		pixels[0] = batch;
		pixels[1] = PIXEL_BUFFER_SIZE;
		svcCall(0xff, &pixels);
		st7789_WriteDMA(&pixels, PIXEL_BUFFER_SIZE * 2);
		st7789_WaitForDMA();
		line = line + PIXEL_BUFFER_LINES;
		batch += 1;
	}
	st7789_WaitNanosecs(2000000);
}


int main(void) {
	setupPrescaler(16);
	setupTimer();

	st7789_GPIOInit();
	st7789_Reset();
	st7789_Init_1_3_LCD();

	for(;;) {
		demoCycleColors();
		demoCheckboard();
		demoMandelbrot();
		demoPixmap();
	}
	return 0;
}
