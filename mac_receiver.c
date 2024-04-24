#include "main.h"
#include <stdio.h>


bool controlToken(struct queueMsg_t *msg);
void controlSrcAddr(struct queueMsg_t *msg);
void controlDestAddr(struct queueMsg_t *msg);
void sendMessage(struct queueMsg_t *msg);
void putNextQueue(struct queueMsg_t *msg, osMessageQueueId_t queue);


//--------------------------------------------------------------------------------
// Thread mac receiver
//--------------------------------------------------------------------------------
void MacReceiver(void *argument)
{
	struct queueMsg_t msg;
	bool reading = true;
	
	while(true)
	{
		//read the input queue
		while(reading)
		{
			osStatus_t status = osMessageQueueGet(queue_macR_id, &msg, NULL, TIMEOUT_QUEUE);
			switch (status) //read entry queue
			{
				case osOK : 					//queue not empty
					//check token tag to detect a token frame 
					if(controlToken(&msg) == true)
					{
						//send it to the mac sender and change msg type
						msg.type = TOKEN;
						printf("RECEIVE TOKEN\r\n");
					}
					else
					{
						printf("RECEIVE MESSAGE\r\n");
					}
					break;
				
				default:
					printf("RECEIVE NOTHING (%d\r\n)", status);
					break;
			}
		}
	}

}


//--------------------------------------------------------------------------------
// token management
//--------------------------------------------------------------------------------
bool controlToken(struct queueMsg_t *msg)
{
	return ((struct token_t*) msg->anyPtr)->tag == (uint8_t) TOKEN_TAG;
}

//--------------------------------------------------------------------------------
// source address management
//--------------------------------------------------------------------------------
void controlSrcAddr(struct queueMsg_t *msg)
{
	
}

//--------------------------------------------------------------------------------
// destination address management
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
// send message management
//--------------------------------------------------------------------------------
void sendMessage(struct queueMsg_t *msg)
{
	switch((msg)->type)
	{
		case TOKEN:
			putNextQueue(msg, queue_macS_id);
			break;
		
		default:
			break;
	}
}

void putNextQueue(struct queueMsg_t *msg, osMessageQueueId_t queue)
{
	bool sending = true;
	while(sending)
	{
		switch(osMessageQueuePut(queue, msg, osPriorityNormal, TIMEOUT_QUEUE))
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


