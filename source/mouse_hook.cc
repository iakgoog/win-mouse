#include <stdexcept>
#include "mouse_hook.h"
#include <iostream>  
#include <stdlib.h>  
#include <string> 

using namespace std;

HHOOK lowLevelMouseHook = NULL;
HHOOK CBTHook = NULL;

HINSTANCE dllInstance = NULL;

static HANDLE MouseThread;
static HANDLE CBTThread;

LRESULT CALLBACK LowLevelMouseProc(int, WPARAM, LPARAM);
LRESULT CALLBACK CBTProc (int, WPARAM, LPARAM);

MouseHookRef MouseHookRegister(MouseHookCallback callback, void* data) {
	return MouseHookManager::GetInstance()->Register(callback, data);
}

void MouseHookUnregister(MouseHookRef ref) {
	MouseHookManager::GetInstance()->Unregister(ref);
}

MouseHookManager* MouseHookManager::GetInstance() {
	static MouseHookManager manager;
	return &manager;
}

MouseHookManager::MouseHookManager() {
	running = false;
	pause = false;
	thread_id = NULL;

	listeners = new std::list<MouseHookRef>();
	uv_mutex_init(&event_lock);
	uv_mutex_init(&init_lock);
	uv_cond_init(&init_cond);
}

MouseHookManager::~MouseHookManager() {
	if(!listeners->empty()) Stop();

	delete listeners;
	uv_mutex_destroy(&event_lock);
	uv_mutex_destroy(&init_lock);
	uv_cond_destroy(&init_cond);
}

DWORD WINAPI MouseAsync(LPVOID lpParam) {
	lowLevelMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, (HINSTANCE) NULL, 0);
	if (lowLevelMouseHook == NULL) {
		return false;
	}
	MSG ThreadMsg;
	while (GetMessage(&ThreadMsg, NULL, 0, 0)) {

	};
	return true;
}

DWORD WINAPI CBTAsync(LPVOID lpParam) {
	CBTHook = SetWindowsHookEx(WH_CBT, CBTProc, dllInstance, 0);
	if (CBTHook == NULL) {
		return false;
	}
	MSG ThreadMsg;
	while (GetMessage(&ThreadMsg, NULL, 0, 0)) {

	};
	return true;
}

MouseHookRef MouseHookManager::Register(MouseHookCallback callback, void* data) {
	uv_mutex_lock(&event_lock);
	
	bool empty = listeners->empty();

	MouseHookRef entry = new MouseHookEntry();
	entry->callback = callback;
	entry->data = data;
	listeners->push_back(entry);

	uv_mutex_unlock(&event_lock);	

	if(empty) {
		MouseThread = CreateThread(NULL, 0, MouseAsync, NULL, 0, NULL);
	}

  CBTThread = CreateThread(NULL, 0, CBTAsync, NULL, 0, NULL);

	return entry;
}

void MouseHookManager::Unregister(MouseHookRef ref) {
	uv_mutex_lock(&event_lock);

	listeners->remove(ref);
	delete ref;
	bool empty = listeners->empty();

	uv_mutex_unlock(&event_lock);
	
	if(empty) Stop();
}

void MouseHookManager::_HandleEvent(WPARAM type, POINT point) {
	if (!pause) {
		uv_mutex_lock(&event_lock);

		for(std::list<MouseHookRef>::iterator it = listeners->begin(); it != listeners->end(); it++) {
			(*it)->callback(type, point, (*it)->data);
		}

		uv_mutex_unlock(&event_lock);
	}
}

void MouseHookManager::_HandlePause(bool value) {
	pause = value;
}

void MouseHookManager::Stop() {
	uv_mutex_lock(&init_lock);

	UnhookWindowsHookEx(lowLevelMouseHook);
  UnhookWindowsHookEx(CBTHook);

  TerminateThread(MouseThread, 0);
  TerminateThread(CBTThread, 0);

	while(thread_id == NULL) uv_cond_wait(&init_cond, &init_lock);
	DWORD id = thread_id;
	
	uv_mutex_unlock(&init_lock);

	PostThreadMessage(id, WM_STOP_MESSAGE_LOOP, NULL, NULL);
	uv_thread_join(&thread);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reasonForCall, LPVOID reserved) {
	dllInstance = (HINSTANCE)module;
	return TRUE;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
	if(nCode >= 0) {
		MSLLHOOKSTRUCT* data = (MSLLHOOKSTRUCT*) lParam;
		POINT point = data->pt;

		MouseHookManager::GetInstance()->_HandleEvent(wParam, point);
	}

	return CallNextHookEx(lowLevelMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK CBTProc(int code, WPARAM wParam, LPARAM lParam) {
  if(code == HCBT_CREATEWND) {
    HWND hwnd = (HWND)wParam;
    CHAR name[1024] = {0};
    GetClassName(hwnd, name, sizeof(name));
    string className(name);
    // cout << ">> window class: " << className << endl;
    if(className == "#32768") { // The class for a menu.
      MouseHookManager::GetInstance()->_HandlePause(true);
    }
  } else if(code == HCBT_DESTROYWND) {
    HWND hwnd = (HWND)wParam;
    CHAR name[1024] = {0};
    GetClassName(hwnd, name, sizeof(name));
    string className(name);
    if(className == "#32768") {
      MouseHookManager::GetInstance()->_HandlePause(false);
    }
  }
	return CallNextHookEx(CBTHook, code, wParam, lParam);
}