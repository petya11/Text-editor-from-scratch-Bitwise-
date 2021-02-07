#include "pch.h"


#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

ID2D1Factory* d2d_factory;
IDWriteFactory* dwrite_factory;
ID2D1HwndRenderTarget* render_target;
IDWriteTextFormat* text_format;
ID2D1SolidColorBrush* text_brush;

WCHAR text[] = L"Foo bar baz fdsfsdfsdfsddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

typedef uint32_t BufferPosition;

struct Buffer {
	char* name;
	char* data;
	BufferPosition gap_start_position;
	BufferPosition gap_end_position;
	BufferPosition end_position;
};

void DebugWriteBuffer(Buffer* buffer) {
	char temp[1024];
	CopyMemory(temp, buffer->data, buffer->gap_start_position)
		temp[buffer->gap_start_position] = 0;
	OutputDebugStringA(temp);
}

LRESULT CALLBACK QedWindowProc(HWND window, UINT message_type, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;
	HRESULT hresult = 0;
	switch (message_type) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE: {
		if (render_target) {
			render_target->Release();
		}
		RECT client_rectangle;
		GetClientRect(window, &client_rectangle);
		D2D1_SIZE_U window_size;
		window_size.width = client_rectangle.right - client_rectangle.left;
		window_size.height = client_rectangle.bottom - client_rectangle.top;
		hresult = d2d_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(window, window_size), &render_target);
		if (text_brush) { text_brush->Release(); }

		hresult = render_target->CreateSolidColorBrush(D2D1::color(D2D1::ColorF::Black), brush_properties, &text_brush);
		break;
	}
	case WM_PAINT: {
		RECT client_rectangle;
		GetClientRect(window, &client_rectangle);
		render_target->BeginDraw();
		render_target->Clear(D2D1::ColorF(D2D1::ColorF::Red));
		D2D1_RECT_F layout_rectangle;
		layout_rectangle.left = (FLOAT)client_rectangle.left;
		layout_rectangle.right = (FLOAT)client_rectangle.right;
		layout_rectangle.top = (FLOAT)client_rectangle.top;
		layout_rectangle.botton = (FLOAT)client_rectangle.botton;
		layout_rectangle, text_brush
		render_target->DrawText(text, wcslen(text), client_rectangle, text_format);
		hresult = render_target->EndDraw();
		ValidateRect(window, 0);
		break;
	}
	default:
		result = DefWindowProcA(window, message_type, wparam, lparam);
		break;
	}
	return result;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	HRESULT hresult;
	hresult = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
	hresult = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory);
	hresult = dwrite_factory->CreateTextFormat(L"Consolas", 0, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 36.0f, L"en-us", &text_format);


	WNDCLASSA window_class = { 0 };
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.hInstance = instance;
	window_class.lpfnWndProc = QedWindowProc;
	window_class.lpszClassName = "qed";
	RegisterClassA(&window_class);

	HWND window = CreateWindowA("qed", "QED", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);



	MSG message;
	while (GetMessage(&message, 0, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	return 0;
}

