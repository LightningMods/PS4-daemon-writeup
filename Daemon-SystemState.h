        enum SystemState
        {
            INVALID = 0,
            INITIALIZING = 10,
            SHUTDOWN_ON_GOING = 100,
            POWER_SAVING = 200,
            SUSPEND_ON_GOING = 300,
            MAIN_ON_STANDBY = 500,
            WORKING = 1000
        };

        enum TriggerCode
        {
            INVALID = 0,
            MISC = 1,
            HDMI_CEC = 2,
            REMOTE_PLAY = 100,
            COMPANION_APP = 101,
            REMOTE_PLAY_NP_PUSH = 102,
            UPDATER_SERVICE = 103,
            BGFT = 104,
            BG_DAILY_CHECK = 105,
            NP_EVENT_JOIN = 106,
            NP_EVENT_INFO_UPDATE = 107,
            SP_CONNECT = 108
        };


///////////////////// USEFUL //////////////////

int sceSystemStateMgrGetCurrentState(void);
int sceSystemStateMgrWakeUp(enum TriggerCode code);
int sceSystemStateMgrEnterStandby(int a1);
bool sceSystemStateMgrIsStandbyModeEnabled(void);
int sceSystemStateMgrGetTriggerCode(void);

///////////////////////////////////////////////


bool isRestMode()
{
  return (unsigned int)sceSystemStateMgrGetCurrentState() == MAIN_ON_STANDBY;
}

bool IsOn()
{
  return (unsigned int)sceSystemStateMgrGetCurrentState() == WORKING;
}

bool IsINITIALIZING()
{
  return (unsigned int)sceSystemStateMgrGetCurrentState() == INITIALIZING;
}
