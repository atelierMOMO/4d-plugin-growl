/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : Growl
 #	author : miyako
 #	2016/07/22
 #
 # --------------------------------------------------------------------------------*/

#include "4DPluginAPI.h"
#include "4DPlugin.h"

@interface Listener : NSObject <GrowlApplicationBridgeDelegate>
{

}

- (void) growlNotificationWasClicked:(id)clickContext;
- (void) growlNotificationTimedOut:(id)clickContext;
- (NSString *) applicationNameForGrowl;
- (NSDictionary *) registrationDictionaryForGrowl;
- (void)call:(notification_type_t)type event:(NSString *)context;

@end

namespace Growl
{
	Listener *listener = nil;
	
	//constants
	process_name_t MONITOR_PROCESS_NAME = (PA_Unichar *)"$\0G\0R\0O\0W\0L\0\0\0";
	process_stack_size_t MONITOR_PROCESS_STACK_SIZE = 0;
	NSString *defaultNotificationName = @"name";
	NSString *defaultApplicationName = @"4D";
	
	//context management
	std::vector<notification_type_t> notificationTypes;
	std::vector<CUTF16String> notificationContexts;
	
	//callback management
	C_TEXT callbackMethodName;
	method_id_t callbackMethodId = 0;
	process_number_t monitorProcessId = 0;
	bool shouldCallMethod = false;
	bool shouldKillListenerLoop = false;
	
	//dict
	NSArray *defaultNotifications = [NSArray arrayWithObject:defaultNotificationName];
	NSDictionary *regDict = [NSDictionary
		dictionaryWithObjects:
		[NSArray arrayWithObjects:
		defaultApplicationName,
		defaultNotifications,
		defaultNotifications,
		nil]
		forKeys:
		[NSArray arrayWithObjects:
		GROWL_APP_NAME,
		GROWL_NOTIFICATIONS_ALL,
		GROWL_NOTIFICATIONS_DEFAULT,
		nil]];
};

@implementation Listener

- (NSString *) applicationNameForGrowl
{
	return Growl::defaultApplicationName;
}

- (NSDictionary *) registrationDictionaryForGrowl
{
		return Growl::regDict;
}

- (void) growlNotificationWasClicked:(id)clickContext
{
	[self call:NotificationWasClicked event:clickContext];
}

- (void) growlNotificationTimedOut:(id)clickContext
{
	[self call:NotificationTimedOut event:clickContext];
}

- (void)call:(notification_type_t)type event:(NSString *)context
{
	if(Growl::shouldCallMethod)
	{
		Growl::notificationTypes.push_back(type);
		
		CUTF16String s;
		uint32_t len = [context length];
		uint32_t size = (len * sizeof(PA_Unichar)) + sizeof(PA_Unichar);
		std::vector<uint8_t> buf(size);
		if([context getCString:(char *)&buf[0] maxLength:size encoding:NSUnicodeStringEncoding])
		{
			s = CUTF16String((const PA_Unichar *)&buf[0], len);
		}
		Growl::notificationContexts.push_back(s);
		
		PA_UnfreezeProcess(Growl::monitorProcessId);
	}
}
@end

#pragma mark -

void generateUuid(C_TEXT &returnValue)
{
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
	returnValue.setUTF16String([[[NSUUID UUID]UUIDString]stringByReplacingOccurrencesOfString:@"-" withString:@""]);
#else
	CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
	NSString *uuid_str = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, uuid);
	returnValue.setUTF16String([uuid_str stringByReplacingOccurrencesOfString:@"-" withString:@""]);
#endif
}

#pragma mark -

void listenerLoop()
{
	Growl::listener = [GrowlApplicationBridge growlDelegate];
	
	Growl::listener = [[Listener alloc]init];
	[GrowlApplicationBridge setGrowlDelegate:Growl::listener];

	Growl::shouldKillListenerLoop = false;
	
	while(!Growl::shouldKillListenerLoop)
	{
		PA_YieldAbsolute();
		while(Growl::notificationTypes.size())
		{
			PA_YieldAbsolute();
			
			C_TEXT processName;
			generateUuid(processName);
			PA_NewProcess((void *)listenerLoopExecute,
										Growl::MONITOR_PROCESS_STACK_SIZE,
										(PA_Unichar *)processName.getUTF16StringPtr());
		
			if(Growl::shouldKillListenerLoop)
				break;
		}
	
		if(!Growl::shouldKillListenerLoop)
		{
			PA_FreezeProcess(PA_GetCurrentProcessNumber());
		}
	}
	
	[GrowlApplicationBridge setGrowlDelegate:nil];
	[Growl::listener release];
	
	Growl::notificationTypes.clear();
	Growl::notificationContexts.clear();
	Growl::callbackMethodName.setUTF16String((PA_Unichar *)"\0\0", 0);
	Growl::callbackMethodId = 0;
	Growl::monitorProcessId = 0;
	Growl::shouldCallMethod = false;
	
	PA_KillProcess();
}

void listenerLoopStart()
{
	if(!Growl::monitorProcessId)
	{
		Growl::monitorProcessId = PA_NewProcess((void *)listenerLoop, Growl::MONITOR_PROCESS_STACK_SIZE, Growl::MONITOR_PROCESS_NAME);
	}
}

void listenerLoopFinish()
{
	if(Growl::monitorProcessId)
	{
		//set flag
		Growl::shouldKillListenerLoop = true;
		
		//tell listener to die
//		while(Growl::monitorProcessId)
//		{
//			PA_YieldAbsolute();
			PA_UnfreezeProcess(Growl::monitorProcessId);
//		}
	
	}
}

void listenerLoopExecute()
{
	std::vector<notification_type_t>::iterator t = Growl::notificationTypes.begin();
	std::vector<CUTF16String>::iterator c = Growl::notificationContexts.begin();
	
	notification_type_t type = *t;
	CUTF16String clickContext = *c;
	
	if(Growl::callbackMethodId)
	{
		PA_Variable	params[2];
		params[0] = PA_CreateVariable(eVK_Longint);
		params[1] = PA_CreateVariable(eVK_Unistring);
		
		PA_SetLongintVariable(&params[0], type);
		PA_Unistring context = PA_CreateUnistring((PA_Unichar *)clickContext.c_str());
		PA_SetStringVariable(&params[1], &context);
		
		//the method could be paused or traced
		Growl::notificationTypes.erase(t);
		Growl::notificationContexts.erase(c);
		
		PA_ExecuteMethodByID(Growl::callbackMethodId, params, 2);
		
		PA_ClearVariable(&params[0]);
		PA_ClearVariable(&params[1]);
	}else{
		//the method could have been removed
		Growl::notificationTypes.erase(t);
		Growl::notificationContexts.erase(c);
	}
}

#pragma mark -

void StartNotification()
{
	if(!Growl::shouldKillListenerLoop)
	{
		PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listenerLoopStart, NULL);
	}
}

void StopNotification()
{
	if(!Growl::shouldKillListenerLoop)
	{
		PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listenerLoopFinish, NULL);
	}
}

#pragma mark -

bool IsProcessOnExit()
{
	C_TEXT name;
	PA_long32 state, time;
	PA_GetProcessInfo(PA_GetCurrentProcessNumber(), name, &state, &time);
	CUTF16String procName(name.getUTF16StringPtr());
	CUTF16String exitProcName((PA_Unichar *)"$\0x\0x\0\0\0");
	return (!procName.compare(exitProcName));
}

void OnStartup()
{

}

void OnCloseProcess()
{
	if(IsProcessOnExit())
	{
		StopNotification();
	}
}

#pragma mark -

void PluginMain(PA_long32 selector, PA_PluginParameters params)
{
	try
	{
		PA_long32 pProcNum = selector;
		sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
		PackagePtr pParams = (PackagePtr)params->fParameters;

		CommandDispatcher(pProcNum, pResult, pParams); 
	}
	catch(...)
	{

	}
}

void CommandDispatcher (PA_long32 pProcNum, sLONG_PTR *pResult, PackagePtr pParams)
{
	switch(pProcNum)
	{
		case kInitPlugin :
		case kServerInitPlugin :
			OnStartup();
			break;
			
		case kCloseProcess :
			OnCloseProcess();
			break;
// --- Growl

		case 1 :
			Growl_Set_notification_method(pResult, pParams);
			break;

		case 2 :
			Growl_SET_MIST_ENABLED(pResult, pParams);
			break;

		case 3 :
			Growl_Get_mist_enabled(pResult, pParams);
			break;

		case 4 :
			Growl_POST_NOTIFICATION(pResult, pParams);
			break;

		case 5 :
			Growl_Get_notification_method(pResult, pParams);
			break;

	}
}

// ------------------------------------- Growl ------------------------------------

#pragma mark -

void Growl_SET_MIST_ENABLED(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT Param1;
	
	Param1.fromParamAtIndex(pParams, 1);
	
	[GrowlApplicationBridge setShouldUseBuiltInNotifications:Param1.getIntValue()];
}

void Growl_Get_mist_enabled(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT returnValue;
	
	returnValue.setIntValue([GrowlApplicationBridge shouldUseBuiltInNotifications]);
	
	returnValue.setReturn(pResult);
}

void Growl_POST_NOTIFICATION(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT Param1;
	C_TEXT Param2;
	C_LONGINT Param4;
	C_LONGINT Param5;
	C_TEXT Param6;
	C_TEXT Param7;

	Param1.fromParamAtIndex(pParams, 1);//title
	Param2.fromParamAtIndex(pParams, 2);//description
	//icon is managed directly
	Param4.fromParamAtIndex(pParams, 4);//priority (-2 to +2)
	Param5.fromParamAtIndex(pParams, 5);//sticky
	Param6.fromParamAtIndex(pParams, 6);//click-context
	Param7.fromParamAtIndex(pParams, 7);//identifier

	NSString *title = Param1.copyUTF16String();
	NSString *description = Param2.copyUTF16String();
	signed int priority = Param4.getIntValue();
	priority = (priority < -2) ? -2 :priority; //-2 to +2
	priority = (priority >  2) ?  2 :priority; //-2 to +2
	BOOL isSticky = (BOOL)Param5.getIntValue();
	NSString *clickContext = Param6.copyUTF16String();
	NSString *identifier = Param7.copyUTF16String();
	
	StartNotification();
	
	@autoreleasepool
	{
	//icon
	PA_Picture p = *(PA_Picture *)(pParams[2]);
	CGImageRef cgImage = (CGImageRef)PA_CreateNativePictureForScreen(p);
	NSData *iconData = nil;
		
	if(cgImage)
	{
		NSImage *image = [[NSImage alloc]initWithCGImage:cgImage size:NSZeroSize];
		CFRelease(cgImage);
		iconData = [image TIFFRepresentation];
		[image release];
	}
	
	[GrowlApplicationBridge notifyWithTitle:title
															description:description
												 notificationName:Growl::defaultNotificationName
																 iconData:iconData
																 priority:priority
																 isSticky:isSticky
														 clickContext:clickContext
															 identifier:[identifier length] == 0 ? nil : identifier];
	}
	
	//cleanup
	[title release];
	[description release];
	[clickContext release];
	[identifier release];
}

#pragma mark -

void Growl_Get_notification_method(sLONG_PTR *pResult, PackagePtr pParams)
{
	Growl::callbackMethodName.setReturn(pResult);
}

void Growl_Set_notification_method(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT Param1;
	
	Param1.fromParamAtIndex(pParams, 1);
	
	if(!Param1.getUTF16Length())
	{
		
		Growl::shouldCallMethod = false;
		
	}else{
		
		method_id_t methodId = PA_GetMethodID((PA_Unichar *)Param1.getUTF16StringPtr());
		
		if(methodId)
		{
			if(methodId != Growl::callbackMethodId)
			{
				Growl::callbackMethodName.setUTF16String(Param1.getUTF16StringPtr(), Param1.getUTF16Length());
				Growl::callbackMethodId = methodId;
			}
			Growl::shouldCallMethod = true;
		}
	}
}

