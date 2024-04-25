#include "main.h"
#include <stdio.h>



//prototypes 
void processMessage(struct queueMsg_t * msg);
void sendMsg(struct queueMsg_t * msg);

void saveMsg(struct queueMsg_t * msg);
void getSavedMsg(struct queueMsg_t * msg);
void sendSecondaryQueue();

struct token_t* getMyTokenState();
void updateToken();
void generateToken(struct queueMsg_t * msg);

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
				sendMsg(token);
				tokenOwned = false;
			}
		}
		else
		{
			osMessageQueueGet(queue_macS_id, &msg, NULL, osWaitForever);
			processMessage(&msg);
		}
	}
}

//--------------------------------------------------------------------------------
// Manage message
//--------------------------------------------------------------------------------

void processMessage(struct queueMsg_t * msg)
{
	switch(msg->type) 
		{
			case NEW_TOKEN:
				if(!isToken)
				{
					printf("NEW TOKEN\r\n");
					generateToken(msg);
					isToken = true;
					sendMsg(msg);
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
					isToken = true;
					tokenOwned = true;
				}
				else
				{
					//chit!!!
					printf("-token received (own already a token)\r\n");
				}
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
	gTokenInterface.broadcastTime = true;
	gTokenInterface.connected = true;
	updateToken();
	getMyTokenState()->tag = TOKEN_TAG;
	msg->type = TOKEN;
	msg->anyPtr = (void*) token->anyPtr;
}

//--------------------------------------------------------------------------------
// Send message to physical layer
//--------------------------------------------------------------------------------

//here it buil the msg and put in the queue of the pyhsic sender
//free the part of msg
void sendMsg(struct queueMsg_t * msg)
{
	bool sending = true;
	while(sending)
	{
		switch(osMessageQueuePut(queue_phyS_id, msg, osPriorityNormal, TIMEOUT_QUEUE))
		{
			case osOK : 					//msg send
				sending = false;
				printf("Message sended\r\n");
				break;
				
			case osErrorResource : //msg not send (queue full)
				printf("Message not sended\r\n");
				break;
			
			default:
				printf("Message not sended\r\n");
				break;
		}
	}
}

//--------------------------------------------------------------------------------
// Manage secondary queue
//--------------------------------------------------------------------------------

void sendSecondaryQueue()
{
	struct queueMsg_t * msg;
	getSavedMsg(msg);
	sendMsg(msg);
}

//saved message getted from the mac sender queue when there isn't token
void saveMsg(struct queueMsg_t * msg)
{
	bool saving = true;
	while(saving)
	{
		switch(osMessageQueuePut(queue_macS_sec_id, msg, osPriorityNormal, TIMEOUT_QUEUE))
		{
			case osOK : 					//msg send
				saving = false;
				break;
				
			case osErrorResource : //msg not send (queue full)
				//make a longer timeout or increase the max of the queue
				break;
			
			default:
				//do nothing
				break;
		}
	}
}
//get saved message 
void getSavedMsg(struct queueMsg_t * msg)
{
	bool getSaving = true;
	while(getSaving)
	{
		switch(osMessageQueuePut(queue_macS_sec_id, msg, osPriorityNormal, TIMEOUT_QUEUE))
		{
			case osOK : 					//msg get
				getSaving = false;
				break;
				
			case osErrorResource : //queue empty
				getSaving = false;
				msg = NULL;
				break;
			
			default:
				msg = NULL;
				break;
		}
	}
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


