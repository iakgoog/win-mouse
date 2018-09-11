#ifndef _MOUSE_HOOK_H
#define _MOUSE_HOOK_H

#define WIN32_LEAN_AND_MEAN 

#include <uv.h>
#include <node.h>
#include <Windows.h>
#include <list>

#define WM_STOP_MESSAGE_LOOP (WM_USER+0)

typedef void (*MouseHookCallback)(WPARAM, POINT, void*);

typedef struct {
	MouseHookCallback callback;
	void* data;
} MouseHookEntry;

typedef MouseHookEntry* MouseHookRef;

MouseHookRef MouseHookRegister(MouseHookCallback callback, void* data);
void MouseHookUnregister(MouseHookRef);

class MouseHookManager {
	public:
		static MouseHookManager* GetInstance();

		MouseHookRef Register(MouseHookCallback, void*);
		void Unregister(MouseHookRef);

		void _Run();
		void _HandleEvent(WPARAM, POINT);
    void _HandlePause(bool);

	private:
		bool running;
    bool pause;
		DWORD thread_id;
		std::list<MouseHookRef>* listeners;
		uv_mutex_t event_lock;
		uv_cond_t init_cond;
		uv_mutex_t init_lock;
		uv_thread_t thread;

		MouseHookManager();
		~MouseHookManager();
		
		void Stop();
};

#endif
