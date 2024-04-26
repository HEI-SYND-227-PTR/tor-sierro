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
void getMessageContent2(struct queueMsg_t *msg, struct msg_content_t* msg_content);

//variables globale



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
	
	
	
	while(1)
	{
		
		if(tokenManager.tokenOwned)
		{
			if(databackManager.waitDataBack == false)
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
			osMessageQueueGet(queue_macS_id, &msg, NULL, osWaitForever);
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
				getMessageContent2(msg, msg_content);
				
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
						osMemoryPoolFree(memPool, databackManager->msgSavedForDataback.anyPtr);
						osMemoryPoolFree(memPool, msg->anyPtr);
					}
					else
					{
						//send the saved message
						macSenderSendMsg(&databackManager->msgSavedForDataback, queue_phyS_id);
						osMemoryPoolFree(memPool, msg->anyPtr);
					}
				}
				else
				{
					//station not connected
					databackManager->waitDataBack = false;
					databackManager->nbrDataBackReceived = 0;
					osMemoryPoolFree(memPool, msg->anyPtr);
					osMemoryPoolFree(memPool, databackManager->msgSavedForDataback.anyPtr);
					
					//send mac error
					struct queueMsg_t msg_mac_error;
					msg_mac_error.anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
					memset((void *)msg_mac_error.anyPtr, 0, MAX_BLOCK_SIZE);
					msg_mac_error.type = MAC_ERROR;
					uint8_t * error = (uint8_t *) "Station not connected\0";
					//copy the error in the memory allocation
					for(uint32_t i=0; i<getStringLength(error); i++)
					{
						((uint8_t *)msg_mac_error.anyPtr)[i] = error[i];
					}
					macSenderSendMsg(&msg_mac_error, queue_lcd_id);
				}
				break;
			
			case START:
				//connect to the chat
				gTokenInterface.connected = true;
				osMemoryPoolFree(memPool, msg->anyPtr);
				break;
			
			case STOP:
				//disconnect ot the chat
				gTokenInterface.connected = false;
				osMemoryPoolFree(memPool, msg->anyPtr);	
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
	//gTokenInterface.broadcastTime = true;
	gTokenInterface.connected = true;
	updateToken(tokenManager);
	getMyTokenState(tokenManager)->tag = TOKEN_TAG;
	msg->type = TOKEN;
	msg->anyPtr = (void*) tokenManager->token->anyPtr;
}

void generateFrame(struct queueMsg_t * msg, uint8_t * previousAnyPtr, struct msg_content_t * msg_content)
{
	previousAnyPtr = msg->anyPtr;
	msg->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memset((void *)msg->anyPtr, 0, MAX_BLOCK_SIZE);
	
	//set ptr for content
	msg_content->length = &((uint8_t *)msg->anyPtr)[2];
	*msg_content->length = getStringLength(previousAnyPtr);
	msg_content->control = ((union control_t *)msg->anyPtr);
	msg_content->ptr = &((uint8_t *)msg->anyPtr)[3];
	msg_content->status = (union status_t *)(&((uint8_t *)msg->anyPtr)[3 + *msg_content->length]);
	
	//set the control
	msg_content->control->destSapi = msg->sapi;
	msg_content->control->destAddr = msg->addr;
	msg_content->control->srcSapi = msg->sapi;
	msg_content->control->srcAddr = MYADDRESS;
	//set the string content
	for(uint32_t i=0; i<*msg_content->length; i++)
	{
		((uint8_t *)msg->anyPtr)[i + OFFSET_TO_MSG] = previousAnyPtr[i];
	}
	//free the previous ptr
	osMemoryPoolFree(memPool, previousAnyPtr);
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


void calculateChecksum(struct queueMsg_t * msg, struct msg_content_t * msg_content)
{
	uint32_t sum = 0;
	for(uint32_t i=0; i<(3 + *msg_content->length); i++)
	{
		sum += ((uint8_t *)msg->anyPtr)[i];
	}
	msg_content->status->checksum = (sum & 0x3F);
}

void copyMsg(struct queueMsg_t * msg, struct queueMsg_t * copy, uint8_t msg_length)
{
	copy->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memset((void *)msg->anyPtr, 0, MAX_BLOCK_SIZE);
	copy->addr = msg->addr;
	copy->sapi = msg->sapi;
	copy->type = msg->type;
	for(uint32_t i=0; i<(msg_length + 4); i++)
	{
		((uint8_t *)copy->anyPtr)[i] = ((uint8_t *)msg->anyPtr)[i];
	}
}

void getMessageContent2(struct queueMsg_t *msg, struct msg_content_t* msg_content)
{
		*msg_content->length = ((uint8_t *)msg->anyPtr)[2];
		msg_content->control = ((union control_t *)msg->anyPtr);
		msg_content->ptr = &((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG];
		msg_content->status = (union status_t *)(&((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG + *msg_content->length]);
}

//--------------------------------------------------------------------------------
// Send message to physical layer
//--------------------------------------------------------------------------------

//here it buil the msg and put in the queue of the pyhsic sender
//free the part of msg
void macSenderSendMsg(struct queueMsg_t * msg, osMessageQueueId_t queueId)
{
	osMessageQueuePut(queueId, msg, osPriorityNormal, osWaitForever);
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
	osMessageQueuePut(queue_macS_sec_id, msg, osPriorityNormal, osWaitForever);
}

//get saved message 
void getSavedMsg(struct queueMsg_t * msg)
{
	osMessageQueueGet(queue_macS_sec_id, msg, NULL, osWaitForever);
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
	for(uint32_t i=0; i<MAX_STATION; i++)
	{
		gTokenInterface.station_list[i] = ((uint8_t *)tokenManager->token->anyPtr)[i+1];
	}
	sendTokenList();
}


