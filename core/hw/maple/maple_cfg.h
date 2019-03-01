#pragma once
#include "types.h"
#include "maple_devs.h"

enum PlainJoystickButtonId
{
	PJBI_B = 1,
	PJBI_A = 2,
	PJBI_START = 3,
	PJBI_DPAD_UP = 4,
	PJBI_DPAD_DOWN = 5,
	PJBI_DPAD_LEFT = 6,
	PJBI_DPAD_RIGHT = 7,
	PJBI_Y = 9,
	PJBI_X = 10,

	PJBI_Count=16
};

enum PlainJoystickAxisId
{
	PJAI_X1 = 0,
	PJAI_Y1 = 1,
	PJAI_X2 = 2,
	PJAI_Y2 = 3,

	PJAI_Count = 4
};

enum PlainJoystickTriggerId
{
	PJTI_L = 0,
	PJTI_R = 1,

	PJTI_Count = 2
};

struct PlainJoystickState
{
	PlainJoystickState()
	{
		kcode=0xFFFFFFFF;
		joy[0]=joy[1]=joy[2]=joy[3]=0x80;
		trigger[0]=trigger[1]=0;
	}

	u32 kcode;

	u8 joy[PJAI_Count];
	u8 trigger[PJTI_Count];
};

struct IMapleConfigMap
{
   virtual void SetVibration(u32 value, u32 max_duration) = 0;
	virtual void GetInput(PlainJoystickState* pjs)=0;
	virtual void GetAbsolutePosition(f32 *px, f32 *py) = 0;
	virtual void GetMouse(u32 *buttons, f32 *delta_x, f32 *delta_y, f32 *delta_wheel) = 0;
	virtual void SetImage(void* img)=0;
	virtual ~IMapleConfigMap() {}
};

void mcfg_CreateDevices();
void mcfg_Create(MapleDeviceType type,u32 bus,u32 port, int player_num = -1);
void mcfg_DestroyDevices();
void mcfg_DestroyDevice(int i, int j);
void mcfg_SerializeDevices(void **data, unsigned int *total_size);
void mcfg_UnserializeDevices(void **data, unsigned int *total_size);
