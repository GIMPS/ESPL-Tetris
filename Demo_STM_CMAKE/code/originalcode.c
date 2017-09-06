/**
 * This is the main file of the ESPLaboratory Demo project.
 * It implements simple sample functions for the usage of UART,
 * writing to the display and processing user inputs.
 *
 * @author: Jonathan Müller-Boruttau,
 * 			Tobias Fuchs tobias.fuchs@tum.de
 * 			Nadja Peters nadja.peters@tum.de (RCS, TUM)
 *
 */
#include "includes.h"

// start and stop bytes for the UART protocol
static const uint8_t startByte = 0xAA,
					 stopByte  = 0x55;

static const uint16_t displaySizeX = 320,
					  displaySizeY = 240;

QueueHandle_t ESPL_RxQueue; // Already defined in ESPL_Functions.h
SemaphoreHandle_t ESPL_DisplayReady;

// Stores lines to be drawn
QueueHandle_t JoystickQueue;

void checkJoystick() {
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	struct coord joystickPosition = {0, 0};
	const TickType_t tickFramerate = 20;

	while (TRUE) {
		// Remember last joystick values
		joystickPosition.x =
					(uint8_t) (ADC_GetConversionValue(ESPL_ADC_Joystick_2) >> 4);
		joystickPosition.y = (uint8_t) 255 -
						 (ADC_GetConversionValue(ESPL_ADC_Joystick_1) >> 4);

		xQueueSend(JoystickQueue, &joystickPosition, 100);

		// Execute every 20 Ticks
		vTaskDelayUntil(&xLastWakeTime, tickFramerate);
	}
}

/**
 * Example function to send data over UART
 *
 * Sends coordinates of a given position via UART.
 * Structure of a package:
 *  8 bit start byte
 *  8 bit x-coordinate
 *  8 bit y-coordinate
 *  8 bit checksum (= x-coord XOR y-coord)
 *  8 bit stop byte
 */


int main() {
	// Initialize Board functions and graphics
	ESPL_SystemInit();

	// Initializes Draw Queue with 100 lines buffer
	JoystickQueue = xQueueCreate(100, 2 * sizeof(char));

	// Initializes Tasks with their respective priority
	xTaskCreate(drawTask, "drawTask", 1000, NULL, 4, NULL);
	xTaskCreate(checkJoystick, "checkJoystick", 1000, NULL, 3, NULL);

	// Start FreeRTOS Scheduler
	vTaskStartScheduler();
}

/**
 * Example task which draws to the display.
 */
void drawTask() {
	char str[100]; // buffer for messages to draw to display
	struct coord joystickPosition; // joystick queue input buffer

	font_t font1; // Load font for ugfx
	font1 = gdispOpenFont("DejaVuSans24*");

	/* building the cave:
	   caveX and caveY define the top left corner of the cave
	    circle movment is limited by 64px from center in every direction
	    (coordinates are stored as uint8_t unsigned bytes)
	    so, cave size is 128px */
	const uint16_t caveX    = displaySizeX/2 - UINT8_MAX/4,
				   caveY    = displaySizeY/2 - UINT8_MAX/4,
				   caveSize = UINT8_MAX/2;
	uint16_t circlePositionX = caveX,
			 circlePositionY = caveY;

	// Start endless loop
	while(TRUE) {
		// wait for buffer swap
		while(xQueueReceive(JoystickQueue, &joystickPosition, 0) == pdTRUE)
			;

		// Clear background
		gdispClear(Red);
		// Draw rectangle "cave" for circle
		// By default, the circle should be in the center of the display.
		// Also, the circle can only move by 127px in both directions (position is limited to uint8_t)
		gdispFillArea(caveX-10, caveY-10, caveSize + 20, caveSize + 20, Red);
		// color inner white
		gdispFillArea(caveX, caveY, caveSize, caveSize, White);

		// Generate string with current joystick values
		sprintf( str, "Axis 1: %5d|Axis 2: %5d|VBat: %5d",
				 ADC_GetConversionValue(ESPL_ADC_Joystick_1),
				 ADC_GetConversionValue(ESPL_ADC_Joystick_2),
				 ADC_GetConversionValue(ESPL_ADC_VBat) );
		// Print string of joystick values
		gdispDrawString(0, 0, str, font1, Black);

		// Generate string with current joystick values
		sprintf( str, "A: %d|B: %d|C %d|D: %d|E: %d|K: %d",
				 GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A),
				 GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B),
				 GPIO_ReadInputDataBit(ESPL_Register_Button_C, ESPL_Pin_Button_C),
				 GPIO_ReadInputDataBit(ESPL_Register_Button_D, ESPL_Pin_Button_D),
				 GPIO_ReadInputDataBit(ESPL_Register_Button_E, ESPL_Pin_Button_E),
				 GPIO_ReadInputDataBit(ESPL_Register_Button_K, ESPL_Pin_Button_K) );
		// Print string of joystick values
		gdispDrawString(0, 11, str, font1, Black);

		// Draw Circle in center of square, add joystick movement
		circlePositionX = caveX + joystickPosition.x/2;
		circlePositionY = caveY + joystickPosition.y/2;
		gdispFillCircle(circlePositionX, circlePositionY, 10, Green);

		// Wait for display to stop writing
		xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);
		// swap buffers
		ESPL_DrawLayer();
	}
}

/**
 * This task polls the joystick value every 20 ticks
 */

void sendPosition(struct coord position) {
	const uint8_t checksum = position.x^position.y;

	UART_SendData(startByte);
	UART_SendData(position.x);
	UART_SendData(position.y);
	UART_SendData(checksum);
	UART_SendData(stopByte);
}

/**
 * Example how to receive data over UART (see protocol above)
 */
void uartReceive() {
	char input;
	uint8_t pos = 0;
	char checksum;
	char buffer[5]; // Start byte,4* line byte, checksum (all xor), End byte
	struct coord position = {0, 0};
	while (TRUE) {
		// wait for data in queue
		xQueueReceive(ESPL_RxQueue, &input, portMAX_DELAY);

		// decode package by buffer position
		switch(pos) {
		// start byte
		case 0:
			if(input != startByte)
				break;
		case 1:
		case 2:
		case 3:
			// read received data in buffer
			buffer[pos] = input;
			pos++;
			break;
		case 4:
			// Check if package is corrupted
			checksum = buffer[1]^buffer[2];
			if(input == stopByte || checksum == buffer[3]) {
				// pass position to Joystick Queue
				position.x = buffer[1];
				position.y = buffer[2];
				xQueueSend(JoystickQueue, &position, 100);
			}
			pos = 0;
		}
	}
}

/*
 *  Hook definitions needed for FreeRTOS to function.
 */
void vApplicationIdleHook() {
	while (TRUE) {
	};
}

void vApplicationMallocFailedHook() {
	while(TRUE) {
	};
}

