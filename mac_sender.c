#include "main.h"
#include <stdio.h>


#define TIMEOUT_QUEUE 100
#define MAX_SAVED_MSG 30

//prototypes 
void checkMsg(struct queueMsg_t ** msg);
void sendMsg(struct queueMsg_t ** msg);
void saveMsg(struct queueMsg_t ** msg);
struct queueMsg_t ** getSavedMsg();

//global variable
bool tokenOwned = 0;
bool isToken = 0;

struct queueMsg_t ** queueMsgSaved[MAX_SAVED_MSG];
struct token_t * token;

void MacSender(void *argument)
{
	struct queueMsg_t **msg;
	struct TOKENINTERFACE *gTokenInterface;
	
	bool reading = true;
	
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
						
					} 
					else 
					{
						
					}
					break;
				
				case osErrorResource : //queue empty
					if(tokenOwned)
					{
						//
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

void checkMsg(struct queueMsg_t ** msg)
{
	switch((*msg)->type) 
		{
			case NEW_TOKEN:
				if(!isToken)
				{
					//generate new token and I owned the token
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
				}
				else
				{
					//token receive from other and I keep the token
					//update token
				}
				break;
				
			default:
				//chit!!!!
				break;
		}
}

//here it buil the msg and put in the queue of the pyhsic sender
//free the part of msg
void sendMsg(struct queueMsg_t ** msg)
{
	
}
//saved message getted from the mac sender queue when there isn't token
void saveMsg(struct queueMsg_t ** msg)
{
	
}

//get saved message 
struct queueMsg_t ** getSavedMsg()
{
	
}




