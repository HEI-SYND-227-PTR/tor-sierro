#include "main.h"
#include <stdio.h>

#define TIMEOUT_QUEUE 10000



void MacReceiver(void *argument)
{
	struct queueMsg_t *msg;
	struct TOKENINTERFACE gTokenInterface;
	bool tokenOwned = 0;
	bool isToken = 0;
	
	while(1)
	{
		//----------------------------------------------------------------------------
		// Read queue					
		//----------------------------------------------------------------------------
		
		
		osMessageQueueGet(queue_macS_id, msg, NULL, osWaitForever);
		
		
		//----------------------------------------------------------------------------
		// Check message type				
		//----------------------------------------------------------------------------
		switch(msg->type) 
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
					//token receive from other and I own the token
					//update token
				}
				break;
				
			default:
				//chit!!!!
				break;
		}
		
		
		
		//----------------------------------------------------------------------------
		// Check message type				
		//----------------------------------------------------------------------------
		
		//Keep the token until there is msg in the queue_macS_id
		
		//send to phys
	}
}
