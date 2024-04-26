#include "main.h"
#include <stdio.h>


bool controlToken(struct queueMsg_t *msg);
void controlSrcAddr(struct queueMsg_t *msg);
void controlDestAddr(struct queueMsg_t *msg);
void macReceiverSendMsg(struct queueMsg_t *msg, osMessageQueueId_t queue);
void getMessageContent(struct queueMsg_t *msg, struct msg_content_t* msg_content);
bool checkMessage(struct queueMsg_t *msg, struct msg_content_t* msg_content);
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
	
		
	while(true)
	{
		//read the input queue
		osStatus_t status = osMessageQueueGet(queue_macR_id, &msg, NULL, osWaitForever);
	
		//check token tag to detect a token frame 
		if(controlToken(&msg))
		{
			//send it to the mac sender and change msg type
			msg.type = TOKEN;
			macReceiverSendMsg(&msg, queue_macS_id);
		}
		else
		{
			//get control and status value of the msg
			getMessageContent(&msg, &msg_content);
			
			if(msg_content.control->destAddr == BROADCAST_ADDRESS) // test braodcast
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
						osMemoryPoolFree(memPool, msg.anyPtr);
					}
				}
			}
			else if(msg_content.control->srcAddr == MYADDRESS)//test source address
			{
				//to mac sender (databack)
				msg.type = DATABACK;
				prepareMessageQueue(&msg, &msg_content);
				macReceiverSendMsg(&msg, queue_macS_id);
			}
			else
			{
				if(msg_content.control->destAddr == MYADDRESS)//send to app and phy
				{	
					if(checkMessage(&msg, &msg_content) && gTokenInterface.connected)
					{
						if(msg_content.control->destSapi == CHAT_SAPI)
						{
							//generate msg to app with malloc
							generateFrameApp(&msg, &msg_to_app, *msg_content.length);
							msg_to_app.type = DATA_IND;
							prepareMessageQueue(&msg_to_app, &msg_content);
							macReceiverSendMsg(&msg, queue_chatR_id); //send it
							
							msg_content.status->read = 1;
						}
					}
					msg.type = TO_PHY;
					macReceiverSendMsg(&msg, queue_phyS_id);	//send ack message
				}
				else
				{
					//send to physical layer to give to the next
					msg.type = TO_PHY;
					macReceiverSendMsg(&msg, queue_phyS_id);	
				}
			}
		}
	}
}
//--------------------------------------------------------------------------------
// message management
//--------------------------------------------------------------------------------
bool checkMessage(struct queueMsg_t *msg, struct msg_content_t* msg_content)
{
	uint32_t sum = 0;
	for(uint8_t i=0; i<(*msg_content->length + 3); i++)
	{
		sum += ((uint8_t *)msg->anyPtr)[i];
	}
	if(msg_content->status->checksum == (sum & 0x3F))
	{
		msg_content->status->ack = true;
		return true;
	}
	else
	{
		msg_content->status->ack = false;
		return false;
	}
}

void generateFrameApp(struct queueMsg_t *msg, struct queueMsg_t *msg_to_app, uint8_t msg_length)
{
	msg_to_app->anyPtr = osMemoryPoolAlloc(memPool,osWaitForever);
	memset((void *)msg_to_app->anyPtr, 0, MAX_BLOCK_SIZE);
	
	for(uint32_t i=0; i<msg_length; i++)
	{
		((uint8_t *)msg_to_app->anyPtr)[i] = ((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG + i];//copy the msg
	}
}

void getMessageContent(struct queueMsg_t *msg, struct msg_content_t* msg_content)
{
		*msg_content->length = ((uint8_t *)msg->anyPtr)[2];
		msg_content->control = ((union control_t *)msg->anyPtr);
		msg_content->ptr = &((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG];
		msg_content->status = (union status_t *)(&((uint8_t *)msg->anyPtr)[OFFSET_TO_MSG + *msg_content->length]);
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
			msg->sapi = msg_content->control->srcAddr;
			break;
		
		case DATA_IND:
			msg->sapi = msg_content->control->srcSapi;
			msg->sapi = msg_content->control->srcAddr;
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
	osStatus_t status = osMessageQueuePut(queue, msg, osPriorityNormal, osWaitForever);
}


