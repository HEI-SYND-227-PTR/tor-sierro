#include "main.h"
#include <stdio.h>

bool controlToken(struct queueMsg_t *msg);
void controlSrcAddr(struct queueMsg_t *msg);
void controlDestAddr(struct queueMsg_t *msg);
void macReceiverSendMsg(struct queueMsg_t *msg, osMessageQueueId_t queue);

void generateFrameApp(struct queueMsg_t *msg, struct queueMsg_t *msg_to_app, uint8_t msg_length);
void prepareMessageQueue(struct queueMsg_t *msg, struct msg_content_t *msg_content);

//--------------------------------------------------------------------------------
// Thread mac receiver
//--------------------------------------------------------------------------------
void MacReceiver(void *argument)
{
	struct queueMsg_t msg;
	struct queueMsg_t msg_to_app;
	struct msg_content_t msg_content;
	osStatus_t retCode;
	while(true) 
	{
		//read the input queue
		retCode = osMessageQueueGet(queue_macR_id, &msg, NULL, osWaitForever);
		CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
//----------------------------------------------------------------------------
//	Manage token								
//----------------------------------------------------------------------------
		if(controlToken(&msg))
		{
			//send it to the mac sender and change msg type
			msg.type = TOKEN;
			macReceiverSendMsg(&msg, queue_macS_id);
		}
		else
		{
			//get control and status value of the msg
			getPtrMessageContent(&msg, &msg_content);
			
			if(msg_content.control->destAddr == BROADCAST_ADDRESS || msg_content.control->destAddr == MYADDRESS)
			{
				if(checkMessage(&msg, &msg_content))
				{
					if(msg_content.control->destSapi == TIME_SAPI)
					{
						//generate msg to app with malloc
						generateFrameApp(&msg, &msg_to_app, *msg_content.length);
						msg_to_app.type = DATA_IND;
						prepareMessageQueue(&msg_to_app, &msg_content);
						macReceiverSendMsg(&msg_to_app, queue_timeR_id); //send it
					}

					else if((msg_content.control->destSapi == CHAT_SAPI))
					{
						if(gTokenInterface.connected)
						{
						//generate msg to app with malloc
						generateFrameApp(&msg, &msg_to_app, *msg_content.length);
						msg_to_app.type = DATA_IND;
						prepareMessageQueue(&msg_to_app, &msg_content);
						macReceiverSendMsg(&msg_to_app, queue_chatR_id); //send it
						}
					}
				}
				msg_content.status->read = true;
			}
			//send all to the mac sender (databack)
			msg.type = DATABACK;
			prepareMessageQueue(&msg, &msg_content);
			macReceiverSendMsg(&msg, queue_macS_id);

		}
	}
}
//--------------------------------------------------------------------------------
// message management
//--------------------------------------------------------------------------------


void generateFrameApp(struct queueMsg_t *msg, struct queueMsg_t *msg_to_app, uint8_t msg_length)
{
	msg_to_app->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memcpy(msg_to_app->anyPtr, &((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG], msg_length);
	((uint8_t *)msg_to_app->anyPtr)[msg_length] = '\0';
}


void prepareMessageQueue(struct queueMsg_t *msg, struct msg_content_t *msg_content)
{
	//update read status, message queue, ...
	switch(msg->type)
	{
		case TOKEN:
			//do nothing
			break;
		
		case DATABACK:
			msg->sapi = msg_content->control->srcSapi;
			msg->addr = msg_content->control->srcAddr;
			break;
		
		case DATA_IND:
			msg->sapi = msg_content->control->srcSapi;
			msg->addr = msg_content->control->srcAddr;
			break;
		
		case TO_PHY:
			//do nothing
			break;
		
		default:
			break;
		
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
// send message management
//--------------------------------------------------------------------------------
void macReceiverSendMsg(struct queueMsg_t *msg, osMessageQueueId_t queue)
{
	osStatus_t retCode = osMessageQueuePut(queue, msg, osPriorityNormal, osWaitForever);
	CheckRetCode(retCode,__LINE__,__FILE__,CONTINUE);
}


