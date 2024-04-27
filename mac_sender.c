#include "main.h"
#include <stdio.h>

struct tokenManager_t
{
	bool tokenOwned;
	bool isToken;
	struct queueMsg_t * token;
};
struct databackManager_t
{
	bool waitDataBack;
	uint8_t nbrDataBackReceived;
	struct queueMsg_t msgSavedForDataback;
};

//prototypes 
void processMessage(struct queueMsg_t * msg, struct msg_content_t * msg_content, struct tokenManager_t * tokenManager, struct databackManager_t * databackManager);
void macSenderSendMsg(struct queueMsg_t * msg, osMessageQueueId_t queueId);

void saveMsg(struct queueMsg_t * msg);
void getSavedMsg(struct queueMsg_t * msg);
void sendSecondaryQueue();

struct token_t* getMyTokenState(struct tokenManager_t * tokenManager);
void updateToken(struct tokenManager_t * tokenManager);
void generateToken(struct queueMsg_t * msg, struct tokenManager_t * tokenManager);
void generateFrame(struct queueMsg_t * msg, uint8_t * previousAnyPtr, struct msg_content_t * msg_content);
void calculateChecksum(struct queueMsg_t * msg, struct msg_content_t * msg_content);
void sendTokenList();
void updateStation(struct tokenManager_t * tokenManager);
uint8_t getStringLength(uint8_t * stringPtr);
void copyMsg(struct queueMsg_t * msg, struct queueMsg_t * copy, uint8_t msg_length);


//--------------------------------------------------------------------------------
// Thread mac sender
//--------------------------------------------------------------------------------

void MacSender(void *argument)
{
	struct queueMsg_t msg;
	struct msg_content_t msg_content;
	struct tokenManager_t tokenManager;
	struct databackManager_t databackManager;
	
	tokenManager.isToken = false;
	tokenManager.tokenOwned = false;
	tokenManager.token = osMemoryPoolAlloc(memPool, osWaitForever);
	memset((void *)tokenManager.token, 0, sizeof(*tokenManager.token));
	
	databackManager.nbrDataBackReceived = 0;
	databackManager.waitDataBack= false;
	
	osStatus_t retCode;
	
	
	while(1)
	{
		
		if(tokenManager.tokenOwned)
		{
			if(databackManager.waitDataBack == false) //you put the waitdataback to false before send it so it don t work
			{
				if(osMessageQueueGetCount(queue_macS_sec_id) != 0)
				{
					sendSecondaryQueue();//process the first message on the secondary queue
				}
				else
				{
					macSenderSendMsg(tokenManager.token, queue_phyS_id);
					tokenManager.tokenOwned = false;
				}
			}
		}
		else
		{
			retCode = osMessageQueueGet(queue_macS_id, &msg, NULL, osWaitForever);
			CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
			processMessage(&msg, &msg_content, &tokenManager, &databackManager);
		}
	}
}

//--------------------------------------------------------------------------------
// Manage message
//--------------------------------------------------------------------------------
void processMessage(struct queueMsg_t * msg, struct msg_content_t * msg_content, struct tokenManager_t * tokenManager, struct databackManager_t * databackManager)
{
	uint8_t * previousAnyPtr = NULL;
	osStatus_t retCode;
	switch(msg->type) 
		{
			case NEW_TOKEN:
				if(!tokenManager->isToken && (databackManager->waitDataBack == false))
				{
					printf("NEW TOKEN\r\n");
					generateToken(msg, tokenManager);
					tokenManager->isToken = true;
					updateStation(tokenManager);
					macSenderSendMsg(msg, queue_phyS_id);
					tokenManager->tokenOwned = false;
				}
				else
				{
					//chit!!!!
				}
				break;
				
			case TOKEN:
				if(!tokenManager->tokenOwned && (databackManager->waitDataBack == false))
				{
					//token receive from other and I keep it
					tokenManager->token = msg;
					//update token
					updateToken(tokenManager);
					updateStation(tokenManager);
					tokenManager->isToken = true;
					tokenManager->tokenOwned = true;
				}
				else
				{
					//chit!!!
					printf("-token received (own already a token)\r\n");
				}
				break;
				
			case DATABACK:
				
				//configure msg content 
				getPtrMessageContent(msg, msg_content);
				
				databackManager->nbrDataBackReceived++; //increase nbr databack msg
				if(msg_content->status->read == true)
				{
					if(msg_content->status->ack == true)
					{
						//the message has been correctly transmit
						databackManager->waitDataBack = false;
						databackManager->nbrDataBackReceived = 0;
						macSenderSendMsg(tokenManager->token, queue_phyS_id);
						tokenManager->tokenOwned = false;
						//clear the copied msg
						
						retCode = osMemoryPoolFree(memPool, databackManager->msgSavedForDataback.anyPtr);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
						
						retCode = osMemoryPoolFree(memPool, msg->anyPtr);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}
					else
					{
						//send the saved message
						macSenderSendMsg(&databackManager->msgSavedForDataback, queue_phyS_id);
						
						retCode = osMemoryPoolFree(memPool, msg->anyPtr);
						CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					}
				}
				else
				{
					//station not connected
					databackManager->waitDataBack = false;
					databackManager->nbrDataBackReceived = 0;
					
					retCode = osMemoryPoolFree(memPool, msg->anyPtr);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					retCode = osMemoryPoolFree(memPool, databackManager->msgSavedForDataback.anyPtr);
					CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
					
					//send mac error
					struct queueMsg_t msg_mac_error;
					msg_mac_error.anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
					memset((void *)msg_mac_error.anyPtr, 0, MAX_BLOCK_SIZE);
					msg_mac_error.type = MAC_ERROR;
					uint8_t * error = (uint8_t *) "Station not connected\0";
					//copy the error in the memory allocation
					memcpy(msg_mac_error.anyPtr, error, getStringLength(error));
					macSenderSendMsg(&msg_mac_error, queue_lcd_id);
				}
				break;
			
			case START:
				//connect to the chat
				gTokenInterface.connected = true;
			
				retCode = osMemoryPoolFree(memPool, msg->anyPtr);
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				break;
			
			case STOP:
				//disconnect ot the chat
				gTokenInterface.connected = false;
			
				retCode = osMemoryPoolFree(memPool, msg->anyPtr);	
				CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
				break;
			
			case DATA_IND:
				generateFrame(msg, previousAnyPtr, msg_content);
				calculateChecksum(msg, msg_content);
				if(msg_content->control->srcSapi == CHAT_SAPI)
				{
					databackManager->waitDataBack = true;
					copyMsg(msg, &databackManager->msgSavedForDataback, *msg_content->length);
				}
				//save message
				saveMsg(msg);
				break;
			
			default:
				//chit!!!!
				break;
		}
}

void generateToken(struct queueMsg_t * msg, struct tokenManager_t * tokenManager)
{
	tokenManager->token->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memset((void *)tokenManager->token->anyPtr, 0, TOKENSIZE-2);
	//generate new token and I owned the token
	updateToken(tokenManager);
	getMyTokenState(tokenManager)->tag = TOKEN_TAG;
	msg->type = TOKEN;
	msg->anyPtr = (void*) tokenManager->token->anyPtr;
}

void generateFrame(struct queueMsg_t * msg, uint8_t * previousAnyPtr, struct msg_content_t * msg_content)
{
	previousAnyPtr = msg->anyPtr;
	msg->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	
	setPtrMessageContent(msg, msg_content, getStringLength(previousAnyPtr));
	
	//set the control
	msg_content->control->destSapi = msg->sapi;
	msg_content->control->destAddr = msg->addr;
	msg_content->control->srcSapi = msg->sapi;
	msg_content->control->srcAddr = MYADDRESS;
	
	//set the string content
	memcpy(&((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG], previousAnyPtr, *msg_content->length);
	//free the previous ptr
	osStatus_t retCode = osMemoryPoolFree(memPool, previousAnyPtr);
	CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
}

//calculate the length with the '\0'
uint8_t getStringLength(uint8_t * stringPtr)
{
	uint8_t length = 0;
	while(*stringPtr != '\0')
	{
		stringPtr++;
		length++;
	};
	return length+1;
}

void copyMsg(struct queueMsg_t * msg, struct queueMsg_t * copy, uint8_t msg_length)
{
	copy->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	copy->addr = msg->addr;
	copy->sapi = msg->sapi;
	copy->type = msg->type;
	memcpy((void *)copy->anyPtr,(const void *) msg->anyPtr, msg_length);
}

//--------------------------------------------------------------------------------
// Send message to physical layer
//--------------------------------------------------------------------------------

//here it buil the msg and put in the queue of the pyhsic sender
//free the part of msg
void macSenderSendMsg(struct queueMsg_t * msg, osMessageQueueId_t queueId)
{
	osStatus_t retCode = osMessageQueuePut(queueId, msg, osPriorityNormal, osWaitForever);
	CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
}

//--------------------------------------------------------------------------------
// Manage secondary queue
//--------------------------------------------------------------------------------

void sendSecondaryQueue()
{
	struct queueMsg_t msg;
	getSavedMsg(&msg);
	macSenderSendMsg(&msg, queue_phyS_id);
}

//saved message getted from the mac sender queue when there isn't token
void saveMsg(struct queueMsg_t * msg)
{
	osStatus_t retCode = osMessageQueuePut(queue_macS_sec_id, msg, osPriorityNormal, osWaitForever);
	CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
}

//get saved message 
void getSavedMsg(struct queueMsg_t * msg)
{
	osStatus_t retCode = osMessageQueueGet(queue_macS_sec_id, msg, NULL, osWaitForever);
	CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
}

//--------------------------------------------------------------------------------
// Manage token
//--------------------------------------------------------------------------------

struct token_t * getMyTokenState(struct tokenManager_t * tokenManager)
{
	return (struct token_t*) tokenManager->token->anyPtr;
}

void updateToken(struct tokenManager_t * tokenManager)
{
	getMyTokenState(tokenManager)->states[MYADDRESS].chat = gTokenInterface.connected;
	getMyTokenState(tokenManager)->states[MYADDRESS].time = gTokenInterface.broadcastTime;
}

void sendTokenList()
{
	struct queueMsg_t msg;
	msg.type = TOKEN_LIST;
	macSenderSendMsg(&msg, queue_lcd_id);
}

void updateStation(struct tokenManager_t * tokenManager)
{
	memcpy(gTokenInterface.station_list, &((uint8_t *)tokenManager->token->anyPtr)[1], MAX_STATION);
	sendTokenList();
}


