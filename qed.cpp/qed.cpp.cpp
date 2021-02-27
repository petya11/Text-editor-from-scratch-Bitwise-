#include "pch.h"


#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

#define Assert(x) \
do {if (!(x)) {DebugBreak();} } while(0)

#define MAX(x, y) \
	((x)) >= (y) ? (x) : (y))



void *Allocate(uint32_t size) {
	return HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, size);
}

void* ReAllocate(void* pointer, uint32_t size) {
	return HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, pointer, size);
}

void Free(void* pointer) {
	HeapFree(GetProcessHeap(), 0, pointer);
}

//Buffers

typedef uint32_t Cursor;

struct Buffer {
	char *data;
	Cursor point;
	uint32_t gap_start;
	uint32_t gap_end;
	uint32_t end;
};

#define BUFFER_GAP_SIZE(buffer) \
	(buffer->gap_end - buffer->gap_start)

#define BUFFER_LENGTH(buffer) \
	(buffer->end - BUFFER_GAP_SIZE(buffer))

#define BUFFER_INDEX(buffer, cursor) \
	((cursor < buffer->gap_start) ? cursor : cursor + BUFFER_GAP_SIZE(buffer)

#define BUFFER_CHARACTER(buffer, cursor) \
	(buffer->data[BUFFER_INDEX(buffer, cursor)])



void InitializeBuffer(Buffer* buffer, uint32_t initial_gap_size) {
	buffer->data = (char*)Allocate(initial_gap_size);
	buffer->point = 0;
	buffer->gap_start = 0;
	buffer->gap_end = initial_gap_size;
	buffer->end = initial_gap_size;
}

static void AssertBufferInvariants(Buffer* buffer) {
	Assert(buffer->data);
	Assert(buffer->gap_start <= buffer->gap_end);
	Assert(buffer->gap_end <= buffer->end);
}

static void AssertCursorInvariants(Buffer* buffer, Cursor cursor) {
	Assert(cursor <= BUFFER_LENGTH(buffer));
}

static void ShiftGapToPosition(Buffer* buffer, Cursor cursor) {
	uint32_t gap_size = BUFFER_GAP_SIZE(buffer);
	if (cursor < buffer->gap_start) {
		uint32_t gap_delta = buffer->gap_start - cursor;
		buffer->gap_start -= gap_delta;
		buffer->gap_end -= gap_delta;
		MoveMemory(buffer->data + buffer->gap_end, buffer->data + buffer->gap_start, gap_delta);
	}else if (cursor > buffer->gap_start) {
		uint32_t gap_delta = cursor - buffer->gap_start;
		MoveMemory(buffer->data + buffer->gap_start, buffer->data + buffer->gap_end, gap_delta);
		buffer->gap_start += gap_delta;
		buffer->gap_end += gap_delta;
	}
	Assert(BUFFER_GAP_SIZE(buffer) == gap_size);
	AssertBufferInvariants(buffer);
}

static void EnsureGapSize(Buffer *buffer, uint32_t minimum_gap_size){
	if (BUFFER_GAP_SIZE(buffer) < minimum_gap_size) {
		ShiftGapToPosition(buffer, BUFFER_LENGTH(buffer));
		uint32_t new_end = MAX(2 * buffer->end, buffer->end + minimum_gap_size);
		buffer->data = (char *)ReAllocate(buffer->data, new_end);
		buffer->gap_end = new_end;
		buffer->end = new_end;
	}
	Assert(BUFFER_GAP_SIZE(buffer) >= minimum_gap_size);
}

bool ReplaceCharacter(Buffer* buffer, Cursor cursor, char character) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor < BUFFER_LENGTH(buffer)) {
		BUFFER_CHARACTER(buffer, cursor) = character;
		return true;
	}	else {
		return false;
	}
}

void InsertCharacter(Buffer * buffer, Cursor cursor, char character) {
	AssertCursorInvariants(buffer, cursor);
	EnsureGapSize(buffer, 100);
	ShiftGapToCursor(buffer, cursor);
	buffer->data[buffer->gap_start] = character;
	buffer->gap_start++;
}

bool DeleteBackwardCharacter(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor > 0) {
		ShiftGapToCursor(buffer, cursor);
		buffer->gap_start--;
		return true;
	}	else {
		return false;
	}
}

bool DeleteFprwardCharacter(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor < BUFFER_LENGTH(buffer)) {
		ShiftGapToCursor(buffer, cursor);
		buffer->gap_end++;
		return true;
	}	else {
		return false;
	}
}

Cursor GetNextCharacterCursor(Buffer* buffer, Cursor cursor) {
	if (cursor < BUFFER_LENGTH(buffer));;; {hi
}


#define BUFFER_NEXT(buffer, cursor) \
	((cursor < BUFFER_LENGTH(buffer) ? ? cursor + 1 : cursor)

#define BUFFER_PREVIOUS(buffer, cursor) \
	((cursor > 0) ? cursor - 1 : cursor)

Buffer* current_buffer;
Cursor current_cursor;

ID2D1Factory* d2d_factory;
IDWriteFactory* dwrite_factory;
ID2D1HwndRenderTarget* render_target;
IDWriteTextFormat* text_format;
ID2D1SolidColorBrush* text_brush;

// dr

uint32_t CopyLineFromBuffer(char* line, int max_line_size, Buffer* buffer, Cursor* out_cursor) {
	Cursor cursor = *out_cursor;
	int i;
	for (i = 0; i < max_line_size && cursor < BUFFER_LENGTH(buffer); i++) {
		char character = BUFFER_CHARACTER(buffer, cursor);
		if (character == '\n') { break; }
		line[i] = character;
		cursor++;
	}
	while (cursor < BUFFER_LENGTH(buffer) && BUFFER_CHARACTER(buffer, cursor) != '\n') {
		cursor++;
	}
	*out_cursor = cursor;
	return i;
}


void DrawBuffer(Buffer* buffer, float line_height, float x, float y, float width, float height) {
	D2D1_RECT_F layout_rectangle;
	layout_rectangle.left = x;
	layout_rectangle.right = x + width;
	layout_rectangle.top = y;
	layout_rectangle.bottom = y + height;
	char utf8_line[64];
	WCHAR utf16_line[64];
	for (Cursor cursor = 0; cursor < BUFFER_LENGTH(buffer); cursor++) {
		uint32_t line_length = CopyLineFromBuffer(utf8_line, sizeof(utf8_line) - 1, buffer, &cursor);
		utf8_line[line_length] = 0;
		MultiByteToWideChar(CP_UTF8, 0, utf8_line, sizeof(utf8_line), utf16_line, sizeof(utf16_line) / sizeof(*utf16_line));
		render_target->DrawText(utf16_line, wcslen(utf16_line), text_format, layout_rectangle, text_brush);
		layout_rectangle.top += line_height;
	}
}

void OutputDebugBuffer(Buffer *buffer) {
	char temp[1024];
	CopyMemory(temp, buffer->data, buffer->gap_start);
	temp[buffer->gap_start] = 0;
	OutputDebugStringA(temp);
	CopyMemory(temp, buffer->data + buffer->gap_end, buffer->end - buffer->gap_end);
	temp[buffer->end - buffer->gap_end] = 0;
	OutputDebugStringA(temp);
}

void RenderWindow(HWND window) {
	RECT client_rectangle;
	GetClientRect(window, &client_rectangle);
	render_target->BeginDraw();
	render_target->Clear(D2D1::ColorF(D2D1::ColorF::White));
	DrawBuffer(current_buffer, 56.0f, (FLOAT)client_rectangle.left, (FLOAT)client_rectangle.top,
	(FLOAT)(client_rectangle.right - client_rectangle.left), (FLOAT)(client_rectangle.bottom - client_rectangle.top));
	render_target->EndDraw();
	ValidateRect(window, 0);
}

LRESULT CALLBACK QedWindowProc(HWND window, UINT message_type, WPARAM wparam, LPARAM lparam) {
	LRESULT result = 0;
	HRESULT hresult = 0;
	switch (message_type) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE: {
		RECT client_rectangle;
		GetClientRect(window, &client_rectangle);
		D2D1_SIZE_U window_size;
		window_size.width = client_rectangle.right - client_rectangle.left;
		window_size.height = client_rectangle.bottom - client_rectangle.top;
		if (render_target) { render_target->Release(); }
		hresult = d2d_factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(window, window_size), &render_target);
		if (text_brush) { text_brush->Release(); }
		hresult = render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &text_brush);
		break;
	}
	case WM_KEYDOWN:
		switch (wparam) {
		case VK_DELETE:
			DeleteFprwardCharacter(current_buffer, current_cursor);
				break;
		case VK_BACK:
			DeleteFprwardCharacter(current_buffer, current_cursor);
			current_cursor = CURSOR_PREVIOUS(current_buffer, current_cursor);
			break;
		case VK_LEFT:
			current_cursor = CURSOR_PREVIOUS(current_buffer, current_cursor);
			break;
		case VK_RIGHT:
			current_cursor = CURSOR_NEXT(current_buffer, current_cursor);
			break;
		case VK_RETURN:
			InsertCharacter(current_buffer, current_cursor, '\n');
			current_cursor = CURSOR_NEXT(current_buffer, current_cursor);
			break;
		}
		RenderWindow(window);
		break;
	case WM_CHAR:
		char character;
		WideCharToMultiByte(CP_UTF8, 0, (WCHAR*)&wparam, 1, &character, 1, 0, 0);
		if (' ' <= character && character <= '~')
			InsertCharacter(current_buffer, current_cursor, character);
		current_cursor = CURSOR_NEXT(current_buffer, current_cursor);
		RenderWindow(window);
	}
		break;
	case WM_PAINT: {
		RenderWindow(window);
		break;
	}
	default:
		result = DefWindowProcA(window, message_type, wparam, lparam);
		break;
	}
	return result;
}


int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	Buffer buffer_data;
	current_buffer = &buffer_data;
	InitializeBuffer(current_buffer, 2);
	const char buffer_text[] = "Hello\nworld!\nFoo bar baz";
	for (int i = 0; i < sizeof(buffer_text); i++) {
		InsertCharacter(current_buffer, i, buffer_text[i]);
	}

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwrite_factory);
	dwrite_factory->CreateTextFormat(L"Consolas", 0, DWRITE_FONT_WEIGHT_REGULAR, 
		DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 36.0f, L"en-us", &text_format);
	text_format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

	WNDCLASSA window_class = { 0 };
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.hInstance = instance;
	window_class.lpfnWndProc = QedWindowProc;
	window_class.lpszClassName = "qed";
	RegisterClassA(&window_class);

	HWND window = CreateWindowA("qed", "QED", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

	MSG message;
	while (GetMessage(&message, 0, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	return 0;
}
