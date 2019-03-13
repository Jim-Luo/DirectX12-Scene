// DXProject.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "DXProject.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInstance, PSTR mCmdLine, int mCmdShow)
{
	Campfire c = Campfire();

	if (!c.initWindows(hInstance, mCmdShow))
		return 0;
	if (!c.initDirect3D())
		return 0;
	return c.Run();

}