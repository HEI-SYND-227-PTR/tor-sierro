#include "main.h"
#include <stdio.h>



//prototypes 
void checkMsg(struct queueMsg_t * msg);
void sendMsg(struct queueMsg_t * msg);
void saveMsg(struct queueMsg_t * msg);
void getSavedMsg(struct queueMsg_t * msg);
void sendSecondaryQueue();
struct token_t* getMyTokenState();
void updateToken();

//global variable
bool tokenOwned = false;
bool isToken = false;

struct queueMsg_t	* token;

//--------------------------------------------------------------------------------
// Thread mac sender
//--------------------------------------------------------------------------------

void MacSender(void *argument)
{
	struct queueMsg_t msg;
	struct queueMsg_t *msg1;
	
	bool reading = true;
	bool sending = true;
	
	while(1)
	{
		//----------------------------------------------------------------------------
		// Read queue					
		//----------------------------------------------------------------------------
		
		while(reading)
		{
			osStatus_t status = osMessageQueueGet(queue_macS_id, &msg, NULL, TIMEOUT_QUEUE);
			switch (status) //read entry queue
			{
				case osOK : 					//queue not empty
					if(tokenOwned)
					{
						//send it
						//sendMsg(&msg);
					} 
					else 
					{
						checkMsg(&msg);
					}
					break;
				
				case osErrorResource : //queue empty
					if(tokenOwned)
					{
						//give the token
						sendMsg(token);
					} 
					else 
					{
						//do nothing
					}
					break;
				
				default:
					//do nothing
					break;
			}
		}
	}
}

//--------------------------------------------------------------------------------
// Manage message
//--------------------------------------------------------------------------------

void checkMsg(struct queueMsg_t * msg)
{
	switch(msg->type) 
		{
			case NEW_TOKEN:
				if(!isToken)
				{
					printf("NEW TOKEN\r\n");
					//generate new token and I owned the token
					gTokenInterface.broadcastTime = true;
					gTokenInterface.connected = true;
					updateToken();
					getMyTokenState()->tag = TOKEN_TAG;
					isToken = true;
					//prepare and send the token
					msg->type = TOKEN;
					msg->anyPtr = (void*) token;
					sendMsg(msg);
					tokenOwned = false;
				}
				else
				{
					//chit!!!!
				}
				break;
			case TOKEN:
				if(tokenOwned)
				{
					//chit!!!
					printf("-token received (own already a token)\r\n");
				}
				else
				{
					//token receive from other and I keep it
					token = msg;
					//update token
					updateToken();
					isToken = true;
					tokenOwned = true;
					
				}
				break;
				
			default:
				//chit!!!!
				break;
		}
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


