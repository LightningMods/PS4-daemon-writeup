
## DAEMON Writeup ##

Welcome to my writeup of how i found and implimented my own PS4 Daemon proc.


### Finding how Daemons work on PS4 ###

when i first began i noticed when you call this functions from a game

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

but how? good queston.

The sub function incharge of spawning it is 

```
sub_1020(...)

```

![pic1](https://github.com/LightningMods/PS4-daemon-writeup/blob/main/ida_1.png)

which calls many other check functions but also does

```sceLncUtilStartLaunchAppByTitleId("NPXS22010",....)```

as you imagine i was thinking but how..

first things first finding the structs which it seems to use
after looking in shellui i found them!

```

enum Flag
{
    Flag_None = 0,
    SkipLaunchCheck = 1,
    SkipResumeCheck = 1,
    SkipSystemUpdateCheck = 2,
    RebootPatchInstall = 4,
    VRMode = 8,
    NonVRMode = 16
};

typedef struct _LncAppParam
{
	u32 sz;
	u32 user_id;
	u32 app_opt;
	u64 crash_report;
	Flag check_flag;
}
LncAppParam;
```

An example of what ShellUI does in case anyone wants to hook it in mono via LNC class

```

LncUtil.LaunchAppParam launchAppParam = default(LncUtil.LaunchAppParam);

byte[] array = new byte[3];
array[0] = 45;
array[1] = 105;
byte[] array2 = array;
LncUtil.LaunchApp("NPXS21018", array2, array2.Length, ref launchAppParam);
```

just like sceSystemServiceLaunchApp these seem to have the same protos in the `libSceSystemService` module


![pic2](https://github.com/LightningMods/PS4-daemon-writeup/blob/main/ida_2.png)

```
int (*sceSystemServiceLaunchApp)(const char* titleId, const char* argv[], LncAppParam* param);
int (*sceLncUtilStartLaunchAppByTitleId)(const char* titleId, const char* argv[], LncAppParam* param);
//returns the Systems runetime app id 
u32 (*sceLncUtilLaunchApp)(const char* titleId, const char* argv[], LncAppParam* param);
```

`sceSystemServiceLaunchApp` calls `sceLncUtilStartLaunchApp` which then calls the IPC iirc



after looking though other PS4 daemons, i noticed they are all similar and use gdd as their sfo catagory
and they are installed to.

(I'll save you the details on why that is)


```/system/vsh/app/TITLE_ID/```

Next we have to copy all our daemon files including eboot which is signed with System/GL Auth
(which makes them limited on memory and forces you to manually load all modules)
via the LoadInternal function flatz used for his GL app)

You can find more info regarding internal module ids here...

[Devwiki internal IDs](https://www.psdevwiki.com/ps4/Libraries#Internal_sysmodule_libraries)

```
make_fself.py --auth-info 010000000010003800000000001c004000ff00000000008000000000000000000000000000000000000000c000400040000000000000008000000000000000f00040ffff000000f000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000 cd.elf eboot.bin && copy eboot.bin I:\

```

so next i did the following after i remounted the System partition as RW via nmount


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

which can also be found where the ps4 keeps a list of errors

so we need to first initialize it using

![pic3](https://github.com/LightningMods/PS4-daemon-writeup/blob/main/ida_3.png)



```
int (*sceLncUtilInitialize)(void);
```
which i found by backtracing shellcore to

```
sceSystemServiceInitializeForShellCore()
```

Note: you cannot call `sceSystemServiceInitializeForShellCore()` without being shellcore/having proper auth
Or you will get EPERM.




now we can finally run our daemon as follows

Note: you can use LncUtil stubs instead with your SDK of choice to link these funcs
But at the time I didn't make the stubs yet so I did it manually.


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
	     goto fatal_error;
								    
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

	LncAppParam param;
	param.sz = sizeof(LncAppParam);
	param.user_id = userIdList.userId[0];
	param.app_opt = 0;
	param.crash_report = 0;
	param.check_flag = 0; //Flag_None

	klog("sceLncUtilInitialize %x\n", sceLncUtilInitialize());

        u32 appid = sceLncUtilLaunchApp("LMSS00001", 0, &param)
        //Error handling
        if (!appid & 0x6) // app_ids start with 0x6XXXXXXX
        
```

and after all our work Success! iv successfully launched my own daemon, mine took awhile to make as i have a RPC Server thats does ALOT.

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

(Also works for Launching Games see the trailer for more info)





