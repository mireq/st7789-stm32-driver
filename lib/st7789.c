#include <stddef.h>
#include <stm32f10x.h>
#include <svc.h>

#include "st7789.h"


#define LCD_FILL_BUFFER_SIZE 64


// Weak attribute to allow override
void __attribute__((weak)) st7789_WaitNanosecs(uint32_t ns) {
	int ctr = ((ST7789_PRESCALER * ST7789_OSC_MHZ) * ns / 6);
	while (ctr) {
		__asm__ volatile ("nop");
		--ctr;
	}
}


void st7789_Reset(void) {
	ST7789_RST_PORT->ODR &= ~ST7789_RST_PIN;
	st7789_WaitNanosecs(10000); // Reset pulse time
	ST7789_RST_PORT->ODR |= ST7789_RST_PIN;
	st7789_WaitNanosecs(120000); // Maximum time of blanking sequence
}


void st7789_StartCommand(void) {
	//st7789_WaitNanosecs(10); //  D/CX setup time
	ST7789_DC_PORT->ODR &= ~ST7789_DC_PIN;
}


void st7789_StartData(void) {
	//st7789_WaitNanosecs(10); //  D/CX setup time
	ST7789_DC_PORT->ODR |= ST7789_DC_PIN;
}


void st7789_WriteSpi(uint8_t data) {
	for (int32_t i = 0; i<10000; i++) {
		if (ST7789_SPI->SR & SPI_SR_TXE) break;
	}
	ST7789_SPI->DR = data;
	while (ST7789_SPI->SR & SPI_SR_BSY);
}


void st7789_ReadSpi(uint8_t *data, size_t length) {
	// Disable SPI output
	ST7789_SPI->CR1 &= ~(SPI_CR1_BIDIOE);
	uint8_t dummy = 0;

	/*
	clockPulse(); // ???
	GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF5));
	for (size_t i = 0; i < 1; ++i) {
		st7789_WaitNanosecs(10);
		GPIOA->ODR |= GPIO_ODR_ODR5;
		st7789_WaitNanosecs(10);
	}
	GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF5)) | (GPIO_CRL_CNF5_1);
	*/

	while (length--) {
		while (!(ST7789_SPI->SR & SPI_SR_TXE));
		ST7789_SPI->DR = dummy;
		while (!(ST7789_SPI->SR & SPI_SR_RXNE));
		*data++ = ST7789_SPI->DR;
	}
	while (ST7789_SPI->SR & SPI_SR_BSY);

	// Enable SPI output
	ST7789_SPI->CR1 |= SPI_CR1_BIDIOE;
}


void __attribute__((weak)) st7789_WriteDMA(void *data, uint16_t length) {
	ST7789_DMA->CCR =  (DMA_CCR1_MINC | DMA_CCR1_DIR); // Memory increment, direction to peripherial
	ST7789_DMA->CMAR  = (uint32_t)data; // Source address
	ST7789_DMA->CPAR  = (uint32_t)&ST7789_SPI->DR; // Destination address
	ST7789_DMA->CNDTR = length;
	ST7789_SPI->CR1 &= ~(SPI_CR1_SPE);  // Disable SPI
	ST7789_SPI->CR2 |= SPI_CR2_TXDMAEN; // Enable DMA transfer
	ST7789_SPI->CR1 |= SPI_CR1_SPE;     // Enable SPI
	ST7789_DMA->CCR |= DMA_CCR1_EN;     // Start DMA transfer
}


void st7789_WaitForDMA(void) {
	while(ST7789_DMA->CNDTR);
}

void st7789_ReadCommand(uint8_t command, void *data, size_t length) {
	st7789_StartCommand();
	st7789_WriteSpi(command);
	st7789_StartData();
	st7789_ReadSpi((void *)data, length);
}


void st7789_WriteCommand(uint8_t command, const void *data, size_t length) {
	st7789_StartCommand();
	st7789_WriteSpi(command);
	st7789_StartData();
	for (size_t i = 0; i < length; ++i) {
		st7789_WriteSpi(((const uint8_t *)data)[i]);
	}
}


void st7789_RunCommand(const st7789_Command *command) {
	st7789_StartCommand();
	st7789_WriteSpi(command->command);
	if (command->dataSize > 0) {
		st7789_StartData();
		for (uint8_t i = 0; i < command->dataSize; ++i) {
			st7789_WriteSpi(command->data[i]);
		}
	}
	if (command->waitMs > 0) {
		st7789_WaitNanosecs(command->waitMs * 1000);
	}
}


void st7789_RunCommands(const st7789_Command *sequence) {
	while (sequence->command != ST7789_CMDLIST_END) {
		st7789_RunCommand(sequence);
		sequence++;
	}
}


void st7789_Init_1_3_LCD(void) {
	// Resolution
	const uint8_t caset[4] = {
		0x00,
		0x00,
		(ST7789_LCD_WIDTH - 1) >> 8,
		(ST7789_LCD_WIDTH - 1) & 0xff
	};
	const uint8_t raset[4] = {
		0x00,
		0x00,
		(ST7789_LCD_HEIGHT - 1) >> 8,
		(ST7789_LCD_HEIGHT - 1) & 0xff
	};
	const st7789_Command initSequence[] = {
		// Sleep
		{ST7789_CMD_SLPIN, 10, 0, NULL},                    // Sleep
		{ST7789_CMD_SWRESET, 200, 0, NULL},                 // Reset
		{ST7789_CMD_SLPOUT, 120, 0, NULL},                  // Sleep out
		{ST7789_CMD_MADCTL, 0, 1, (const uint8_t *)"\x00"}, // Page / column address order
		{ST7789_CMD_COLMOD, 0, 1, (const uint8_t *)"\x55"}, // 16 bit RGB
		{ST7789_CMD_INVON, 0, 0, NULL},                     // Inversion on
		{ST7789_CMD_CASET, 0, 4, (const uint8_t *)&caset},  // Set width
		{ST7789_CMD_RASET, 0, 4, (const uint8_t *)&raset},  // Set height
		// Porch setting
		{ST7789_CMD_PORCTRL, 0, 5, (const uint8_t *)"\x0c\x0c\x00\x33\x33"},
		// Set VGH to 13.26V and VGL to -10.43V
		{ST7789_CMD_GCTRL, 0, 1, (const uint8_t *)"\x35"},
		// Set VCOM to 1.675V
		{ST7789_CMD_VCOMS, 0, 1, (const uint8_t *)"\x1f"},
		// LCM control
		{ST7789_CMD_LCMCTRL, 0, 1, (const uint8_t *)"\x2c"},
		// VDV/VRH command enable
		{ST7789_CMD_VDVVRHEN, 0, 2, (const uint8_t *)"\x01\xc3"},
		// VDV set to default value
		{ST7789_CMD_VDVSET, 0, 1, (const uint8_t *)"\x20"},
		 // Set frame rate to 60Hz
		{ST7789_CMD_FRCTR2, 0, 1, (const uint8_t *)"\x0f"},
		// Set VDS to 2.3V, AVCL to -4.8V and AVDD to 6.8V
		{ST7789_CMD_PWCTRL1, 0, 2, (const uint8_t *)"\xa4\xa1"},
		// Gamma corection
		//{ST7789_CMD_PVGAMCTRL, 0, 14, (const uint8_t *)"\xd0\x08\x11\x08\x0c\x15\x39\x33\x50\x36\x13\x14\x29\x2d"},
		//{ST7789_CMD_NVGAMCTRL, 0, 14, (const uint8_t *)"\xd0\x08\x10\x08\x06\x06\x39\x44\x51\x0b\x16\x14\x2f\x31"},
		// Little endian
		{ST7789_CMD_RAMCTRL, 0, 2, (const uint8_t *)"\x00\x08"},
		{ST7789_CMDLIST_END, 0, 0, NULL},                   // End of commands
	};
	st7789_RunCommands(initSequence);
	st7789_Clear(0x0000);
	const st7789_Command initSequence2[] = {
		{ST7789_CMD_DISPON, 100, 0, NULL},                  // Display on
		{ST7789_CMD_SLPOUT, 100, 0, NULL},                  // Sleep out
		{ST7789_CMD_TEON, 0, 0, NULL},                      // Tearing line effect on
		{ST7789_CMDLIST_END, 0, 0, NULL},                   // End of commands
	};
	st7789_RunCommands(initSequence2);
}


void st7789_StartMemoryWrite(void) {
	st7789_StartCommand();
	st7789_WriteSpi(ST7789_CMD_RAMWR);
	st7789_StartData();
}


void st7789_SetWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd) {
	uint8_t caset[4];
	uint8_t raset[4];
	caset[0] = (uint8_t)(xStart >> 8);
	caset[1] = (uint8_t)(xStart & 0xff);
	caset[2] = (uint8_t)(xEnd >> 8);
	caset[3] = (uint8_t)(xEnd & 0xff);
	raset[0] = (uint8_t)(yStart >> 8);
	raset[1] = (uint8_t)(yStart & 0xff);
	raset[2] = (uint8_t)(yEnd >> 8);
	raset[3] = (uint8_t)(yEnd & 0xff);
	st7789_Command sequence[] = {
		{ST7789_CMD_CASET, 0, 4, caset},
		{ST7789_CMD_RASET, 0, 4, raset},
		{ST7789_CMD_RAMWR, 0, 0, NULL},
		{ST7789_CMDLIST_END, 0, 0, NULL},
	};
	st7789_StartCommand();
	st7789_RunCommands(sequence);
	st7789_StartData();
}


void st7789_FillArea(uint16_t color, uint16_t startX, uint16_t startY, uint16_t width, uint16_t height) {
	uint16_t buf[LCD_FILL_BUFFER_SIZE];
	for (size_t i = 0; i < LCD_FILL_BUFFER_SIZE; i++) {
		buf[i] = color;
	}
	st7789_SetWindow(startX, startY, startX + width - 1, startY + height - 1);
	size_t bytestToWrite = width * height * 2;
	uint16_t transferSize = (uint16_t)LCD_FILL_BUFFER_SIZE * 2;
	while (bytestToWrite > 0) {
		if (bytestToWrite < transferSize) {
			transferSize = bytestToWrite;
		}
		bytestToWrite -= transferSize;
		st7789_WriteDMA(&buf, transferSize);
		st7789_WaitForDMA();
	}
}


void st7789_Clear(uint16_t color) {
	st7789_FillArea(color, 0, 0, ST7789_LCD_WIDTH, ST7789_LCD_HEIGHT);
}


uint16_t st7789_RGBToColor(uint8_t r, uint8_t g, uint8_t b) {
	return (((uint16_t)r >> 3) << 11) | (((uint16_t)g >> 2) << 5) | ((uint16_t)b >> 3);
}
