/*
 * Final Project of Lab ESPL
 * Author: Ren Jiawei, Su junxin
 * Date: 12.6.2017
 */

#include "includes.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include <time.h>
#include <stdlib.h>
//--------------------------------------------------START-------------------------------------------------------------------

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
 implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
		StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
	/* If the buffers to be provided to the Idle task are declared inside this
	 function then they must be declared static - otherwise they will be allocated on
	 the stack and so not exists after this function exits. */
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

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
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
		StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
	/* If the buffers to be provided to the Timer task are declared inside this
	 function then they must be declared static - otherwise they will be allocated on
	 the stack and so not exists after this function exits. */
	static StaticTask_t xTimerTaskTCB;
	static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

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
//-------------------------------------------------END--------------------------------------------------------------
static const uint8_t startByte = 0xAA, stopByte = 0x55;
QueueHandle_t ESPL_RxQueue; // Already defined in ESPL_Functions.h
SemaphoreHandle_t ESPL_DisplayReady;
#define maxLevel 4
#define maxNumOfTetris 4
#define speed_coefficient_single 200
#define speed_coefficient_double 250
//Game constants
static const int wall = -1;
static const int arrayHeight = 24, arrayWidth = 12;
static const int displayLeft = 1, displayRight = 11, displayUp = 3,
		displayDown = 23;
static const int maxRefreshPeriod = 100000;
static const uint16_t displaySizeX = 320, displaySizeY = 240;
int oppA = 0, oppB = 0, oppC = 0, oppD = 0, oppE = 0;
//Game Variable
int game_level;
int roundTime = 100;
int game_score = 0;
int tot_cleared_lines;
int start_game_level = 0;
int high_score[5] = { 0, 0, 0, 0, 0 };
int player_rank = 4;
int isGameOver;
int oppAState = 1, oppBState = 1, oppCState = 1, oppDState = 1, oppEState = 1;
int score_rule[maxNumOfTetris][maxLevel] = { 40, 80, 120, 160, 100, 200, 300,
		400, 300, 600, 900, 1200, 1200, 2500, 3600, 4800 };
int speed_coefficient = 400;
int gameoverColor = 0;
int currentx = 0, currenty = 0, currentnumber = 0, currentcolor = 0,
		nextnumber = 0, nextcolor = 0;
int opp_currentx = 0, opp_currenty = 0, opp_currentnumber = 0,
		opp_currentcolor = 0, opp_nextnumber = 0, opp_nextcolor = 0;
int isConnected = 0;
int fps = 0, fps_temp = 0;
typedef struct tetrisObj tetrisObj;
typedef enum direction direction;
typedef enum key key;
typedef enum gameState gameState;
typedef enum gameMode gameMode;
int myState = -1, oppState = -1;
QueueHandle_t JoystickQueue;
/*Describe the move direction of the tetris object*/
enum direction {
	down, left, right
};

/*Describe the detected signal.
 * Some of them are sent from clicked button(bntA,B,C,D,E).
 * The key sysRefresh is sent automatically by the system.*/
enum key {
	bntA, bntB, bntC, bntD, bntE, sysRefresh
};

/*Describe the different game states of the game. */
enum gameState {
	startScene,
	selectControl,
	gameInit,
	inGameRound,
	endGameRound,
	pause,
	gameOver
};

/*Describe the different gamemode of the game.*/
/*Describe the different gamemode of the game.*/
enum gameMode {
	startingSelect,
	singlePlayer,
	doublePlayer_Select,
	doublePlayer_Rotate,
	doublePlayer_Move
};

color_t color[5] = { White, Red, Yellow, Blue, Green };	//The array stores the color of the tetris block

key publicKey = bntB;
gameMode game_mode = startingSelect;
SemaphoreHandle_t inputReceived;

/*
 * Init opposite button state to 0
 */
void initOppBtn() {
	oppA = 0;
	oppB = 0;
	oppC = 0;
	oppD = 0;
	oppE = 0;
}

void sendPosition() {
	int AState, BState, CState, DState, EState;
	AState = GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A);
	BState = GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B);
	CState = GPIO_ReadInputDataBit(ESPL_Register_Button_C, ESPL_Pin_Button_C);
	DState = GPIO_ReadInputDataBit(ESPL_Register_Button_D, ESPL_Pin_Button_D);
	EState = GPIO_ReadInputDataBit(ESPL_Register_Button_E, ESPL_Pin_Button_E);
	const uint8_t checksum1 = AState ^ BState;
	const uint8_t checksum2 = CState ^ DState;
	UART_SendData(startByte);	//0
	UART_SendData(AState);	//1
	UART_SendData(BState);	//2
	UART_SendData(CState);	//3
	UART_SendData(DState);	//4
	UART_SendData(checksum1);	//5
	UART_SendData(checksum2);	//6
	UART_SendData(currentx);	//7
	UART_SendData(currenty);	//8
	UART_SendData(currentnumber);	//9
	UART_SendData(currentcolor);	//10
	UART_SendData(nextnumber);	//11
	UART_SendData(nextcolor);	//12
	UART_SendData(myState);	//13
	UART_SendData(EState);	//14
	UART_SendData(stopByte);	//15
}
/*
 * Send button, current tetris, next tetris and game state to the remote device
 */
void send_btn() {
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	const TickType_t tickFramerate = 10;
	while (TRUE) {
		sendPosition();

		// Execute every 20 Ticks
		vTaskDelayUntil(&xLastWakeTime, tickFramerate);
	}
}
/*
 * Receive button, current tetris, next tetris and game state from the remote device
 */
void receive_data() {
	char input;
	uint8_t pos = 0;
	char checksum1, checksum2;
	char buffer[14]; // Start byte, opposite A button state, checksign (copy of data), End byte
	while (TRUE) {
		// wait for data in queue
		xQueueReceive(ESPL_RxQueue, &input, portMAX_DELAY);
		// decode package by buffer position
		switch (pos) {
		// start byte
		case 0:
			if (input != startByte)
				break;
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
			// read received data in buffer
			buffer[pos] = input;
			pos++;
			break;
		case 15:
			// Check if package is corrupted
			checksum1 = buffer[5];
			checksum2 = buffer[6];
			if (input == stopByte && checksum1 == buffer[1] ^ buffer[2]
					&& checksum2 == buffer[3] ^ buffer[4]) {
				oppAState = buffer[1];
				oppBState = buffer[2];
				oppCState = buffer[3];
				oppDState = buffer[4];
				opp_currentx = buffer[7];
				opp_currenty = buffer[8];
				opp_currentnumber = buffer[9];
				opp_currentcolor = buffer[10];
				opp_nextnumber = buffer[11];
				opp_nextcolor = buffer[12];
				oppState = buffer[13];
				oppEState = buffer[14];
				//xQueueSend(JoystickQueue, &opp_sysrefresh_private, 100);
			}
			pos = 0;
		}
	}
}
/*
 * This task will revceive player button input and system refresh signal from both local device and remote device.
 */
void user_input() {

	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	const TickType_t tickFramerate = 20;

	// Detect debounce of buttons
	int oppAprev = 1, oppBprev = 1, oppCprev = 1, oppDprev = 1, oppEprev = 1;
	int Aprev = 1, Bprev = 1, Cprev = 1, Dprev = 1, Eprev = 1;
	int roundNumber = 0, connectionError = 0, errorFuse = 0;
	while (TRUE) {
		//Receive buttons. Which button to receive depends on game mode and game state
		if (game_mode == singlePlayer || game_mode == doublePlayer_Rotate
				|| game_mode == doublePlayer_Select
				|| game_mode == startingSelect
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (GPIO_ReadInputDataBit(ESPL_Register_Button_A, ESPL_Pin_Button_A)
					== 0 && Aprev == 1) {
				publicKey = bntA;
				xSemaphoreGive(inputReceived);
				Aprev = 0;
			} else if (GPIO_ReadInputDataBit(ESPL_Register_Button_A,
			ESPL_Pin_Button_A) == 1) {
				Aprev = 1;
			}
		}

		if (game_mode == doublePlayer_Move || game_mode == doublePlayer_Select
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (oppAState == 0 && oppAprev == 1) {
				publicKey = bntA;
				oppA = 1;
				xSemaphoreGive(inputReceived);
				oppAprev = 0;
			} else if (oppAState == 1) {
				oppAprev = 1;
			}
		}

		if (game_mode == singlePlayer || game_mode == doublePlayer_Move
				|| game_mode == doublePlayer_Select
				|| game_mode == startingSelect
				|| (game_mode == doublePlayer_Rotate && myState == (int) pause)
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (GPIO_ReadInputDataBit(ESPL_Register_Button_B, ESPL_Pin_Button_B)
					== 0 && Bprev == 1) {
				publicKey = bntB;
				xSemaphoreGive(inputReceived);
				Bprev = 0;
			} else if (GPIO_ReadInputDataBit(ESPL_Register_Button_B,
			ESPL_Pin_Button_B) == 1) {
				Bprev = 1;
			}

		}

		if (game_mode == doublePlayer_Rotate || game_mode == doublePlayer_Select
				|| game_mode == startingSelect
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (oppBState == 0 && oppBprev == 1) {
				publicKey = bntB;
				oppB = 1;
				xSemaphoreGive(inputReceived);
				oppBprev = 0;
			} else if (oppBState == 1) {
				oppBprev = 1;
			}
		}

		if (game_mode == singlePlayer || game_mode == doublePlayer_Move
				|| game_mode == doublePlayer_Select
				|| game_mode == startingSelect
				|| (game_mode == doublePlayer_Rotate && myState == (int) pause)
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (GPIO_ReadInputDataBit(ESPL_Register_Button_C, ESPL_Pin_Button_C)
					== 0 && Cprev == 1) {
				publicKey = bntC;
				xSemaphoreGive(inputReceived);
				Cprev = 0;
			} else if (GPIO_ReadInputDataBit(ESPL_Register_Button_C,
			ESPL_Pin_Button_C) == 1) {
				Cprev = 1;
			}

		}

		if (game_mode == doublePlayer_Rotate || game_mode == doublePlayer_Select
				|| (game_mode == startingSelect && isConnected)
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (oppCState == 0 && oppCprev == 1) {
				publicKey = bntC;
				oppC = 1;
				xSemaphoreGive(inputReceived);
				oppCprev = 0;
			} else if (oppCState == 1) {
				oppCprev = 1;
			}
		}

		if (game_mode == singlePlayer || game_mode == doublePlayer_Move
				|| game_mode == doublePlayer_Select
				|| game_mode == startingSelect
				|| (game_mode == doublePlayer_Rotate && myState == (int) pause)
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (GPIO_ReadInputDataBit(ESPL_Register_Button_D, ESPL_Pin_Button_D)
					== 0 && Dprev == 1) {
				publicKey = bntD;
				xSemaphoreGive(inputReceived);
				Dprev = 0;
			} else if (GPIO_ReadInputDataBit(ESPL_Register_Button_D,
			ESPL_Pin_Button_D) == 1) {
				Dprev = 1;
			}

		}

		if (game_mode == doublePlayer_Rotate || game_mode == doublePlayer_Select
				|| (game_mode == startingSelect && isConnected)
				|| (game_mode == doublePlayer_Rotate
						&& myState == (int) gameOver)) {
			if (oppDState == 0 && oppDprev == 1) {
				publicKey = bntD;
				oppD = 1;
				xSemaphoreGive(inputReceived);
				oppDprev = 0;
			} else if (oppDState == 1) {
				oppDprev = 1;
			}
		}
		//For pause and pause sychronization
		if (game_mode == singlePlayer || game_mode == doublePlayer_Rotate) {
			if (GPIO_ReadInputDataBit(ESPL_Register_Button_E, ESPL_Pin_Button_E)
					== 0 && Eprev == 1) {
				publicKey = bntE;
				xSemaphoreGive(inputReceived);
				Eprev = 0;
			} else if (GPIO_ReadInputDataBit(ESPL_Register_Button_E,
			ESPL_Pin_Button_E) == 1) {
				Eprev = 1;
			}
		}

		if (game_mode == doublePlayer_Rotate) {
			if (oppEState == 0 && oppEprev == 1) {
				publicKey = bntE;
				oppE = 1;
				xSemaphoreGive(inputReceived);
				oppEprev = 0;
			} else if (oppEState == 1) {
				oppEprev = 1;
			}
		}

		if (game_mode == doublePlayer_Move && oppState == (int) pause) {
			publicKey = bntE;
			xSemaphoreGive(inputReceived);
		}
		if (game_mode == doublePlayer_Move && myState == (int) pause) {
			if (oppState == (int) gameInit) {
				publicKey = bntB;
				xSemaphoreGive(inputReceived);
			}
			if (oppState == (int) gameOver) {
				publicKey = bntD;
				xSemaphoreGive(inputReceived);
			}
			if (oppState == (int) endGameRound
					|| oppState == (int) inGameRound) {
				publicKey = bntC;
				xSemaphoreGive(inputReceived);
			}

		}

		//Check for device connection
		roundNumber = (roundNumber + 1) % 10;
		if (roundNumber == 0) {
			if (connectionError < 5) {
				isConnected = 1;
				errorFuse = 0;
			}

			else {
				errorFuse++;
				if (errorFuse >= 10) {
					isConnected = 0;
					errorFuse = 0;
				}
			}
			connectionError = 0;

		} else {
			if (!(myState == oppState
					|| ((myState <= (int) endGameRound
							&& myState >= (int) gameInit)
							&& (oppState <= (int) endGameRound
									&& oppState >= (int) gameInit))))
				connectionError++;
		}
		oppState = -1;
		vTaskDelayUntil(&xLastWakeTime, tickFramerate);
	}
}
/*
 * This task is used to refresh screen and control the speed of the game. Specifically, it send
 * the state manager a signal to end a game round. And the time period between two signals is set
 * accordingto game level. Moreover, rand number seed is generated here.
 */
void sys_refresh() {
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	while (TRUE) {

		srand(xTaskGetTickCount()); //generate random seed
		publicKey = sysRefresh; 	//send a system refresh key
		xSemaphoreGive(inputReceived); 	// Give semaphore to task manager
		roundTime = speed_coefficient * (8 - game_level) / 4; //Change game speed accoding to level
		vTaskDelayUntil(&xLastWakeTime, roundTime);
	}
}
/*
 * Counts how many times draw function is called in 1 second
 */
void fps_count() {
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	while (TRUE) {
		fps = fps_temp;
		fps_temp = 0;
		vTaskDelayUntil(&xLastWakeTime, 1000);
	}
}
/*
 * Insert plaer's score into history rank
 */
int rank(int score) {
	int rank_number = 4;
	high_score[rank_number] = score;
	for (int i = 3; i >= 1; i--) {
		if (score >= high_score[i]) {
			high_score[rank_number] = high_score[i];
			rank_number = i;
			high_score[rank_number] = score;
		} else
			break;
	}
	return rank_number;
}

/*
 * This module receive the current state and the signal from user or system and return the next gamestate.
 */
gameState getState(gameState state, key privateKey) {
	switch (state) {
	case startScene:

		/* State startScene is the state of the staring menu. User can choose the game mode (single or double player)
		 * and adjust the starting game level here by pressing the buttons. When the button is pressed, the privatekey
		 * is assigned.
		 */
	{
		if (privateKey == bntA) {
			game_mode = singlePlayer; //Set game mode as singleplayer when button A is pressed.
			speed_coefficient = speed_coefficient_single; //Set the starting speed coefficient. Not the starting speed!! Single mode is faster
			return gameInit;	//Leave starting menu and Enter gameInit state.
		}
		if (privateKey == bntB && isConnected) {
			game_mode = doublePlayer_Select;//Set game mode as doubleplayer when button B is pressed.
			speed_coefficient = speed_coefficient_double;//Set the starting speed coefficient. Not the starting speed!!Double mode is slower
			return selectControl;//Leave starting menu and Enter gameInit state.
		}
		if (privateKey == bntC) {
			if (start_game_level < 4)//The upper limitation of the starting level is 4
				start_game_level++;	//Increase the starting game level when B is pressed
		}
		if (privateKey == bntD) {
			if (start_game_level > 0)//The lower limitation of the starting level is 0
				start_game_level--;	//Decrease the starting game level when B is pressed
		} else
			return startScene;//If no button is pressed, remain in the starting menu.
		break;
	}
	case selectControl: {
		if (!isConnected) {
			systemInit();
			return startScene;
		}
		if (privateKey == bntA) {
			if (oppA)
				game_mode = doublePlayer_Rotate;
			else
				game_mode = doublePlayer_Move;
			return gameInit;
		}

		if (privateKey == bntB) {
			if (oppB)
				game_mode = doublePlayer_Move;
			else
				game_mode = doublePlayer_Rotate;
			return gameInit;
		}
		if (privateKey == bntC) {
			if (start_game_level < 4)//The upper limitation of the starting level is 4
				start_game_level++;	//Increase the starting game level when B is pressed
		}
		if (privateKey == bntD) {
			if (start_game_level > 0)//The lower limitation of the starting level is 0
				start_game_level--;	//Decrease the starting game level when B is pressed
		}
		return selectControl;
		break;
	}
	case gameInit:
		/*
		 * In the state gameInit, the game interface is drawn. the starting game level, lines will be assigned.
		 * The first tetris and the next one of it will be generated. All the block are set to be empty.
		 * This state is introduced so that we don't go through the above operations in every refreshing.
		 */
	{
		if (!isConnected
				&& (game_mode == doublePlayer_Move
						|| game_mode == doublePlayer_Rotate)) {
			systemInit();
			return startScene;
		}
		return inGameRound;		//Continue the game if the game is not over.
		break;
	}

	case inGameRound:
		/*
		 * In the state inGameRound, the user can control the tetris to move and rotate, or pause the game.
		 * In the game, a round starts when the center of tetris drops down one row, and this round ends when the
		 * center of tetris drops down to the next row.
		 * (IMPORTANT! The word 'drop down' is different with the word 'move down'.
		 * 'Drop down' means it's system enforced, it's not controlled by the user while the 'move down' is controled by the user)
		 */
	{
		if (!isConnected
				&& (game_mode == doublePlayer_Move
						|| game_mode == doublePlayer_Rotate)) {
			systemInit();
			return startScene;
		}
		if (privateKey == sysRefresh) {
			return endGameRound;//In the end of every round, the sysRefresh is assigned to privatekey and the state enter endGameRound.
		}
		if (privateKey == bntA || privateKey == bntB || privateKey == bntC
				|| privateKey == bntD)
			return inGameRound;	//Remain in the inGameRound state after user's movement or rotation.
		if (privateKey == bntE)
			return pause;			//Enter pause state if button E is pressed.
		break;
	}

	case endGameRound:
		/*In the end of every round, check if the game is over.*/
	{
		if (isGameOver) {
			return gameOver;		//Enter gameover scene if the game is over.
		}

		else {
			if (!isConnected
					&& (game_mode == doublePlayer_Move
							|| game_mode == doublePlayer_Rotate)) {
				systemInit();
				return startScene;
			}
			if (game_mode == doublePlayer_Move) {
				if (privateKey == bntE)
					return pause;	//Enter pause state if button E is pressed.
				return endGameRound;
			} else
				return inGameRound;	//Continue the game if the game is not over.
		}

		break;
	}

	case pause:
		/*
		 * Pause scene is the pause menu. User can choose to continue the game or end the game.
		 */
	{
		if (!isConnected
				&& (game_mode == doublePlayer_Move
						|| game_mode == doublePlayer_Rotate)) {
			systemInit();
			return startScene;
		}
		if (privateKey == bntB) {
			return gameInit;				//Resume the game if C is pressed.
		}

		if (privateKey == bntC) {
			if (game_mode == doublePlayer_Move)
				return endGameRound;
			return inGameRound;				//Resume the game if C is pressed.
		}

		else if (privateKey == bntD) {
			return gameOver;			//End the game if D is pressed.
		} else
			return pause;//If no operation is executed, the system remain in the pause scene waiting for the input.
		break;
	}
	case gameOver: {
		if (!isConnected && (game_mode == doublePlayer_Move)) {
			systemInit();
			return startScene;
		}
		if (privateKey != sysRefresh) {
			systemInit();
			return startScene;//User can press any button to go back to the starting menu.
		} else
			return gameOver;//If no button is pressed, the system remain in the gameover scene.
		break;
	}

	}
}
/*
 * This function initialzes game map: clear map to 0 and set walls.
 */
void clearBlockArray(int blockArray[arrayHeight][arrayWidth]) {
	// Clear game map to 0
	for (int row = 0; row < arrayHeight; row++)
		for (int col = 0; col < arrayWidth; col++) {
			blockArray[row][col] = 0;
		}
	//Set walls
	for (int row = 0; row < arrayHeight; row++) {
		blockArray[row][0] = wall;
		blockArray[row][arrayWidth - 1] = wall;
	}
	for (int col = 0; col < arrayWidth; col++) {
		blockArray[arrayHeight - 1][col] = wall;
	}
}

/* Class of game object. Denoting an object containing 4 coordinates of blocks.
 *
 * A tetris object comes with its center, space, number, nextnumber and its color.
 *
 * center & space: For every tetris, a center block is defined. The movement is executed on the coordinate of the center.
 * Base on the center point, the whole tetris can be generated. The coordinates of the 4 blocks including the center is store
 * in the array space.
 *
 * number & nextnumber: In this game, we give serial numbers to the tetris with different shapes to realize the rotation function.
 * For every tetris, it has a 'number' and a 'nextnumber'. The rotation is realized by clearing the current tetris and generating
 *  a new tetris according to the 'nextnumber'.
 *
 *  color: color of the tetris shown on the screen.
 */
struct tetrisObj {
	point center;
	point space[4];

	int number;
	int nextnumber;
	int color;

};
/*
 * This function simply copy all attribute from one tetris to another.
 * This is for avoiding messy pointers.
 */
void copyTetris(tetrisObj *currentTetris, tetrisObj *nextTetris) {
	currentTetris->center.x = nextTetris->center.x;
	currentTetris->center.y = nextTetris->center.y;
	currentTetris->color = nextTetris->color;
	currentTetris->nextnumber = nextTetris->nextnumber;
	currentTetris->number = nextTetris->number;
	shapeTetris(currentTetris);
}
/*
 * This is a constructor for tetrisobj class. Center of tetris is set to be in the hidden
 * part of the map (on the top of the map). Number and clor are random numbers. It also generates
 * The other 3 block coordinates of the tetris by calling shapeTetris
 */
void makeTetris(tetrisObj* tetris) {
	tetris->center.x = 5;
	tetris->center.y = 1;
	tetris->number = rand() % 24 + 1;
	tetris->color = rand() % 4 + 1;
	shapeTetris(tetris);
}
/*
 * The function parse tetrises info to UART port
 */
void sendTetris(tetrisObj* currentTetris, tetrisObj* nextTetris) {
	currentnumber = currentTetris->number;
	currentcolor = currentTetris->color;
	nextnumber = nextTetris->number;
	nextcolor = nextTetris->color;
	currentx = currentTetris->center.x;
	currenty = currentTetris->center.y;
}
/*
 * The function update tetrises to the info received from UART
 */
void syncTetris(tetrisObj* currentTetris, tetrisObj* nextTetris) {
	currentTetris->center.x = opp_currentx;
	currentTetris->center.y = opp_currenty;
	currentTetris->number = opp_currentnumber;
	currentTetris->color = opp_currentcolor;
	nextTetris->number = opp_nextnumber;
	nextTetris->color = opp_nextcolor;
	shapeTetris(currentTetris);
	shapeTetris(nextTetris);
}
/*
 * This method will compute the coordinate of all 4 tetris blocks from center coordinates.
 * Rules are specified into 19 different types. To make the chance even, we made some duplicated
 * copies of some special types like square. The types also come with a number denoting the type it can rotate
 * to.
 */
void shapeTetris(tetrisObj* tetris) {

	tetris->space[0].x = tetris->center.x;
	tetris->space[0].y = tetris->center.y;

	switch (tetris->number) {
	case 1: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 20;
		break;
	}
	case 2: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x + 2;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y;
		tetris->nextnumber = 3;
		break;
	}
	case 3: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 2;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 2;
		break;
	}
	case 4: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 5;
		break;
	}
	case 5: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y - 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y;
		tetris->nextnumber = 6;
		break;
	}
	case 6: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 7;
		break;
	}
	case 7: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y - 1;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y;
		tetris->nextnumber = 4;
		break;
	}
	case 8: {
		tetris->space[1].x = tetris->center.x - 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y - 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 11;
		break;
	}
	case 9: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y - 1;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 10;
		break;
	}
	case 10: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y - 1;
		tetris->space[2].x = tetris->center.x + 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 9;
		break;
	}
	case 11: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 8;
		break;
	}
	case 12: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y - 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 13;
		break;
	}
	case 13: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 14;
		break;
	}
	case 14: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y - 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 15;
		break;
	}
	case 15: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 12;
		break;
	}
	case 16: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y - 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 17;
		break;
	}
	case 17: {
		tetris->space[1].x = tetris->center.x - 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x + 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 18;
		break;
	}
	case 18: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y - 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 19;
		break;
	}
	case 19: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x - 1;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 16;
		break;
	}
	case 20: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 1;
		break;
	}
	case 21: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 22;
		break;
	}
	case 22: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 1;
		tetris->space[3].x = tetris->center.x + 1;
		tetris->space[3].y = tetris->center.y + 1;
		tetris->nextnumber = 21;
		break;
	}
	case 23: {
		tetris->space[1].x = tetris->center.x + 1;
		tetris->space[1].y = tetris->center.y;
		tetris->space[2].x = tetris->center.x + 2;
		tetris->space[2].y = tetris->center.y;
		tetris->space[3].x = tetris->center.x - 1;
		tetris->space[3].y = tetris->center.y;
		tetris->nextnumber = 24;
		break;
	}
	case 24: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 2;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 23;
		break;
	}
	case 25: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 2;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 26;
		break;
	}
	case 26: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 2;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 27;
		break;
	}
	case 27: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 2;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 28;
		break;
	}
	case 28: {
		tetris->space[1].x = tetris->center.x;
		tetris->space[1].y = tetris->center.y + 1;
		tetris->space[2].x = tetris->center.x;
		tetris->space[2].y = tetris->center.y + 2;
		tetris->space[3].x = tetris->center.x;
		tetris->space[3].y = tetris->center.y - 1;
		tetris->nextnumber = 25;
		break;
	}

	}
}
/*
 * This method check if the tetris collide with filled block on game map
 */
int noCollision(tetrisObj* tetris, int blockArray[arrayHeight][arrayWidth]) {
	int i;
	for (i = 0; i <= 3; i++) {
		if (blockArray[tetris->space[i].y][tetris->space[i].x] != 0)
			return 0;
	}
	return 1;
}
/*
 * This function moves tetris on the game map. Return 0 for move is able to be made. Otherwise, return 0
 */
int moveTetris(tetrisObj* tetris, direction dir,
		int blockArray[arrayHeight][arrayWidth]) {
	point pre_center;
	pre_center.x = tetris->center.x;
	pre_center.y = tetris->center.y;
	switch (dir) {
	case down: {
		tetris->center.y++;
		break;
	}
	case left: {
		tetris->center.x--;
		break;
	}
	case right: {
		tetris->center.x++;
		break;
	}
	}
	shapeTetris(tetris);
	// if collison happens, restore to original center coordinates
	if (!noCollision(tetris, blockArray)) {
		tetris->center.x = pre_center.x;
		tetris->center.y = pre_center.y;
		shapeTetris(tetris);
		return 0;
	} else
		return 1;
}
/*
 * This function rotate the tetris(change it from one type to another basically)
 */
void rotateTetris(tetrisObj *tetris, int blockArray[arrayHeight][arrayWidth]) {
	int pre_number = tetris->number;
	tetris->number = tetris->nextnumber;
	shapeTetris(tetris);
	// if collison happens, restore to original type
	if (!noCollision(tetris, blockArray)) {
		tetris->number = pre_number;
		shapeTetris(tetris);
	}
}
void drawInfo() {
	char str[100];
	font_t font_info = gdispOpenFont("DejaVuSans12_aa");
	sprintf(str, "fps %d", fps);
	gdispDrawString(0, 0, str, font_info, White);
	sprintf(str, "cnt %d", isConnected);
	gdispDrawString(0, 10, str, font_info, White);
	fps_temp++;
}

void drawStart() {

	char str[100];
	font_t font = gdispOpenFont("SEASRN__14");
	font_t font1 = gdispOpenFont("FFF_Tusj60");
	font_t font2 = gdispOpenFont("BEBAS___14");
	gdispClear(Black);

	xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);

	sprintf(str, "TETRIS");
	gdispDrawString(45, 25, str, font1, White);

	sprintf(str, " Start Single Player");
	gdispDrawString(85, 110, str, font, White);

	sprintf(str, "Start Double Player");
	gdispDrawString(85, 140, str, font, White);

	sprintf(str, "-- Start Level:  %d --", start_game_level);
	gdispDrawString(90, 170, str, font, White);

	gdispFillCircle(68, 118, 10, Yellow);
	sprintf(str, " A");
	gdispDrawString(64, 110, str, font2, Black);

	gdispFillCircle(68, 148, 10, Red);
	sprintf(str, " B");
	gdispDrawString(64, 140, str, font2, Black);

	gdispFillCircle(58, 178, 10, Blue);
	sprintf(str, " D");
	gdispDrawString(54, 170, str, font2, Black);

	gdispFillCircle(258, 178, 10, Green);
	sprintf(str, " C");
	gdispDrawString(254, 170, str, font2, Black);

	sprintf(str, "PRESENTED by JIAWEI & JUNXIN");
	gdispDrawString(30, 210, str, font, Grey);

	for (int i = 0; i <= 240; i += 5) {
		sprintf(str, "*");
		gdispDrawString(0, i, str, font, White);
		sprintf(str, "*");
		gdispDrawString(315, i, str, font, White);
	}
	for (int i = 0; i <= 320; i += 5) {
		sprintf(str, "*");
		gdispDrawString(i, 0, str, font, White);
		sprintf(str, "*");
		gdispDrawString(i, 235, str, font, White);
	}
	drawInfo();

	ESPL_DrawLayer();
}
void drawGameOver(int blockArray[arrayHeight][arrayWidth]) {
	font_t font1 = gdispOpenFont("FFF_Tusj22");
	char str[100];
	font_t font = gdispOpenFont("SEASRN__14");

	gdispClear(Black);
	xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);
	for (int row = displayUp; row < displayDown; row++)
		for (int col = displayLeft; col < displayRight; col++) {
			if (blockArray[row][col] == 0) {
				if (row >= 11 && row <= 15)
					continue;
				gdispFillArea(18 + 11 * col, 11 * row - 20, 9, 9, color[0]);
				gdispDrawBox(19 + 11 * col, 11 * row - 19, 8, 8, Grey);
			}

		}

	sprintf(str, "GAME");
	gdispDrawString(48, 102, str, font1, Red);
	sprintf(str, "OVER");
	gdispDrawString(48, 132, str, font1, Red);
	++gameoverColor;
	sprintf(str, "1st");
	if (player_rank != 1)
		gdispDrawString(180, 50, str, font, White);
	else
		gdispDrawString(180, 50, str, font, color[gameoverColor % 4 + 1]);
	sprintf(str, " 2nd");
	if (player_rank != 2)
		gdispDrawString(180, 90, str, font, White);
	else
		gdispDrawString(180, 90, str, font, color[gameoverColor % 4 + 1]);
	sprintf(str, " 3rd");
	if (player_rank != 3)
		gdispDrawString(180, 130, str, font, White);
	else
		gdispDrawString(180, 130, str, font, color[gameoverColor % 4 + 1]);

	sprintf(str, "***********************");
	gdispDrawString(180, 80, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 120, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 160, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 35, str, font, White);

	for (int i = 0; i <= 240; i += 5) {
		sprintf(str, "*");
		gdispDrawString(0, i, str, font, White);
		sprintf(str, "*");
		gdispDrawString(315, i, str, font, White);
	}
	for (int i = 0; i <= 320; i += 5) {
		sprintf(str, "*");
		gdispDrawString(i, 0, str, font, White);
		sprintf(str, "*");
		gdispDrawString(i, 235, str, font, White);
	}
	for (int i = 20; i <= 220; i += 5) {
		sprintf(str, "*");
		gdispDrawString(158, i, str, font, White);
	}
	sprintf(str, "%d", high_score[1]);
	if (player_rank != 1)
		gdispDrawString(240, 65, str, font, White);
	else
		gdispDrawString(240, 65, str, font, color[gameoverColor % 4 + 1]);
	sprintf(str, "%d", high_score[2]);
	if (player_rank != 2)
		gdispDrawString(240, 105, str, font, White);
	else
		gdispDrawString(240, 105, str, font, color[gameoverColor % 4 + 1]);
	sprintf(str, "%d", high_score[3]);
	if (player_rank != 3)
		gdispDrawString(240, 145, str, font, White);
	else
		gdispDrawString(240, 145, str, font, color[gameoverColor % 4 + 1]);

	sprintf(str, "Rank");
	gdispDrawString(180, 20, str, font, White);

	sprintf(str, " Score");
	gdispDrawString(180, 170, str, font, Lime);
	sprintf(str, "%d", game_score);
	gdispDrawString(240, 185, str, font, Lime);

	drawInfo();
	ESPL_DrawLayer();
}
void drawPause(int blockArray[arrayHeight][arrayWidth]) {
	char str[100];
	font_t font = gdispOpenFont("SEASRN__14");
	font_t font2 = gdispOpenFont("BEBAS___14");

	gdispClear(Black);
	xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);
	for (int row = displayUp; row < displayDown; row++)
		for (int col = displayLeft; col < displayRight; col++) {
			if (blockArray[row][col] == 0) {
				gdispFillArea(18 + 11 * col, 11 * row - 20, 9, 9, color[0]);
				gdispDrawBox(19 + 11 * col, 11 * row - 19, 8, 8, Grey);
			}

		}

	gdispFillArea(30, 30, 105, 30, Red);
	gdispDrawBox(30, 30, 105, 30, White);
	sprintf(str, "        PAUSE");
	gdispDrawString(35, 40, str, font, White);
	sprintf(str, "Secret");
	gdispDrawString(215, 200, str, font, Red);

	gdispFillArea(30, 90, 105, 30, Red);
	gdispDrawBox(30, 90, 105, 30, White);
	sprintf(str, "          reset");
	gdispDrawString(35, 95, str, font, White);
	gdispFillCircle(49, 102, 9, Yellow);
	sprintf(str, "B");
	gdispDrawString(47, 95, str, font2, White);

	gdispFillArea(30, 120, 105, 30, Red);
	gdispDrawBox(30, 120, 105, 30, White);
	sprintf(str, "         resume");
	gdispDrawString(35, 125, str, font, White);
	gdispFillCircle(49, 132, 9, Green);
	sprintf(str, "C");
	gdispDrawString(47, 125, str, font2, White);

	gdispFillArea(30, 150, 105, 30, Red);
	gdispDrawBox(30, 150, 105, 30, White);
	sprintf(str, "            exit");
	gdispDrawString(30, 155, str, font, White);
	gdispFillCircle(49, 163, 9, Blue);
	sprintf(str, "D");
	gdispDrawString(47, 155, str, font2, White);

	sprintf(str, "Lines");
	gdispDrawString(180, 50, str, font, White);
	sprintf(str, " Score");
	gdispDrawString(180, 90, str, font, White);
	sprintf(str, " Level");
	gdispDrawString(180, 130, str, font, White);

	sprintf(str, "***********************");
	gdispDrawString(180, 80, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 120, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 160, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 35, str, font, White);

	for (int i = 0; i <= 240; i += 5) {
		sprintf(str, "*");
		gdispDrawString(0, i, str, font, White);
		sprintf(str, "*");
		gdispDrawString(315, i, str, font, White);
	}
	for (int i = 0; i <= 320; i += 5) {
		sprintf(str, "*");
		gdispDrawString(i, 0, str, font, White);
		sprintf(str, "*");
		gdispDrawString(i, 235, str, font, White);
	}
	for (int i = 20; i <= 220; i += 5) {
		sprintf(str, "*");
		gdispDrawString(158, i, str, font, White);
	}
	sprintf(str, "%d", tot_cleared_lines);
	gdispDrawString(240, 65, str, font, White);
	sprintf(str, "%d", game_score);
	gdispDrawString(240, 105, str, font, White);
	sprintf(str, "%d", game_level);
	gdispDrawString(240, 145, str, font, White);

	if (game_mode == singlePlayer)
		sprintf(str, "Single");
	if (game_mode == doublePlayer_Move)
		sprintf(str, "Move");
	if (game_mode == doublePlayer_Rotate)
		sprintf(str, "Rotate");
	gdispDrawString(180, 20, str, font, White);

	sprintf(str, " Next");
	gdispDrawString(180, 170, str, font, White);

	drawInfo();
	ESPL_DrawLayer();
}

void drawSelect(int isSelected) {
	int round = 1;
	if (isSelected)
		round = 6;
	while (round > 0) {
		char str[100];
		font_t font = gdispOpenFont("SEASRN__14");
		font_t font1 = gdispOpenFont("FFF_Tusj60");
		font_t font2 = gdispOpenFont("BEBAS___14");
		gdispClear(Black);

		xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);
		if (round % 2 == 0 && game_mode == doublePlayer_Rotate) {
			sprintf(str, "Control Rotate");
			gdispDrawString(85, 140, str, font, Black);

		} else {
			sprintf(str, "Control Rotate");
			gdispDrawString(85, 140, str, font, White);

		}

		if (round % 2 == 0 && game_mode == doublePlayer_Move) {

			sprintf(str, " Control Move");
			gdispDrawString(85, 110, str, font, Black);

		} else {

			sprintf(str, " Control Move");
			gdispDrawString(85, 110, str, font, White);

		}

		sprintf(str, "TETRIS");
		gdispDrawString(45, 25, str, font1, White);

		sprintf(str, "-- Start Level:  %d --", start_game_level);
		gdispDrawString(90, 170, str, font, White);

		gdispFillCircle(68, 118, 10, Yellow);
		sprintf(str, " A");
		gdispDrawString(64, 110, str, font2, Black);

		gdispFillCircle(68, 148, 10, Red);
		sprintf(str, " B");
		gdispDrawString(64, 140, str, font2, Black);

		gdispFillCircle(58, 178, 10, Blue);
		sprintf(str, " D");
		gdispDrawString(54, 170, str, font2, Black);

		gdispFillCircle(258, 178, 10, Green);
		sprintf(str, " C");
		gdispDrawString(254, 170, str, font2, Black);

		sprintf(str, "PRESENTED by JIAWEI & JUNXIN");
		gdispDrawString(30, 210, str, font, Grey);

		for (int i = 0; i <= 240; i += 5) {
			sprintf(str, "*");
			gdispDrawString(0, i, str, font, White);
			sprintf(str, "*");
			gdispDrawString(315, i, str, font, White);
		}
		for (int i = 0; i <= 320; i += 5) {
			sprintf(str, "*");
			gdispDrawString(i, 0, str, font, White);
			sprintf(str, "*");
			gdispDrawString(i, 235, str, font, White);
		}
		drawInfo();
		ESPL_DrawLayer();
		round--;
		if (round)
			vTaskDelay(200);
	}
}
void drawGameScene(tetrisObj *nextTetris,
		int blockArray[arrayHeight][arrayWidth]) {

	char str[100];
	font_t font = gdispOpenFont("SEASRN__14");

	gdispClear(Black);
	xSemaphoreTake(ESPL_DisplayReady, portMAX_DELAY);
	for (int row = displayUp; row < displayDown; row++)
		for (int col = displayLeft; col < displayRight; col++) {
			if (blockArray[row][col] == 0) {
				gdispFillArea(18 + 11 * col, 11 * row - 20, 9, 9, color[0]);
				gdispDrawBox(19 + 11 * col, 11 * row - 19, 8, 8, Grey);
			}

			else {
				gdispFillArea(18 + 11 * col, 11 * row - 20, 9, 9,
						color[blockArray[row][col]]);
				gdispDrawBox(19 + 11 * col, 11 * row - 19, 8, 8, White);
			}

		}
	sprintf(str, "Lines");
	gdispDrawString(180, 50, str, font, White);
	sprintf(str, " Score");
	gdispDrawString(180, 90, str, font, White);
	sprintf(str, " Level");
	gdispDrawString(180, 130, str, font, White);

	sprintf(str, "***********************");
	gdispDrawString(180, 80, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 120, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 160, str, font, White);
	sprintf(str, "***********************");
	gdispDrawString(180, 35, str, font, White);

	for (int i = 0; i <= 240; i += 5) {
		sprintf(str, "*");
		gdispDrawString(0, i, str, font, White);
		sprintf(str, "*");
		gdispDrawString(315, i, str, font, White);
	}
	for (int i = 0; i <= 320; i += 5) {
		sprintf(str, "*");
		gdispDrawString(i, 0, str, font, White);
		sprintf(str, "*");
		gdispDrawString(i, 235, str, font, White);
	}
	for (int i = 20; i <= 220; i += 5) {
		sprintf(str, "*");
		gdispDrawString(158, i, str, font, White);
	}
	sprintf(str, "%d", tot_cleared_lines);
	gdispDrawString(240, 65, str, font, White);
	sprintf(str, "%d", game_score);
	gdispDrawString(240, 105, str, font, White);
	sprintf(str, "%d", game_level);
	gdispDrawString(240, 145, str, font, White);

	if (game_mode == singlePlayer)
		sprintf(str, "Single");
	if (game_mode == doublePlayer_Move)
		sprintf(str, "Move");
	if (game_mode == doublePlayer_Rotate)
		sprintf(str, "Rotate");
	gdispDrawString(180, 20, str, font, White);

	sprintf(str, " Next");
	gdispDrawString(180, 170, str, font, White);

	for (int i = 0; i < 4; i++) {
		gdispFillArea(190 + 11 * nextTetris->space[i].x,
				190 + 11 * nextTetris->space[i].y, 9, 9,
				color[nextTetris->color]);
		gdispDrawBox(191 + 11 * nextTetris->space[i].x,
				191 + 11 * nextTetris->space[i].y, 8, 8, White);
	}
	drawInfo();
	ESPL_DrawLayer();
}
/*
 * This function initialize game constants, tetris and game map before game starts
 */

void beforeGame(tetrisObj *currentTetris, tetrisObj *nextTetris,
		int blockArray[arrayHeight][arrayWidth]) {
	game_score = 0;
	game_level = start_game_level;
	tot_cleared_lines = 0;
	isGameOver = 0;
	makeTetris(currentTetris);
	makeTetris(nextTetris);
	if (game_mode == doublePlayer_Move) {
		int break_count = 0;
		while (opp_currentnumber == -1 || opp_nextnumber == -1
				|| opp_nextcolor == -1) {
			if (break_count > 200)
				break;
			break_count++;
			vTaskDelay(5);
		}
		syncTetris(currentTetris, nextTetris);
	} else {
		sendTetris(currentTetris, nextTetris);
	}
	clearBlockArray(blockArray);
}
/*
 * This function will try to drop tetris 1 block downward and return 0.
 * If this cannot be done, it means the tetris end its falling. The return is 1
 */
int checkForNewTetris(tetrisObj *tetris,
		int blockArray[arrayHeight][arrayWidth]) {
	if (!moveTetris(tetris, down, blockArray))
		return 1;
	else
		return 0;
}
/*
 * This function clears tetris's previous position on game map
 */
void clearTetrisPos(tetrisObj *tetris, int blockArray[arrayHeight][arrayWidth]) {
	for (int i = 0; i < 4; i++) {
		blockArray[tetris->space[i].y][tetris->space[i].x] = 0;
	}
}

/*
 * This function prints tetris's position on game map, including its color
 */
void printTetrisPos(tetrisObj *tetris, int blockArray[arrayHeight][arrayWidth]) {

	for (int i = 0; i < 4; i++) {
		blockArray[tetris->space[i].y][tetris->space[i].x] = tetris->color;
	}
}

/*
 * This function will count, record and clear completed lines
 */
int checkFullLine(int fullLineNumber[5],
		int blockArray[arrayHeight][arrayWidth]) {
	int cnt = 0;
	for (int row = displayUp; row < displayDown; row++) {
		int isLineFull = 1;
		//check if the line is completed
		for (int col = displayLeft; col < displayRight; col++) {
			if (blockArray[row][col] == 0) {
				isLineFull = 0;
				break;
			}
		}
		//Count and record the completed line
		if (isLineFull) {
			cnt++;
			fullLineNumber[cnt] = row;
			//Clear the completed line
			for (int col = displayLeft; col < displayRight; col++) {
				blockArray[row][col] = 0;
			}
		}
	}
	return cnt;
}
/*
 * This function will drop the blocks above cleared lines down
 */
void blockDropAfterFull(int fullLineNumber[5], int cnt,
		int blockArray[arrayHeight][arrayWidth]) {
	int clearedCnt = 0; // count how many lines have been cleared
	while (cnt > 0) {
		int emptyLineNumber = fullLineNumber[cnt] + clearedCnt; //find the lowest empty line

		//Shift every line above the empty line one block down
		for (int row = emptyLineNumber; row >= displayUp; row--) {
			for (int col = displayLeft; col < displayRight; col++) {
				blockArray[row][col] = blockArray[row - 1][col];
			}
		}
		cnt--;
		clearedCnt++;
	}
}
/*
 * This function check if game is over. Return 1 for game is over, otherwise 0
 */
int gameOverCheck(tetrisObj *tetris) {
	int _isGameOver = 0;
	//Check if the last tetris is in the hidden part of the map
	for (int i = 0; i < 4; i++) {
		if (tetris->space[i].y < displayUp) {
			_isGameOver = 1;
			break;
		}
	}
	return _isGameOver;
}
/*
 * This function initialise system before start scene
 */
void systemInit() {
	player_rank = 4;
	currentnumber = -1;
	nextnumber = -1;
	opp_currentnumber = -1;
	opp_nextnumber = -1;
	nextcolor = -1;
	opp_nextcolor = -1;
	start_game_level = 0;
	game_mode = startingSelect;
}

/* State manager task is the core of the game. The task is activated under two conditions: 1. the player
 * presses a button 2. the "sys_refresh" task signals a game round is ended, ie, a tetris ends its falling.
 * The task controls the switching between different scenes. Specifically, state manager will be
 * switching scenes everytime it is called. The scene switching follows a certain logic, and the logic
 * is moduled as a state machine and implemented in "getState" function. Therefore, a drawing
 * task to keep refreshing the screen is NOT needed. Screen is refreshed when draw function is called by
 * the state manager.
 */

void stateManager() {
	/* Game state. Used to implement state machine logic. The game has five states,
	 * startScene, gameInit, inGameRound, endGameRound, pause, gameOver. Initialized
	 * with the start scene
	 */
	gameState state = startScene;

	/* Game Map. Block array is a 24 ROW x 12 COL block map. However only from ROW 3 to ROW 22 and
	 * from COL 1 to COL 10 will be displayed. ROW 0 to ROW 2 will be used for game over check.
	 * COL 0, COL 11 and ROW 23 will be set to wall.
	 *
	 * A oordinate on the map can have 2 status. 0 for there is NO block on the coordinate. Non 0 for
	 * there IS a block on the coordinate, and different integer numbers representing corresponding colors.
	 * A draw function will print the game map according to this array
	 */
	int blockArray[arrayHeight][arrayWidth];

	/* currentTetris and nextTetris are two pointers to tetris object. Current tetris is the tetris
	 * that is falling on the game map. Next tetris is the tetris stationarily shown on the screen
	 * to tell the player what will fall next.
	 */
	tetrisObj obj1;
	tetrisObj obj2;
	tetrisObj *currentTetris = &obj1;
	tetrisObj *nextTetris = &obj2;
	int noresponseError = 0;
	systemInit();	// Initialize game constants:start_game_level,game_mode;
	drawStart();

	while (TRUE) {

		//Task will be activated by any user input and system refresh
		if ((xSemaphoreTake(inputReceived, portMAX_DELAY == pdTRUE))) {

			//Stores global publicKey into a privatekey to block any changes.
			//In case of input being too frequent so that key changes before scene switching finishes.
			key privateKey = publicKey;

			//Use previous state and the key received to compute for the new state
			state = getState(state, privateKey);
			myState = (int) state;
			initOppBtn();
			switch (state) {

			/* Start scene state displays a scene for user to choose single player mode and double player mode.
			 * It also allows player to choose start level from 0 to 4
			 */
			case startScene: {
				drawStart();
				break;
			}
			case selectControl: {
				drawSelect(0);
				break;
			}
				/* gameInit is only excuted one time before a game starts. It initializes some game constants.
				 * It displays an empty map afterwards.
				 */
			case gameInit: {
				if (game_mode == doublePlayer_Rotate
						|| game_mode == doublePlayer_Move)
					drawSelect(1);
				drawSelect(0);
				beforeGame(currentTetris, nextTetris, blockArray);//Initialize game constants: score,level, lines, isGameOver
				drawGameScene(nextTetris, blockArray);
				vTaskDelay(1000);
				break;
			}

				/* inGameRound state allows player to control the movement and rotation of the tetris freely. The state is only
				 * entered between two system refreshes. No new tetris check or game over check will be made in this state.
				 * Therefore, player can even move the tetris after it ends its falling.
				 */
			case inGameRound: {
				clearTetrisPos(currentTetris, blockArray);//Clear current tetris's previous position on map
				switch (privateKey) {
				case bntA: {
					rotateTetris(currentTetris, blockArray);//Press A to rotate
					break;
				}
				case bntB: {
					moveTetris(currentTetris, right, blockArray); //Press B to move right
					break;
				}
				case bntC: {
					moveTetris(currentTetris, down, blockArray); //Press C to move down
					break;
				}
				case bntD: {
					moveTetris(currentTetris, left, blockArray); //Press D to move left
					break;
				}
				}
				sendTetris(currentTetris, nextTetris);
				vTaskDelay(100);
				printTetrisPos(currentTetris, blockArray); //Print new position of current tetris on map
				drawGameScene(nextTetris, blockArray);
				break;
			}

				/* endGameRound is entered when signal is sent from "sys_refresh". A game round refers to the time period
				 * between two "natural" fall down, or to say "automatical" fall down. This time period is controlled by sys_refresh,
				 * which will get shorter when the level is higher. When a game round ends, five things will be done: 1.  check if the
				 * tetris falls to the end, otherwise, drop it 1 block down 2. update line, score, level 3. clear full line and update
				 * game map 4.Check if game is over 5. Change current tetris to next tetris and generate a new next tetris
				 */
			case endGameRound: {
				int isNewTetris = 0;
				if (!noresponseError)//when no response, master device has generated a new tetris while slave device still holding the previous tetris. Only happens in pause sychroniyation
					clearTetrisPos(currentTetris, blockArray); //Clear current tetris's previous position on map
				else
					noresponseError = 0;
				if (game_mode == doublePlayer_Move) {
					int break_count = 0;
					while (opp_currenty == currentTetris->center.y
							&& opp_currentx == currentTetris->center.x
							&& opp_currentnumber == currentTetris->number) {
						if (break_count > 200)
							break;
						break_count++;
						vTaskDelay(5);
					}
					syncTetris(currentTetris, nextTetris);
					tetrisObj temp;
					copyTetris(&temp, currentTetris);
					isNewTetris = checkForNewTetris(&temp, blockArray);
				} else {
					isNewTetris = checkForNewTetris(currentTetris, blockArray);	//check if the tetris falls to the end and try to drop it 1 block down
					sendTetris(currentTetris, nextTetris);
				}
				printTetrisPos(currentTetris, blockArray);//Print new position of current tetris on map
				drawGameScene(nextTetris, blockArray);

				if (isNewTetris) {
					isGameOver = gameOverCheck(currentTetris);//Check if game is over
					//Reads from master
					if (game_mode == doublePlayer_Move) {
						int break_count = 0;
						while (opp_currenty == currentTetris->center.y
								|| !isNewTetris) {// if the master has not generated new tetris
							if (break_count > 300) {
								noresponseError = 1;
								break;
							}
							break_count++;
							if (opp_currenty != 1
									&& (opp_currentx != currentTetris->center.x
											|| opp_currentnumber
													== currentTetris->nextnumber
											|| !isNewTetris)// if tetris in master still moves or rotates
									&& opp_currentcolor
											== currentTetris->color) {
								break_count = 0;
								clearTetrisPos(currentTetris, blockArray);
								syncTetris(currentTetris, nextTetris);
								tetrisObj temp;
								copyTetris(&temp, currentTetris);
								isNewTetris = checkForNewTetris(&temp,
										blockArray);
								printTetrisPos(currentTetris, blockArray);//Print new position of current tetris on map
								drawGameScene(nextTetris, blockArray);
							}
							vTaskDelay(5);
						}
						syncTetris(currentTetris, nextTetris);// sync to master
					} else {
						copyTetris(currentTetris, nextTetris);//Change current tetris to next tetris and generate a new next tetris
						makeTetris(nextTetris);
						sendTetris(currentTetris, nextTetris);
					}
				}
				if (isNewTetris) {

					int fullLineNumber[5];	//Stores which lines are completed
					int numOfFullLine = checkFullLine(fullLineNumber,
							blockArray);//Record, count and clear full lines.
					if (numOfFullLine) {
						game_score += score_rule[numOfFullLine - 1][game_level];//update score
						tot_cleared_lines += numOfFullLine;	//update lines
						if (game_level < maxLevel)
							game_level = start_game_level
									+ tot_cleared_lines / 2;	//update level
						drawGameScene(nextTetris, blockArray);
						blockDropAfterFull(fullLineNumber, numOfFullLine,
								blockArray);//let blocks above empty lines drop
						drawGameScene(nextTetris, blockArray);
					}

				}
				break;
			}
				/*
				 * Pause state will pause the game. Game map and next tetris will be set to black in case player take advantage of pausing.
				 * Player can choose either continue or exit.
				 */
			case pause: {
				drawPause(blockArray);
				break;
			}

				/* game over state will highlight player's score. Press any key to return to start scene*/
			case gameOver: {
				if (player_rank == 4)
					player_rank = rank(game_score);
				game_level = 0;
				drawGameOver(blockArray);
				break;
			}
			}
		}

	}
	initOppBtn();
}
int main() {
	// Initialize Board functions and graphics
	ESPL_SystemInit();

	inputReceived = xSemaphoreCreateBinary();
	xTaskCreate(sys_refresh, "sys_refresh", 2000, NULL, 3, NULL);// A task to refresh the system. It has the highest priority. Also sets game speed
	xTaskCreate(fps_count, "fps_count", 2000, NULL, 2, NULL);// A task to refresh the system. It has the highest priority. Also sets game speed
	xTaskCreate(user_input, "user_input", 2000, NULL, 2, NULL);	// A task to receive input
	xTaskCreate(receive_data, "receive_data", 1000, NULL, 2, NULL);	// A task to receive data from another device, for double player mode
	xTaskCreate(stateManager, "stateManager", 2000, NULL, 2, NULL);	// A task to switch between scenes. Core of the game
	xTaskCreate(send_btn, "send_btn", 1000, NULL, 2, NULL);
	// Start FreeRTOS Scheduler
	vTaskStartScheduler();
}
/*
 *  Hook definitions needed for FreeRTOS to function.
 */
void vApplicationIdleHook() {
	while (TRUE) {
	};
}

void vApplicationMallocFailedHook() {
	while (TRUE) {
	};
}

