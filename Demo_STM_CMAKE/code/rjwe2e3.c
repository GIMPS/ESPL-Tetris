/**
 *Exercise 2
 *Ren Jiawei
 *ga62xud
 *
 */
#include "includes.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

// start and stop bytes for the UART protocol
static const uint8_t startByte = 0xAA,
					 stopByte  = 0x55;

static const uint16_t displaySizeX = 320,
					  displaySizeY = 240;

QueueHandle_t ESPL_RxQueue; // Already defined in ESPL_Functions.h
SemaphoreHandle_t ESPL_DisplayReady;

// Stores lines to be drawn
QueueHandle_t JoystickQueue;

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}






/* Dimensions the buffer that the task being created will use as its stack.
NOTE:  This is the number of words the stack will hold, not the number of
bytes.  For example, if each stack item is 32-bits, and this is set to 100,
then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 200

/* Structure that will hold the TCB of the task being created. */
StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack.  Note this is
an array of StackType_t variables.  The size of StackType_t is dependent on
the RTOS port. */
StackType_t xStack[ STACK_SIZE ];


//Blinking Circle

int flag1=1,flag2=1;
//Dynamic task
void task1() {
	while(TRUE){
		flag1=1;
		vTaskDelay(1000);
	}
}
//Static task
void task2() {
	while(TRUE){
		flag2=1;
		vTaskDelay(500);
	}
}

//Button count

int cnt1=0,cnt2=0;

//Semaphore

SemaphoreHandle_t btnA_pressed;

void cnt_Sema(void *pvParameters){
	while(TRUE){
		if( xSemaphoreTake(  btnA_pressed, portMAX_DELAY == pdTRUE ))
		{
	    	cnt1++;
		}

	}


}
void cntA_task(void *pvParameters){
	int aprev=1;
	while(TRUE){
	if (GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A)==0&&aprev==1){
			xSemaphoreGive(  btnA_pressed);
			aprev=0;
				}
    else if (GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A)==1){
    	aprev=1;
	}
	vTaskDelay(50);
}
}



//Task Notification

TaskHandle_t btnB_pressed,btnB_cnted;
void cnt_Noti(){
	while(TRUE){
			ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		    cnt2++;
			xTaskNotifyGive(  btnB_cnted);
		}
}


void cntB_task(void *pvParameters){
	int bprev=1;
	while(TRUE){
	if (GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B)==0&&bprev==1){
		    xTaskNotifyGive(  btnB_pressed);
		    ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
			bprev=0;
				}
    else if (GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B)==1){
    	bprev=1;
	}
	vTaskDelay(50);
}
}


void reset_task(){
	cnt1=cnt2=0;
}

TimerHandle_t reset_timer;


//Cnt opposite button and recieve opposite joystick
int oppcnt1=0,oppcnt2=0;

void send_js(){
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

		sendPosition(joystickPosition);

		// Execute every 20 Ticks
		vTaskDelayUntil(&xLastWakeTime, tickFramerate);
	}
}


void receive_js(){
	char input;
	uint8_t pos = 0;
	char checksum1,checksum2;
	char buffer[8]; // Start byte,4* line byte, checksum (all xor), End byte
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
		case 4:
		case 5:
		case 6:
			// read received data in buffer
			buffer[pos] = input;
			pos++;
			break;
		case 7:
			// Check if package is corrupted
			checksum1 = buffer[1]^buffer[2];
			checksum2 = buffer[3]^buffer[4];
			if(input == stopByte || (checksum1 == buffer[5]&&checksum2 == buffer[6])) {
				// pass position to Joystick Queue
				position.x = buffer[1];
				position.y = buffer[2];
				xQueueSend(JoystickQueue, &position, 100);

				//Update opposite button peressed times
				oppcnt1=buffer[3];
				oppcnt2=buffer[4];

			}
			pos = 0;
		}
	}
}

int main() {
	// Initialize Board functions and graphics
	ESPL_SystemInit();

	btnA_pressed = xSemaphoreCreateBinary();
	// Initializes Draw Queue with 100 lines buffer
	JoystickQueue = xQueueCreate(100, 2 * sizeof(char));

	// Initializes Tasks with their respective priority
	xTaskCreate(drawTask, "drawTask", 1000, NULL, 2, NULL);
	//xTaskCreate(checkJoystick, "checkJoystick", 1000, NULL, 2, NULL);
	xTaskCreate(receive_js, "receive_js",1000,NULL,2,NULL);
	xTaskCreate(send_js, "send_js",1000,NULL,2,NULL);


	//Blinking Circle
	xTaskCreate(task1,"task1",STACK_SIZE,NULL,4,NULL);//Dynamic task
	xTaskCreateStatic(task2,"task2",STACK_SIZE,( void * ) 1, 3, xStack, &xTaskBuffer);//Static task

	//Semaphore
	xTaskCreate(cntA_task,"cntA_task",1000,NULL,1,NULL);
	xTaskCreate(cnt_Sema,"cnt_Sema",1000,NULL,1,NULL);
	//Task Notification
	xTaskCreate(cntB_task,"cntB_task",1000,NULL,1,&btnB_cnted);
	xTaskCreate(cnt_Noti,"cnt_Noti",1000,NULL,2,&btnB_pressed);
	//Reset timer
    reset_timer= xTimerCreate( "reset_timer",pdMS_TO_TICKS( 15000 ),pdTRUE,( void * ) 0,reset_task);
    xTimerStart(reset_timer, 0 );



	// Start FreeRTOS Scheduler
	vTaskStartScheduler();
}



/**
 * Example task which draws to the display.
 */

void drawTask() {

    // t is a counter controlling rotating
	double t=0;

	// cnt counts button pressed times
	int cnta=0,cntb=0,cntc=0,cntd=0;

	//stta,b,c,d shows previous state
	int stta=1,sttb=1,sttc=1,sttd=1;

	//x,yshift moves screen with joystick
	int xshift=0,yshift=0;

	const int radius=70;
	font_t font = gdispOpenFont("DejaVuSans32");

	char oppButt[100];
	char str[100];
	struct coord joystickPosition; // joystick queue input buffer

	int stickX,stickY;


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
		//sqpos and clpos will rotate with time varying
		point sqpos={140+radius*cos(t/3.14),105+radius*sin(t/3.14)};
		point clpos={160+radius*cos(t/3.14+3.14),125+radius*sin(t/3.14+3.14)};

		while(xQueueReceive(JoystickQueue, &joystickPosition, 0) == pdTRUE)
			;
		gdispClear(White);

		sprintf( str, "A: %d|B: %d|C %d|D: %d",cnta,cntb,cntc,cntd );
		// Print string of button pressed times

		stickX=displaySizeX/2+(joystickPosition.x-displaySizeX/2)/2,stickY=displaySizeY/2+(joystickPosition.y-displaySizeY/2)/2;


		//Detect debounce
		if(GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A)==0&&stta==1)
		{
			cnta++;
			stta=0;
		}
		else if(GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A)==1) stta=1;
		if(GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B)==0&&sttb==1)
		{
			cntb++;
			sttb=0;
		}
		else if(GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B)==1)sttb=1;
		if(GPIO_ReadInputDataBit(ESPL_Register_Button_C, ESPL_Pin_Button_C)==0&&sttc==1)
		{
			cntc++;
			sttc=0;
		}
		else if(GPIO_ReadInputDataBit(ESPL_Register_Button_C, ESPL_Pin_Button_C)==1) sttc=1;
		if(GPIO_ReadInputDataBit(ESPL_Register_Button_D, ESPL_Pin_Button_D)==0&&sttd==1)
		{
			cntd++;
			sttd=0;
		}
		else if(GPIO_ReadInputDataBit(ESPL_Register_Button_D, ESPL_Pin_Button_D)==1) sttd=1;

		//Reset
		if(GPIO_ReadInputDataBit(ESPL_Register_Button_E, ESPL_Pin_Button_E)==0)
		{
			cnta=0;cntb=0;cntc=0;cntd=0;
		}


		//Make shift according to joystick position
		yshift=(joystickPosition.y-displaySizeY/2)/2;
		xshift=(joystickPosition.x-displaySizeX/2)/2;

		gdispFillArea(xshift+sqpos.x,yshift+sqpos.y,40,40,Red);
		//draw triangle
		gdispDrawLine(xshift+160,yshift+100,xshift+140,yshift+140,Black);
		gdispDrawLine(xshift+140,yshift+140,xshift+180,yshift+140,Black);
		gdispDrawLine(xshift+160,yshift+100,xshift+180,yshift+140,Black);

		gdispFillCircle(xshift+clpos.x,yshift+clpos.y,20,Blue);

		gdispDrawString(xshift+50, yshift+50, str, font, Purple);
		gdispDrawString(xshift+130, yshift+220, "Fixed String",font , Blue);
		gdispDrawString(xshift+130+cos(t/3.14)*80, yshift+10, "Moving String",font , Blue);

		//Exercise 2,1

		if(flag1){
			gdispFillCircle(stickX,stickY-60,20,Black);
			flag1=0;
		}
		if(flag2){
			gdispFillCircle(stickX,stickY+60,20,Red);
			flag2=0;
		}

		sprintf(oppButt, "Opp A: %d Opp B:%d",oppcnt1,oppcnt2);
		gdispDrawString(60+stickX, 60+stickY,oppButt, font , Purple);

		xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);
		ESPL_DrawLayer();

		vTaskDelay(20);
		t+=0.1;

	}
}


void sendPosition(struct coord position) {
	const uint8_t checksum1 = position.x^position.y;
	const uint8_t checksum2 = cnt1^cnt2;
	UART_SendData(startByte);
	UART_SendData(position.x);
	UART_SendData(position.y);
	UART_SendData(cnt1);
	UART_SendData(cnt2);
	UART_SendData(checksum1);
	UART_SendData(checksum2);
	UART_SendData(stopByte);
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
