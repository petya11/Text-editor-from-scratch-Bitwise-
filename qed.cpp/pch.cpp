#include "windows.h"
#include "d2d1.h"
#include "dwrite.h"
#include "stdint.h"
#include "assert.h"

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

#define Assert(x) \
	do { if (!(x)) {DebugBreak();} } while(0)

#define MAX(x, y) \
	((x) >= (y) ? (x) : (y))

#define INLINE \
	__forceinline

void* Allocate(uint32_t size) {
	return HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, size);
}

void* ReAllocate(void* pointer, uint32_t size) {
	return HeapReAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, pointer, size);
}

void Free(void* pointer) {
	HeapFree(GetProcessHeap(), 0, pointer);
}

void Debugf(char* format, ...) {
	char temp[1024];
	va_list arg_list;
	va_start(arg_list, format);
	wvsprintfA(temp, format, arg_list);
	va_end(arg_list);
	OutputDebugStringA(temp);
}

INLINE bool IsPrintableCharacter(char character){
	return ' ' <= character && character <= '~';
	}

//Buffers
struct Mode;

typedef uint32_t Cursor;

struct Buffer {
	uint32_t reference_count;

	char* data;
	uint32_t gap_start;
	uint32_t gap_end;
	uint32_t end;

	const char* filename;
	Mode* mode;
	Cursor point;
};


Buffer *CreateBuffer(uint32_t initial_gap_size) {
	Buffer *buffer = (Buffer*)Allocate(sizeof(Buffer));
	buffer->reference_count = 1;
	buffer->data = (char*)Allocate(initial_gap_size);
	buffer->gap_start = 0;
	buffer->gap_end = initial_gap_size;
	buffer->end = initial_gap_size;
	buffer->filename = 0;
	buffer->point = 0;
	return buffer;
}

void RealiseBuffer(Buffer* buffer) {
	Assert(buffer->reference_count > 0);
	buffer->reference_count--;
	if (buffer->reference_count == 0) {
		Free(buffer->data);
		Free(buffer);
	}
}

INLINE uint32_t GetBufferGapSize(Buffer* buffer) {
	return buffer->gap_end - buffer->gap_start;
}

INLINE uint32_t GetBufferLength(Buffer* buffer) {
	return buffer->end - GetBufferGapSize(buffer);
}

INLINE static void AssertBufferInvariants(Buffer* buffer) {
	Assert(buffer->data);
	Assert(buffer->gap_start <= buffer->gap_end);
	Assert(buffer->gap_end <= buffer->end);
}

INLINE static void AssertCursorInvariants(Buffer* buffer, Cursor cursor) {
	Assert(cursor <= GetBufferLength(buffer));
}

INLINE uint32_t GetBufferDataIndexFromCursor(Buffer* buffer, Cursor cursor) {
	return (cursor < buffer->gap_start) ? cursor : cursor + GetBufferGapSize(buffer);
}

//#define SetBufferCharacter(buffer, cursor) \	(buffer->data[GetBufferDataIndexFromCursor(buffer, cursor)])

INLINE char GetBufferCharacter(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	return buffer->data[GetBufferDataIndexFromCursor(buffer, cursor)];
}

INLINE void SetBufferCharacter(Buffer* buffer, Cursor cursor, char character) {
	AssertCursorInvariants(buffer, cursor);
	buffer->data[GetBufferDataIndexFromCursor(buffer, cursor)] = character;
}

static void ShiftGapToCursor(Buffer* buffer, Cursor cursor) {
	uint32_t gap_size = GetBufferGapSize(buffer);
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
	Assert(GetBufferGapSize(buffer) == gap_size);
	AssertBufferInvariants(buffer);
}

static void EnsureGapSize(Buffer *buffer, uint32_t minimum_gap_size) {
	if (GetBufferGapSize(buffer) < minimum_gap_size) {
		ShiftGapToCursor(buffer, GetBufferLength(buffer));
		uint32_t new_end = MAX(2 * buffer->end, buffer->end + minimum_gap_size - GetBufferGapSize(buffer));
		buffer->data = (char*)ReAllocate(buffer->data, new_end);
		buffer->gap_end = new_end;
		buffer->end = new_end;
	}
	Assert(GetBufferGapSize(buffer) >= minimum_gap_size);
}

bool ReplaceCharacter(Buffer* buffer, Cursor cursor, char character) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor < GetBufferLength(buffer)) {
		SetBufferCharacter(buffer, cursor, character);
		return true;
	}else {
		return false;
	}
}

void InsertCharacter(Buffer* buffer, Cursor cursor, char character) {
	AssertCursorInvariants(buffer, cursor);
	EnsureGapSize(buffer, 1);
	ShiftGapToCursor(buffer, cursor);
	buffer->data[buffer->gap_start] = character;
	buffer->gap_start++;

	if (buffer->point >= cursor) {
		buffer->point++;
	}
}

void InsertString(Buffer* buffer, Cursor cursor, char* string) {
	while (*string) {
		InsertCharacter(buffer, cursor, *string);
		string++;
	}
}

bool DeleteBackwardCharacter(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor > 0) {
		ShiftGapToCursor(buffer, cursor);
		buffer->gap_start--;
		if (buffer->point >= cursor){
			buffer->point--;
		}
		return true;
	}else {
		return false;
	}
}

bool DeleteForwardCharacter(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor < GetBufferLength(buffer)) {
		ShiftGapToCursor(buffer, cursor);
		buffer->gap_end++;
		if (buffer->point > cursor) {
			buffer->point--;
		}
		return true;
	}else {
		return false;
	}
}

void DeleteBuffer(Buffer* buffer) {
	buffer->gap_start = 0;
	buffer->gap_end - buffer->end;
	buffer->point = 0;


}

Cursor GetNextCharacterCursor(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor < GetBufferLength(buffer)) {
		return cursor + 1;
	}else {
		return cursor;
	}
}

Cursor GetPreviousCharacterCursor(Buffer * buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	if (cursor > 0) {
		return cursor - 1;
	}else {
		return cursor;
	}
}

Cursor GetBeginningOfLineCursor(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	cursor = GetPreviousCharacterCursor(buffer, cursor);
	while (cursor > 0) {
		char character = GetBufferCharacter(buffer, cursor);
		if (character == '\n') { 
			return GetNextCharacterCursor(buffer, cursor);
		}
		cursor = GetPreviousCharacterCursor(buffer, cursor);
	}
	return 0;
}

Cursor GetEndOfLineCursor(Buffer* buffer, Cursor cursor) {
	AssertCursorInvariants(buffer, cursor);
	while (cursor < GetBufferLength(buffer)) {
		char character = GetBufferCharacter(buffer, cursor);
		if (character == '\n') {
			return cursor;
		}
		cursor = GetNextCharacterCursor(buffer, cursor);
	}
	return GetBufferLength(buffer);
}

Cursor GetBeginningOfBufferCursor(Buffer* buffer, Cursor cursor) {
	return 0;
}

Cursor GetEndOfBufferCursor(Buffer* buffer, Cursor cursor) {
	return GetBufferLength(buffer);
}

Buffer* current_buffer;
//

void SaveBuffer(Buffer* buffer) {
	Assert(buffer->filename);
	HANDLE file_handle = CreateFileA(buffer->filename, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
		if (file_handle == INVALID_HANDLE_VALUE) { return; }
		DWORD size_written;
		WriteFile(file_handle, buffer->data, buffer->gap_start, &size_written, 0);   
		WriteFile(file_handle, buffer->data + buffer->gap_end, buffer->end - buffer->gap_end, &size_written, 0);  
		CloseHandle(file_handle);
}   

void LoadBuffer(Buffer* buffer) {
		Assert(buffer->filename);
		HANDLE file_handle = CreateFileA(buffer->filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
		if (file_handle == INVALID_HANDLE_VALUE) { return; }
		uint32_t file_size = GetFileSize(file_handle, 0);
		DeleteBuffer(buffer);
		EnsureGapSize(buffer, file_size);
		DWORD size_read;
		ReadFile(file_handle, buffer->data, file_size, &size_read, 0);
		CloseHandle(file_handle);
		buffer->gap_start - size_read;
}

struct InputEvent {
	uint16_t key_combination;
	uint8_t character;
	//InputEventType type;
};

InputEvent last_input_event;
//
//

typedef void (*CommandFunction)();

struct Command {
	const char* name;
	CommandFunction function;
};

#define COMMAND(name)\
	void Command_##name();\
	Command command_##name = {#name, Command_##name};\
	void Command_##name()

COMMAND(Nothing) {
	//
}

COMMAND(SelfInsertCharacter) {
	InsertCharacter(current_buffer, current_buffer->point, last_input_event.character);
}

COMMAND(InsertNewLine) {
	InsertCharacter(current_buffer, current_buffer->point, '\n');
}

COMMAND(ForwardCharacter) {
	current_buffer->point = GetNextCharacterCursor(current_buffer, current_buffer->point);
}

COMMAND(BackwardCharacter) {
	current_buffer->point = GetPreviousCharacterCursor(current_buffer, current_buffer->point);
}

COMMAND(BegginingOfLine) {
	current_buffer->point = GetBeginningOfLineCursor(current_buffer, current_buffer->point);
}

COMMAND(EndOfLine) {
	current_buffer->point = GetEndOfLineCursor(current_buffer, current_buffer->point);
}

COMMAND(BeginningOfBuffer) {
	current_buffer->point = GetBeginningOfBufferCursor(current_buffer, current_buffer->point);
}

COMMAND(EndOfBuffer) {
	current_buffer->point = GetEndOfBufferCursor(current_buffer, current_buffer->point);
}

COMMAND(DeleteForwardCharacter) {
	DeleteForwardCharacter(current_buffer, current_buffer->point);
}

COMMAND(DeleteBackwardCharacter) {
	DeleteBackwardCharacter(current_buffer, current_buffer->point);
}

COMMAND(Quit) {
	PostQuitMessage(0);
}

COMMAND(LoadBuffer) {
	if (current_buffer->filename)
	LoadBuffer(current_buffer);
}

COMMAND(SaveBuffer) {
	if(current_buffer->filename)
	SaveBuffer(current_buffer);
}
//
//

enum { MAX_KEY_COMBINATIONS = 1 << (8 + 3) };

struct Keymap {
	Keymap* parent;
	Command commands[MAX_KEY_COMBINATIONS];
};

Keymap* current_keymap;

INLINE uint16_t GetKeyCombination(uint8_t key, uint8_t ctrl, uint8_t alt, uint8_t shift) {
	return (uint16_t)key | ((uint16_t)ctrl << 8) | ((uint16_t)alt << 9) | ((uint16_t)shift << 10);
}

INLINE uint16_t Ctrl(uint8_t key) {
	return GetKeyCombination(key, 1, 0, 0);
}
INLINE uint16_t Alt(uint8_t key) {
	return GetKeyCombination(key, 0, 1, 0);
}
INLINE uint16_t Shift(uint8_t key) {
	return GetKeyCombination(key, 0, 0, 1);
}
INLINE uint16_t CtrlAlt(uint8_t key) {
	return GetKeyCombination(key, 1, 1, 0);
}

Command GetKeymapCommand(Keymap* keymap, uint16_t key_combination) {
	Assert(key_combination < MAX_KEY_COMBINATIONS);
	while (keymap) {
		Command command = keymap->commands[key_combination];
		if (command.function) {
			return command;
		}
		keymap = keymap->parent;
	}
	return command_Nothing;
}     

Keymap* CreateCopyKeymap(Keymap* original) {
	Keymap* keymap = (Keymap*)Allocate(sizeof(Keymap));
	CopyMemory(keymap, original, sizeof(Keymap));
	return keymap;
}

Keymap* CreateEmptyKeymap() {
		Keymap *keymap = (Keymap*)Allocate(sizeof(Keymap));
		keymap->parent = 0;
		for (int i = 0; i < MAX_KEY_COMBINATIONS; i++) {
			keymap->commands[i] = { 0 };
		}
		keymap->commands[Alt(VK_F4)] = command_Quit;
	return keymap;    
}

Keymap* CreateDefaultKeymap() {
	Keymap* keymap = CreateEmptyKeymap();
	for (uint32_t key = 0; key < 256; key++) {
		char character = MapVirtualKey(key, MAPVK_VK_TO_CHAR);
		if (IsPrintableCharacter(character)) {
			keymap->commands[key] = command_SelfInsertCharacter;
			keymap->commands[Shift(key)] = command_SelfInsertCharacter;
		}
	}
	keymap->commands[VK_RETURN] = command_InsertNewLine;
	keymap->commands[VK_BACK] = command_DeleteBackwardCharacter;
	keymap->commands[VK_DELETE] = command_DeleteForwardCharacter;
	keymap->commands[VK_LEFT] = command_ForwardCharacter;
	keymap->commands[VK_RIGHT] = command_BackwardCharacter;
	keymap->commands[VK_HOME] = keymap->commands[Ctrl('A')] = command_BegginingOfLine;
	keymap->commands[VK_END] = keymap->commands[Ctrl('E')] = command_EndOfLine;
	keymap->commands[Ctrl(VK_HOME)] = command_BegginingOfLine;
	keymap->commands[Ctrl(VK_END)] = command_EndOfLine;
	keymap->commands[Ctrl('O')] = command_LoadBuffer;
	keymap->commands[Ctrl('S')] = command_SaveBuffer;
	return keymap;
}

Keymap* CreateDeriveKeymap(Keymap* parent) {
	Keymap* keymap = CreateEmptyKeymap();
	keymap->parent = parent;
	return keymap;
}

void DispatchInputEvent(Keymap* keymap, InputEvent input_event) {
		Command command = GetKeymapCommand(keymap, input_event.key_combination);
		//Debugf("Input: key=%x, command=%s\n", input_event.key_combination, input_event.character ? input_event.character : ' ', command.name);
		command.function();   
}

//

struct Mode {
	const char* name;
	Keymap* keymap;
};
  
Mode* CreateMode(char *name, Keymap* keymap) {
	Mode* mode = (Mode*)Allocate(sizeof(Mode));
	mode->name = name;
	mode->keymap = keymap;
	return mode;
}

Mode* text_mode;

// drawing

ID2D1Factory* d2d_factory;
IDWriteFactory* dwrite_factory;
ID2D1HwndRenderTarget* render_target;
IDWriteTextFormat* text_format;
ID2D1SolidColorBrush* text_brush;

	uint32_t CopyLineFromBuffer(char* line, int max_line_size, Buffer *buffer, Cursor *out_cursor) {
		Cursor cursor = *out_cursor;
		int i;
		for (i = 0; i < max_line_size && cursor < GetBufferLength(buffer); i++) {
			char character = GetBufferCharacter(buffer, cursor);
			if (character == '\n') { break; }
			line[i] = character;
			cursor++;
		}
		while (cursor < GetBufferLength(buffer) && GetBufferCharacter(buffer, cursor) != '\n') {
			cursor++;
		}
		*out_cursor = cursor;
		return i;
	}


	void DrawBuffer(Buffer * buffer, float line_height, float x, float y, float width, float height) {
		D2D1_RECT_F layout_rectangle;
		layout_rectangle.left = x;
		layout_rectangle.right = x + width;
		layout_rectangle.top = y;
		layout_rectangle.bottom = y + height;
		char utf8_line[64];
		WCHAR utf16_line[64];
		for (Cursor cursor = 0; cursor < GetBufferLength(buffer); cursor++) {
			uint32_t line_length = CopyLineFromBuffer(utf8_line, sizeof(utf8_line) - 1, buffer, &cursor);
			utf8_line[line_length] = 0;
			MultiByteToWideChar(CP_UTF8, 0, utf8_line, sizeof(utf8_line), utf16_line, sizeof(utf16_line) / sizeof(*utf16_line));
			render_target->DrawText(utf16_line, wcslen(utf16_line), text_format, layout_rectangle, text_brush);
			layout_rectangle.top += line_height;
		}
	}

	void OutputDebugBuffer(Buffer * buffer) {
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
		static uint8_t ctrl;
		static uint8_t alt;
		static uint8_t shift;

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
		case WM_SYSKEYDOWN:
			switch (wparam) {
			case VK_CONTROL:
				ctrl = 1;
				break;
			case VK_MENU:
				alt = 1;
				break;
			case VK_SHIFT:
				shift = 1;
				break;
			default:
				last_input_event.key_combination = GetKeyCombination(wparam, ctrl, alt, shift);
				BYTE keyboard_state[256];
				GetKeyboardState(keyboard_state);
				WORD characters;
				if (ToAscii(wparam, (lparam >> 16) & 0xFF, keyboard_state, &characters, 0) == 1) {     
					last_input_event.character = (uint8_t)characters;
				}else {
					last_input_event.character = 0;
				}
				DispatchInputEvent(current_buffer->mode->keymap, last_input_event);
				RenderWindow(window);
				break;
			}
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			switch (wparam) {
				ctrl = 0;
				break;
			case VK_MENU:
				alt = 0;
				break;
			case VK_SHIFT:
				shift = 0;   
				break;
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

enum {MAX_RPN_STACK = 32};
static uint32_t rpn_stack_data[MAX_RPN_STACK];
static uint32_t* rpn_stack_top = rpn_stack_data;

COMMAND(RPN_Print) {
	for (uint32_t* stack = rpn_stack_data; stack != rpn_stack_top; stack++) {
		uint32_t value = *stack;
		int digit_count = 0;
		do {
			char character = '0' + value % 10;
			InsertCharacter(current_buffer, current_buffer->point, character);
			Command_BackwardCharacter();
			digit_count++;
			value /= 10;
		} while (value != 0);
		for (int i = 0; i < digit_count; i++) {
			Command_ForwardCharacter();
		}
		InsertCharacter(current_buffer, current_buffer->point, ' ');
	}
	Command_InsertNewLine();
}

COMMAND(RPN_Push) {
	char character = last_input_event.character;
	if ('0' <= character && character <= '9') {
		*rpn_stack_top++ = character - '0';
	}
	Command_RPN_Print();
}

COMMAND(RPN_Pop) {
	if (rpn_stack_top - rpn_stack_data >= 1) {
		rpn_stack_top--;
	}
	Command_RPN_Print();

}


COMMAND(RPN_Add) {
	if (rpn_stack_top - rpn_stack_data >= 2) {
		rpn_stack_top[-2] = rpn_stack_top[-1] + rpn_stack_top[-2];
		rpn_stack_top--;

	}
	Command_RPN_Print();
}

COMMAND(RPN_Multiply) {
	if (rpn_stack_top - rpn_stack_data >= 2) {
		rpn_stack_top[-2] = rpn_stack_top[-1] + rpn_stack_top[-2];
		rpn_stack_top--;
	}
	Command_RPN_Print();
}

COMMAND(RPN_Mode) {
	static Mode* rpn_mode;
	if (!rpn_mode) {
		rpn_mode = CreateMode("RPN Calculator", CreateEmptyKeymap());
		Keymap* rpn_keymap = rpn_mode->keymap;
		for (int digit = 0; digit <= 9; digit++) {
			rpn_keymap->commands['0' + digit] = command_RPN_Push;
		}
		rpn_keymap->commands[Shift(VK_OEM_PLUS)] = command_RPN_Add;
		rpn_keymap->commands[Shift('8')] = command_RPN_Multiply;
		rpn_keymap->commands[VK_BACK] = command_RPN_Pop;
	}
	current_buffer->mode = rpn_mode;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
	text_mode = CreateMode("text", CreateDefaultKeymap());

	current_buffer = CreateBuffer(1);
	Command_RPN_Mode();

	//current_buffer = CreateBuffer(2);
	//current_buffer->mode = text_mode;
	//current_buffer->filename = "d:\\qed\\test.txt";
	
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
