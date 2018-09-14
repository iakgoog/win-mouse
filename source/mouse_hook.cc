#include <stdexcept>
#include "mouse_hook.h"
#include <iostream>  
#include <stdlib.h>  
#include <string> 

using namespace std;

HHOOK lowLevelMouseHook = NULL;

HWINEVENTHOOK g_hook = NULL;

HINSTANCE dllInstance = NULL;

LRESULT CALLBACK LowLevelMouseProc(int, WPARAM, LPARAM);

void CALLBACK HandleWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);

void RunThread(void* arg) {
  MouseHookManager* mouse = (MouseHookManager*) arg;
  mouse->_Run();
}

MouseHookRef MouseHookRegister(MouseHookCallback callback, void* data) {
  return MouseHookManager::GetInstance()->Register(callback, data);
}

void MouseHookUnregister(MouseHookRef ref) {
  MouseHookManager::GetInstance()->Unregister(ref);
}

void Unhook() {
  if (lowLevelMouseHook != NULL) {
    UnhookWindowsHookEx(lowLevelMouseHook);
    lowLevelMouseHook = NULL;
  }

  if (g_hook != NULL) {
    UnhookWinEvent(g_hook);
    g_hook = NULL;
  }
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

MouseHookRef MouseHookManager::Register(MouseHookCallback callback, void* data) {
  uv_mutex_lock(&event_lock);
  
  bool empty = listeners->empty();

  MouseHookRef entry = new MouseHookEntry();
  entry->callback = callback;
  entry->data = data;
  listeners->push_back(entry);

  uv_mutex_unlock(&event_lock);	

  if(empty) uv_thread_create(&thread, RunThread, this);

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
  uv_mutex_lock(&event_lock);

  for(std::list<MouseHookRef>::iterator it = listeners->begin(); it != listeners->end(); it++) {
    (*it)->callback(type, point, (*it)->data);
  }

  uv_mutex_unlock(&event_lock);
}

void MouseHookManager::_Run() {
  MSG msg;
  BOOL val;

  uv_mutex_lock(&init_lock);

  PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
  thread_id = GetCurrentThreadId();
  
  uv_cond_signal(&init_cond);
  uv_mutex_unlock(&init_lock);

  lowLevelMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, (HINSTANCE) NULL, 0);

  g_hook = SetWinEventHook(
    EVENT_SYSTEM_MENUSTART, EVENT_SYSTEM_MENUEND,  // Range of events (4 to 5).
    NULL,                                          // Handle to DLL.
    HandleWinEvent,                                // The callback.
    0, 0,              // Process and thread IDs of interest (0 = all)
    WINEVENT_OUTOFCONTEXT); // Flags.

  while((val = GetMessage(&msg, NULL, 0, 0)) != 0) {
    if(val == -1) throw std::runtime_error("GetMessage failed (return value -1)");
    if(msg.message == WM_STOP_MESSAGE_LOOP) break;
  }

  UnhookWindowsHookEx(lowLevelMouseHook);
  UnhookWinEvent(g_hook);

  uv_mutex_lock(&init_lock);
  thread_id = NULL;
  uv_mutex_unlock(&init_lock);
}

void MouseHookManager::_HandlePause(bool value) {
  pause = value;
}

void MouseHookManager::Stop() {
  uv_mutex_lock(&init_lock);

  while(thread_id == NULL) uv_cond_wait(&init_cond, &init_lock);
  DWORD id = thread_id;
  
  uv_mutex_unlock(&init_lock);

  PostThreadMessage(id, WM_STOP_MESSAGE_LOOP, NULL, NULL);
  uv_thread_join(&thread);
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if(nCode >= 0) {
    MSLLHOOKSTRUCT* data = (MSLLHOOKSTRUCT*) lParam;
    POINT point = data->pt;

    MouseHookManager::GetInstance()->_HandleEvent(wParam, point);
  }

  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void CALLBACK HandleWinEvent(HWINEVENTHOOK hook, DWORD event, HWND hwnd, 
                             LONG idObject, LONG idChild, 
                             DWORD dwEventThread, DWORD dwmsEventTime)
{
    switch (event) {
      case EVENT_SYSTEM_MENUSTART:
        // cout << "EVENT_SYSTEM_MENUSTART >>>>>>>>>>>>>>>>>>>>>>>>" << endl;
        if (lowLevelMouseHook != NULL) {
          UnhookWindowsHookEx(lowLevelMouseHook);
          lowLevelMouseHook = NULL;
        }
        break;
      case EVENT_SYSTEM_MENUEND:
        // cout << "<<<<<<<<<<<<<<<<<<<<<<<< EVENT_SYSTEM_MENUEND" << endl;
        if (lowLevelMouseHook == NULL) {
          lowLevelMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, (HINSTANCE) NULL, 0);
        }
        break;
    }
}

BOOL WINAPI DllMain(HMODULE module, DWORD reasonForCall, LPVOID reserved) {
  switch (reasonForCall) {
    case DLL_PROCESS_ATTACH:
      dllInstance = (HINSTANCE)module;
      break;

    case DLL_PROCESS_DETACH:
      Unhook();
      break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;
  }

  return TRUE;
}