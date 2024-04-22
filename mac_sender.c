#include "main.h"
#include <stdio.h>


#define TIMEOUT_QUEUE 100

//prototypes 
void checkMsg(struct queueMsg_t ** msg);
void sendMsg(struct queueMsg_t ** msg);
void saveMsg(struct queueMsg_t ** msg);
void getSavedMsg(struct queueMsg_t ** msg);
void sendSecondaryQueue();
struct token_t* getToken();

//global variable
bool tokenOwned = 0;
bool isToken = 0;

struct queueMsg_t	** token;

//--------------------------------------------------------------------------------
// Thread mac sender
//--------------------------------------------------------------------------------

void MacSender(void *argument)
{
	struct queueMsg_t **msg;
	struct TOKENINTERFACE *gTokenInterface;
	
	bool reading = true;
	bool sending = true;
	
	while(1)
	{
		//----------------------------------------------------------------------------
		// Read queue					
		//----------------------------------------------------------------------------
		
		while(reading)
		{
			 
			switch (osMessageQueueGet(queue_macS_id, msg, NULL, TIMEOUT_QUEUE)) //read entry queue
			{
				case osOK : 					//queue not empty
					if(tokenOwned)
					{
						//send it
						//sendMsg(msg);
					} 
					else 
					{
						//save it in the other queue
						//saveMsg(msg);
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

void checkMsg(struct queueMsg_t ** msg)
{
	switch((*msg)->type) 
		{
			case NEW_TOKEN:
				if(!isToken)
				{
					//generate new token and I owned the token
					gTokenInterface.broadcastTime = true;
					gTokenInterface.connected = true;
					getToken()->states[MYADDRESS] = (gTokenInterface.connected<<CHAT_SAPI) & (gTokenInterface.broadcastTime<<TIME_SAPI);
					isToken = true;
					//prepare and send the token
					(*msg)->type = TOKEN;
					(*msg)->anyPtr = (void*) token;
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
					printf("-token received (own already a token)");
				}
				else
				{
					//token receive from other and I keep it
					token = msg;
					//update token
					getToken()->states[MYADDRESS] = (gTokenInterface.connected<<CHAT_SAPI) & (gTokenInterface.broadcastTime<<TIME_SAPI);
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
void sendMsg(struct queueMsg_t ** msg)
{
	bool sending = true;
	while(sending)
	{
		switch(osMessageQueuePut(queue_phyS_id, msg, osPriorityNormal, TIMEOUT_QUEUE))
		{
			case osOK : 					//msg send
				sending = false;
				break;
				
			case osErrorResource : //msg not send (queue full)
				//make a longer timeout
				break;
			
			default:
				//do nothing
				break;
		}
	}
}

//--------------------------------------------------------------------------------
// Manage secondary queue
//--------------------------------------------------------------------------------

void sendSecondaryQueue()
{
	struct queueMsg_t ** msg;
	getSavedMsg(msg);
	sendMsg(msg);
}

//saved message getted from the mac sender queue when there isn't token
void saveMsg(struct queueMsg_t ** msg)
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
void getSavedMsg(struct queueMsg_t ** msg)
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

struct token_t * getToken()
{
	return ((struct token_t*)(*token)->anyPtr);
}

