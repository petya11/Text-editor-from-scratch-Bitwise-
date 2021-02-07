#include <pch.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int){
	WNDCLASSA window_class = { 0 };
	window_class.hInstance = instance;
	window_class.lpfnWndProc = DefWindowProcA;
	window_class.lpszClassName = "qed";
	RegisterClassA(&window_class);

	HWND window = CreateWindowA("qed", "QED", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);


	return 0;   
}

