#include "main.h"
#include <stdio.h>



//prototypes 
void processMessage(struct queueMsg_t * msg, struct msg_content_t * msg_content);
void macSenderSendMsg(struct queueMsg_t * msg, osMessageQueueId_t queueId);

void saveMsg(struct queueMsg_t * msg);
void getSavedMsg(struct queueMsg_t * msg);
void sendSecondaryQueue();

struct token_t* getMyTokenState();
void updateToken();
void generateToken(struct queueMsg_t * msg);
void generateFrame(struct queueMsg_t * msg, uint8_t * previousAnyPtr, struct msg_content_t * msg_content);
void calculateChecksum(struct queueMsg_t * msg, struct msg_content_t * msg_content);
void sendTokenList();
void updateStation(struct queueMsg_t * token);

//global variable
bool tokenOwned = false;
bool isToken = false;

struct queueMsg_t * token;

//--------------------------------------------------------------------------------
// Thread mac sender
//--------------------------------------------------------------------------------

void MacSender(void *argument)
{
	struct queueMsg_t msg;
	struct msg_content_t msg_content;
	
	token = osMemoryPoolAlloc(memPool, osWaitForever);
	memset((void *)token, 0, sizeof(*token));
	
	bool process_msg = false;
	bool end_process_msg = false;
	
	while(1)
	{
		
		if(tokenOwned)
		{
			if(osMessageQueueGetCount(queue_macS_sec_id) != 0)
			{
				sendSecondaryQueue();//process the first message on the secondary queue
			}
			else
			{
				macSenderSendMsg(token, queue_phyS_id);
				tokenOwned = false;
			}
		}
		else
		{
			osMessageQueueGet(queue_macS_id, &msg, NULL, osWaitForever);
			processMessage(&msg, &msg_content);
		}
	}
}

//--------------------------------------------------------------------------------
// Manage message
//--------------------------------------------------------------------------------
void processMessage(struct queueMsg_t * msg, struct msg_content_t * msg_content)
{
	uint8_t * previousAnyPtr = NULL;
	switch(msg->type) 
		{
			case NEW_TOKEN:
				if(!isToken)
				{
					printf("NEW TOKEN\r\n");
					generateToken(msg);
					isToken = true;
					updateStation(token);
					macSenderSendMsg(msg, queue_phyS_id);
					tokenOwned = false;
				}
				else
				{
					//chit!!!!
				}
				break;
				
			case TOKEN:
				if(!tokenOwned)
				{
					//token receive from other and I keep it
					token = msg;
					//update token
					updateToken();
					updateStation(token);
					isToken = true;
					tokenOwned = true;
				}
				else
				{
					//chit!!!
					printf("-token received (own already a token)\r\n");
				}
				break;
				
			case DATABACK:
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
				//save message
				saveMsg(msg);
				break;
			
			default:
				//chit!!!!
				break;
		}
}

void generateToken(struct queueMsg_t * msg)
{
	token->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memset((void *)token->anyPtr, 0, TOKENSIZE-2);
	//generate new token and I owned the token
	//gTokenInterface.broadcastTime = true;
	gTokenInterface.connected = true;
	updateToken();
	getMyTokenState()->tag = TOKEN_TAG;
	msg->type = TOKEN;
	msg->anyPtr = (void*) token->anyPtr;
}

void generateFrame(struct queueMsg_t * msg, uint8_t * previousAnyPtr, struct msg_content_t * msg_content)
{
	previousAnyPtr = msg->anyPtr;
	msg->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memset((void *)msg->anyPtr, 0, MAX_BLOCK_SIZE);
	
	//set ptr for content
	msg_content->length = &((uint8_t *)msg->anyPtr)[3];
	*msg_content->length = sizeof(*previousAnyPtr);
	msg_content->control = ((union control_t *)msg->anyPtr);
	msg_content->ptr = &((uint8_t *)msg->anyPtr)[4];
	msg_content->status = (union status_t *)(&((uint8_t *)msg->anyPtr)[3 + *msg_content->length + 1]);
	
	//set the control
	msg_content->control->destSapi = msg->sapi;
	msg_content->control->destAddr = msg->addr;
	msg_content->control->srcSapi = msg->sapi;
	msg_content->control->srcAddr = MYADDRESS;
	//set the string content
	for(uint32_t i=0; i<*msg_content->length; i++)
	{
		((uint8_t *)msg->anyPtr)[i + OFFSET_TO_MSG] = previousAnyPtr[i + OFFSET_TO_MSG];
	}
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
	struct queueMsg_t * msg;
	getSavedMsg(msg);
	macSenderSendMsg(msg, queue_phyS_id);
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

struct token_t * getMyTokenState()
{
	return (struct token_t*) token->anyPtr;
}

void updateToken()
{
	getMyTokenState()->states[MYADDRESS].chat = gTokenInterface.connected;
	getMyTokenState()->states[MYADDRESS].time = gTokenInterface.broadcastTime;
}

void sendTokenList()
{
	struct queueMsg_t msg;
	msg.type = TOKEN_LIST;
	macSenderSendMsg(&msg, queue_lcd_id);
}

void updateStation(struct queueMsg_t * token)
{
	for(uint32_t i=0; i<MAX_STATION; i++)
	{
		gTokenInterface.station_list[i] = ((uint8_t *)token->anyPtr)[i+1];
	}
	sendTokenList();
}


