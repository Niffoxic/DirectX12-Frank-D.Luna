#pragma once
#include <windows.h>

typedef struct _FULL_SCREEN_EVENT
{
	UINT Width;
	UINT Height;
} FULL_SCREEN_EVENT;

typedef struct _WINDOWED_SCREEN_EVENT
{
	UINT Width;
	UINT Height;
} WINDOWED_SCREEN_EVENT;

typedef struct _WINDOW_RESIZE_EVENT
{
	UINT Width;
	UINT Height;
} WINDOW_RESIZE_EVENT;


typedef struct _WINDOW_PAUSE_EVENT
{
	bool Paused;
} WINDOW_PAUSE_EVENT;
