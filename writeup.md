
## DAEMON Writeup ##

Welcome to my writeup of how i found and implimented my own PS4 Daemon proc.


### Finding how Daemons work on PS4 ###

when i first begain i noticed when you call this functions from a game

```sceCommonDialogInitialize()```

you actually spawn a daemon of CDLG type

```[SceLncService] launchApp(NPXS22010)
[SceLncService] category={gdg} VRMode={0,0}
[SceLncService] hnm,psnf,pc,tk,ns={0,0,0,0,0} appBootMode={-1}
[SceLncService] appType={SCE_LNC_APP_TYPE_CDLG} [] appVer={00.00}
[SceLncService] Num. of logged-in users is 1
[SceLncService] spawnApp
[Syscore App] createApp NPXS22010

[SceShellCore] FMEM 143.2/ 243.4 NPXS22010 SceCdlgApp
```

but how? good queston

The sub function incharge of spawning it is 

```
sub_1020()

```

![pic1](https://github.com/LightningMods/PS4-daemon-writeup/blob/main/ida_1.png)

which calls many other functions to check things but also does

```sceLncUtilStartLaunchAppByTitleId("NPXS22010",....)```

as you imagine i was thinking but how..

first things first finding the structs which it seems to use
after looking in shellui i found them!

```
typedef struct _LaunchAppParam
{
	uint32_t size;
	int32_t user_id;
	int32_t app_attr;
	int32_t enable_crash_report;
	uint64_t check_flag;
}
LaunchAppParam;
```

just like sceSystemServiceLaunchApp these seem to have the same protos in the `libSceSystemService` module


![pic2](https://github.com/LightningMods/PS4-daemon-writeup/blob/main/ida_2.png)

```
int (*sceSystemServiceLaunchApp)(const char* titleId, const char* argv[], LaunchAppParam* param);
int (*sceLncUtilStartLaunchAppByTitleId)(const char* titleId, const char* argv[], LaunchAppParam* param);
int (*sceLncUtilStartLaunchApp)(const char* titleId, const char* argv[], LaunchAppParam* param);
```

`sceSystemServiceLaunchApp` calls `sceLncUtilStartLaunchApp` which then calls the IPC iirc



after looking though other PS4 daemons i noticed they are all similar and use gdd as their sfo catagory
and they are installed to 

```/system/vsh/app/TITLE_ID/```

Next we have to copy all our daemon files including eboot which is signed with System/GL Auth
(which makes them limited on memory and forces you to manually load all modules)

so next i did the following after remount the System partition as RW via nmount


```
/system/vsh/app/NPXS20119/sce_sys/param.sfo -> /system/vsh/app/LMSS0001/sce_sys/param.sfo
eboot.bin -> /system/vsh/app/LMSS0001/eboot.bin
etc
```

but when i tried running my function i had gotten 

`
SCE_LNC_UTIL_ERROR_NOT_INITIALIZED 
0x80940001
`

so we need to first initlize it using

![pic3](https://github.com/LightningMods/PS4-daemon-writeup/blob/main/ida_3.png)



```
int (*sceLncUtilInitialize)();
```
which i found by backtracing shellcore to 

```
sceSystemServiceInitializeForShellCore()
```




now we can finally run our daemon as follows

```
     sys_dynlib_load_prx("/system/common/lib/libSceSystemService.sprx", &libcmi);

    int serres = sys_dynlib_dlsym(libcmi, "sceSystemServiceLaunchApp", &sceSystemServiceLaunchApp_pointer);
    if (!serres)
    {
                 klog("sceSystemServiceLaunchApp-pointer %p resolved from PRX\n", sceSystemServiceLaunchApp_pointer);

	sceLncUtilInitialize = (void*)(sceSystemServiceLaunchApp_pointer + 0x1110);

	klog("sceLncUtilInitialize %p resolved from PRX\n", sceLncUtilInitialize);
        sceLncUtilLaunchApp = (void*)(sceSystemServiceLaunchApp_pointer + 0x1130);

	klog("sceLncUtilLaunchApp %p resolved from PRX\n", sceLncUtilLaunchApp);

	if(!sceLncUtilInitialize || !sceLncUtilLaunchApp)
	printf("error\n");
								    
        OrbisUserServiceInitializeParams params;
	memset(&params, 0, sizeof(params));
	params.priority = 700;

	klog("ret %x\n", sceUserServiceInitialize(&params));

	OrbisUserServiceLoginUserIdList userIdList;

	klog("ret %x\n", sceUserServiceGetLoginUserIdList(&userIdList));

	for (int i = 0; i < 4; i++)
	{
		if (userIdList.userId[i] != 0xFF)
		{
		  klog("[%i] User ID 0x%x\n", i, userIdList.userId[i]);
		}
	}

	LaunchAppParam param;
	param.size = sizeof(LaunchAppParam);
	param.user_id = userIdList.userId[0];
	param.app_attr = 0;
	param.enable_crash_report = 0;
	param.check_flag = 0;

	klog("sceLncUtilInitialize %x\n", sceLncUtilInitialize());

       uint64_t l2 = sceLncUtilLaunchApp("LMSS00001", 0, &param);
```

and after all our work Success! iv successfully launched my own daemon, mine took awhile to make as i have a RPC Server thats does ALOT

```

[SceLncService] launchApp(LMSS0001)
[SceLncService] category={gdd} VRMode={1,0}
[SceLncService] hnm,psnf,pc,tk,ns={0,0,0,0,0} appBootMode={-1}
[SceLncService] appType={SCE_LNC_APP_TYPE_DAEMON} [] appVer={00.00}
[SceLncService] Num. of logged-in users is 1
[SceLncService] spawnApp
[Syscore App] createApp LMSS0001
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_SYSTEM_SERVICE
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_USER_SERVICE
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_NETCTL
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_NET
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_HTTP
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_SSL
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_SYS_CORE
[DEBUG] Started Internal Module 0x80000018
[DEBUG] Started Internal Module SCE_SYSMODULE_INTERNAL_NETCTL
[DEBUG] Starting System FTP Process on Port 999
Client list mutex UID: 0x802CD4E0
Server thread started!
Server thread UID: 0x812189C0
Server socket fd: 5
starting KLOG Thread on port 998
sceNetBind(): 0x00000000
sceNetListen(): 0x00000000
Waiting for incoming connections...
```






