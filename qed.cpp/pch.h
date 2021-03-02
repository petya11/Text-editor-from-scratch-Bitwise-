#include "windows.h"
#include "d2d1.h"
#include "dwrite.h"
#include "stdint.h"
#include "assert.h"

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

#define Assert(x) \
	do { if (!(x)) {DebugBreak();} } while(0)

#define MIN(x, y) \
	((x) <= (y) ? (x) : (y))

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
	buffer->gap_end = buffer->end; 
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

Cursor GetBeginningOfNextLineCursor(Buffer* buffer, Cursor cursor) {
	return GetNextCharacterCursor(buffer, GetEndOfLineCursor(buffer, cursor));
}

Cursor GetEndOfPreviousLineCursor(Buffer* buffer, Cursor cursor) {
	return GetPreviousCharacterCursor(buffer, GetBeginningOfLineCursor(buffer, cursor));
}

Cursor GetBeginningOfPreviousLineCursor(Buffer* buffer, Cursor cursor) {
	return GetBeginningOfLineCursor(buffer, GetPreviousCharacterCursor(buffer, GetBeginningOfLineCursor(buffer, cursor)));
}

Cursor GetBeginningOfBufferCursor(Buffer* buffer, Cursor cursor) {
	return 0;
}

Cursor GetEndOfBufferCursor(Buffer* buffer, Cursor cursor) {
	return GetBufferLength(buffer);
}

uint32_t GetBufferLineLength(Buffer* buffer, Cursor cursor) {
	return GetEndOfLineCursor(buffer, cursor) - GetBeginningOfLineCursor(buffer, cursor);
}

uint32_t GetBufferColumn(Buffer* buffer, Cursor cursor) {
	return cursor - GetBeginningOfLineCursor(buffer, cursor);
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
		buffer->gap_start = size_read;// ====
}

struct InputEvent {
	uint16_t key_combination;
	uint8_t character;
	uint32_t time; //??
};

InputEvent last_input_event;
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

COMMAND(NextCharacter) {
	current_buffer->point = GetNextCharacterCursor(current_buffer, current_buffer->point);
}

COMMAND(PreviousCharacter) {
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

uint32_t goal_column = -1;

COMMAND(NextLine) {
	if (goal_column == -1) {
		goal_column = GetBufferColumn(current_buffer, current_buffer->point);
	}
	Cursor beginning_of_next_line = GetBeginningOfNextLineCursor(current_buffer, current_buffer->point);
	UINT32 next_line_length = GetBufferLineLength(current_buffer, beginning_of_next_line);
	current_buffer->point = beginning_of_next_line + MIN(next_line_length, goal_column);     
}

COMMAND(PreviousLine) {
	if (goal_column == -1) {
		goal_column = GetBufferColumn(current_buffer, current_buffer->point);
	}
	Cursor beginning_of_previous_line = GetBeginningOfPreviousLineCursor(current_buffer, current_buffer->point);
	UINT32 previous_line_length = GetBufferLineLength(current_buffer, beginning_of_previous_line);
	current_buffer->point = beginning_of_previous_line + MIN(previous_line_length, goal_column);
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

struct Box {
	int left;
	int right;
	int top;
	int bottom;
};

struct Pane {
	Box box;
	Buffer* buffer;
	Cursor start;
	Cursor end;
};

void SetPaneBuffer(Pane* pane, Buffer* buffer) {
	pane->buffer = buffer;
	pane->start = buffer->point;
	pane->end = UINT32_MAX;
}

enum { MAX_PANES = 16 };
Pane panes[MAX_PANES];
int pane_count;
int active_pane;

Pane* CreatePane(Buffer* buffer, Box box) {
	Assert(pane_count < MAX_PANES);
	Pane* pane = &panes[pane_count];
	pane->box = box;
	SetPaneBuffer(pane, buffer);
	pane_count++;
	return pane;
}

void ReleasePane(Pane* pane) {
	panes[pane - panes] = pane[pane_count - 1];
	pane_count--;
}

void SetActivePane(int pane) {
	current_buffer = panes[pane].buffer;
	active_pane = pane;
}

COMMAND(NextPane) {
	active_pane++;
	if (active_pane >= pane_count) {
		active_pane = 0;
	}
	SetActivePane(active_pane);
}

COMMAND(PreviousPane) {
	active_pane--;
	if (active_pane < 0) {
		active_pane = pane_count - 1;
	}
	SetActivePane(active_pane);
}

enum { MAX_KEY_COMBINATIONS = 1 << (8 + 3) };

struct Keymap {
	Keymap* parent;
	Command commands[MAX_KEY_COMBINATIONS];
};


enum {
	CTRL = 1 << 8,
	ALT = 1 << 9,
	SHIFT = 1 << 10
};

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

Keymap* GetSystemKeymap() {
	static Keymap* keymap;
	if (!keymap) {
		Keymap * keymap = (Keymap*)Allocate(sizeof(Keymap));
		ZeroMemory(keymap, sizeof(Keymap));
		keymap->commands[VK_F4 | ALT] = command_Quit;
		keymap->commands[CTRL | VK_TAB] = command_NextPane;
		keymap->commands[CTRL | SHIFT | VK_TAB] = command_PreviousPane;
	}
	return keymap;
}

Keymap* CreateEmptyKeymap() {
		Keymap *keymap = (Keymap*)Allocate(sizeof(Keymap));
		keymap->parent = GetSystemKeymap();
		for (uint16_t key_combination = 0; key_combination < MAX_KEY_COMBINATIONS; key_combination++) {
			keymap->commands[key_combination] = { 0 };
		}
	return keymap;    
}

Keymap* CreateDefaultKeymap() {
	Keymap* keymap = CreateEmptyKeymap();
	for (uint32_t key = 0; key < 256; key++) {
		char character = MapVirtualKey(key, MAPVK_VK_TO_CHAR);
		if (IsPrintableCharacter(character)) {
			keymap->commands[key] = command_SelfInsertCharacter;
			keymap->commands[SHIFT | key] = command_SelfInsertCharacter;
		}
	}
	keymap->commands[VK_RETURN] = command_InsertNewLine;
	keymap->commands[VK_BACK] = command_DeleteBackwardCharacter;
	keymap->commands[VK_DELETE] = command_DeleteForwardCharacter;
	keymap->commands[VK_LEFT] = command_PreviousCharacter;
	keymap->commands[VK_RIGHT] = command_NextCharacter;
	keymap->commands[VK_DOWN] = command_PreviousLine;
	keymap->commands[VK_UP] = command_NextLine;
	keymap->commands[VK_HOME] = keymap->commands[CTRL | 'A'] = command_BegginingOfLine;
	keymap->commands[VK_END] = keymap->commands[CTRL | 'E'] = command_EndOfLine;
	keymap->commands[CTRL | VK_HOME] = command_BegginingOfLine;
	keymap->commands[CTRL | VK_END] = command_EndOfLine;
	keymap->commands[CTRL | 'O'] = command_LoadBuffer;
	keymap->commands[CTRL | 'S'] = command_SaveBuffer;
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
  
Mode* CreateMode(const char *name, Keymap* keymap) {
	Mode* mode = (Mode*)Allocate(sizeof(Mode));
	mode->name = name;
	mode->keymap = keymap;
	return mode;
}
COMMAND(TextMode) {
	static Mode* text_mode;
	if (!text_mode) {
		text_mode = CreateMode("text", CreateDefaultKeymap());
	}
	current_buffer->mode = text_mode;
}

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

	void UpdatePaneScrolling(Pane* pane) {
		Buffer* buffer = pane->buffer;
		Cursor start = MIN(GetBufferLength(buffer), pane->start);
		Cursor end = MIN(GetBufferLength(buffer), pane->end);

		start = GetBeginningOfLineCursor(buffer, buffer->point);

		if (buffer->point < start) {
			start = GetBeginningOfLineCursor(buffer, start);
		}

			while (end < buffer->point) {
				end = GetBeginningOfNextLineCursor(buffer, end);
				if (end != GetBufferLength(buffer)) {
					start = GetBeginningOfNextLineCursor(buffer, start);
				}
			}
		pane->start = start;
		pane->end = end;
	}

	void DrawPane(Pane* pane) {
		UpdatePaneScrolling(pane);

		Buffer* buffer = pane->buffer;
		Cursor start = pane->start;

		float border_width = 3.f;
		D2D1_RECT_F layout_rectangle;
		layout_rectangle.left = (float)pane->box.left;
		layout_rectangle.right = (float)pane->box.right;
		layout_rectangle.top = (float)pane->box.top;
		layout_rectangle.bottom = (float)pane->box.bottom;
		if (pane == &panes[active_pane]) {
			render_target->DrawRectangle(layout_rectangle, text_brush, border_width);
		}
		layout_rectangle.left += border_width;
		layout_rectangle.right -= border_width;
		layout_rectangle.top += border_width;
		layout_rectangle.bottom -= border_width;
		render_target->PushAxisAlignedClip(layout_rectangle, D2D1_ANTIALIAS_MODE_ALIASED);
		char utf8_line[64];
		WCHAR utf16_line[64];
		Cursor cursor;
		for (cursor = pane->start; cursor < GetBufferLength(buffer) && layout_rectangle.top < layout_rectangle.bottom; cursor = GetNextCharacterCursor(buffer, cursor)) {
			Cursor previous_cursor = cursor;
			uint32_t utf8_line_length = CopyLineFromBuffer(utf8_line, sizeof(utf8_line) - 1, buffer, &cursor);
			utf8_line[utf8_line_length] = 0;
			int utf16_line_length = MultiByteToWideChar(CP_UTF8, 0, utf8_line, utf8_line_length, utf16_line, sizeof(utf16_line) / sizeof(*utf16_line));
			
			IDWriteTextLayout* text_layout;
			dwrite_factory->CreateTextLayout(utf16_line, utf16_line_length, text_format, 
				layout_rectangle.right - layout_rectangle.left, layout_rectangle.bottom - layout_rectangle.top, &text_layout);
			render_target->DrawTextLayout(D2D1::Point2F(layout_rectangle.left, layout_rectangle.top), text_layout, text_brush);
			
			DWRITE_LINE_METRICS line_metric;
			UINT32 line_count;
			text_layout->GetLineMetrics(&line_metric, 1, &line_count);
		

			if (pane == &panes[active_pane] && previous_cursor <= buffer->point && buffer->point <= cursor) {
				float caret_x;
				float caret_y;
				DWRITE_HIT_TEST_METRICS caret_metrics;
				text_layout->HitTestTextPosition(buffer->point - previous_cursor, 0, &caret_x, &caret_y, &caret_metrics);

				D2D1_RECT_F caret_rectangle;
				caret_rectangle.left = layout_rectangle.left + caret_metrics.left;
				caret_rectangle.right = caret_rectangle.left;
				caret_rectangle.top = layout_rectangle.top + caret_metrics.top;
				caret_rectangle.bottom = caret_rectangle.top + caret_metrics.height;
				render_target->DrawRectangle(caret_rectangle, text_brush);     
			}
			if (line_count == 1) {
				layout_rectangle.top += line_metric.height;

			}
			text_layout->Release();
		}
		pane->end = GetEndOfPreviousLineCursor(buffer, cursor);
		render_target->PopAxisAlignedClip();
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

	void DrawWindow(HWND window) {
		RECT client_rectangle;
		GetClientRect(window, &client_rectangle);
		render_target->BeginDraw();
		render_target->Clear(D2D1::ColorF(D2D1::ColorF::White));
		for (int i = 0; i < pane_count; i++) {
			DrawPane(&panes[i]);
		}
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
		case WM_SYSKEYDOWN:
				if (GetKeyState(VK_CONTROL) >> 15) {
					wparam |= CTRL;
				}
				if (GetKeyState(VK_MENU) >> 15) {
					wparam |= ALT;
				}
				if (GetKeyState(VK_SHIFT) >> 15) {
					wparam |= SHIFT;
				}
				last_input_event.time = GetTickCount();
				last_input_event.key_combination = wparam;
				BYTE keyboard_state[256];
				GetKeyboardState(keyboard_state);
				WORD characters;
				if (ToAscii(wparam, (lparam >> 16) & 0xFF, keyboard_state, &characters, 0) == 1) {     
					last_input_event.character = (uint8_t)characters;
				}else {
					last_input_event.character = 0;
				}
				DispatchInputEvent(current_buffer->mode->keymap, last_input_event);
				DrawWindow(window);
				break;
			case WM_PAINT:
				DrawWindow(window);
				break;
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
			Command_PreviousCharacter();
			digit_count++;
			value /= 10;
		} while (value != 0);
		for (int i = 0; i < digit_count; i++) {
			Command_NextCharacter();
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
		rpn_keymap->commands[SHIFT | VK_OEM_PLUS] = command_RPN_Add;
		rpn_keymap->commands[SHIFT | '8'] = command_RPN_Multiply;
		rpn_keymap->commands[VK_BACK] = command_RPN_Pop;
	}
	current_buffer->mode = rpn_mode;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {

	Buffer *rpn_buffer = CreateBuffer(1);
	current_buffer = rpn_buffer;
	Command_RPN_Mode();

	Buffer* test_buffer = CreateBuffer(1);
	test_buffer->filename = "d:\\qed\\test.txt";
	current_buffer = test_buffer;
	Command_TextMode();

	Box pane1_box;
	pane1_box.left = 10;
	pane1_box.right = 500;
	pane1_box.top = 10;
	pane1_box.bottom = 500;
	Pane* pane1 = CreatePane(current_buffer, pane1_box);

	Box pane2_box;
	pane2_box.left = 600;
	pane2_box.right = 1100;
	pane2_box.top = 10;
	pane2_box.bottom = 500;
	Pane* pane2 = CreatePane(rpn_buffer, pane2_box);

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
