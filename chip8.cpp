/*
Chip-8 Interpreter/Emulator
Copyright (C) 2016  Alex Kowald

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <cassert>
#include <cstring>
#include <climits>
#include <sys/stat.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <SDL.h>

#include "chip8.h"

#define SANITY_CHECK(condition, error_message) \
	if(!(condition)) Halt(error_message);
#define PRINT_DEBUG_INSTRUCTION(addr, opCode, name) \
	if(debug) printf("0x%04X:0x%04X - %s\n", addr-0x02, opCode, name);

enum
{
	DebugState_StepInto=0,
	DebugState_Run,
};

Chip8::Chip8()
{
	texture = nullptr;
	renderer = nullptr;
	window = nullptr;
	audioDevice = 0;

	static const uint8_t fonts[16 * 5] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80, // F
	};
	std::memcpy(&font, &fonts, sizeof(fonts));

	rng.seed(std::random_device()());

	pixels = new uint32_t[W*H]();
	background = 0x000000; // Black.
	foreground = 0xFFFFFF; // White.

	debug = false;
	ips = 3000; // Instructions per second.
	pixelScale = 16;

	Reset();

	if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_AUDIO) != 0)
	{
		printf("SDL_Init error: %s\n", SDL_GetError());
	}

	// Ensure we stay out of program space. (0x200 onwards)
	SANITY_CHECK(PROGRAM_SPACE - ((uint8_t *)&keys + sizeof(keys) - (uint8_t *)&memory) >= 0, "No free program space");
}

Chip8::~Chip8()
{
	delete pixels;

	CleanupSDL();
	SDL_Quit();
}

void Chip8::Reset()
{
	SP = 0;
	PC = PROGRAM_SPACE;

	for(auto &reg: V) reg = 0x00;
	I = 0x0000;
	delayTimer = 0x00;
	soundTimer = 0x00;
	keys = 0x00;
	waitingKey = 0x00;

	halt = false;
	debugState = DebugState_StepInto;

	ClearScreen();
}

void Chip8::SetBackgroundColor(uint32_t color)
{
	color = std::min(color, 0xFFFFFFu);
	background = color;
}

void Chip8::SetForegroundColor(uint32_t color)
{
	color = std::min(color, 0xFFFFFFu);
	foreground = color;
};

void Chip8::AudioCallback(void *userdata, uint8_t *stream, int len)
{
	static_cast<Chip8 *>(userdata)->SawtoothWave(stream, len);
}

void Chip8::SawtoothWave(uint8_t *stream, int len)
{
	//printf("SawtoothWave callback: len = %d, tick = %d\n", len/2, SDL_GetTicks());
	
	len /= 2;
	Sint16 *buffer = (Sint16 *)stream;

	if(soundTimer == 0)
	{
		for(int i=0; i<len; i++) buffer[i] = 0;
		return;
	}

	for(int i=0; i<len; i++)
	{
		double step = audioLevel + audioStep;
		if(step > 1.0) step = -1.0;
		audioLevel = step;
		
		buffer[i] = (Sint16)(audioVolume * step);
	}
}

void Chip8::CleanupSDL()
{
	init = false;

	if(texture != nullptr)
	{
		SDL_DestroyTexture(texture);
		texture = nullptr;
	}
	if(renderer != nullptr)
	{
		SDL_DestroyRenderer(renderer);
		renderer = nullptr;
	}
	if(window != nullptr)
	{
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	if(audioDevice > 0)
	{
		SDL_CloseAudioDevice(audioDevice);
		audioDevice = 0;
	}
}

bool Chip8::InitSDL()
{
	CleanupSDL();

	// Check to see if the call to SDL_Init() was successful.
	int mask = SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_AUDIO;
	if(SDL_WasInit(mask) != mask)
	{
		printf("SDL is not initialized.\n");
		return false;
	}

	// Initialize graphics.
	window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W*pixelScale, H*pixelScale, SDL_WINDOW_SHOWN);
	if(window == nullptr)
	{
		printf("SDL_CreateWindow error: %s\n", SDL_GetError());
		return false;
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if(renderer == nullptr)
	{
		printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
		return false;
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, W, H);
	if(texture == nullptr)
	{
		printf("SDL_CreateTexture error: %s\n", SDL_GetError());
		return false;
	}

	// Initialize audio components.
	SDL_AudioSpec want, have;
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = 2048;
	want.callback = AudioCallback;
	want.userdata = this;

	const char *device = nullptr;
	if(preferredAudio.length() > 0) device = preferredAudio.c_str();

	audioDevice = SDL_OpenAudioDevice(device, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if(audioDevice == 0)
	{
		printf("SDL_OpenAudioDevice error: %s\nRunning without audio support!\n", SDL_GetError());
	}else{
		printf("Using audio driver: '%s'\n", SDL_GetCurrentAudioDriver());
		//printf(" frequency: %d format: f %d s %d be %d sz %d channels: %d samples: %d\n", have.freq, SDL_AUDIO_ISFLOAT(have.format), SDL_AUDIO_ISSIGNED(have.format), SDL_AUDIO_ISBIGENDIAN(have.format), SDL_AUDIO_BITSIZE(have.format), have.channels, have.samples);
		
		int frequency = 400; // Hz.
		audioStep = 2.0 / (1.0 * have.freq / frequency);
		SetVolume(0.1f);
		audioLevel = 0.0;

		SDL_PauseAudioDevice(audioDevice, 0);
	}

	return true;
}

void Chip8::ShowAudioDevices()
{
	if(SDL_WasInit(SDL_INIT_AUDIO) == 0)
	{
		printf("Failed to retrieve audio devices: Audio subsystem is not initialized!\n");
		return;
	}

	printf("====================================================\n");

	int numAudioDevices = SDL_GetNumAudioDevices(0);
	printf("%d audio devices:\n", numAudioDevices);
	for(int i=0; i<numAudioDevices; i++)
	{
		printf(" \"%s\"\n", SDL_GetAudioDeviceName(i, 0));
	}

	int numAudioDrivers = SDL_GetNumAudioDrivers();
	printf("%d audio drivers:", numAudioDrivers);
	for(int i=0; i<numAudioDrivers; i++)
	{
		printf(" '%s'", SDL_GetAudioDriver(i));
	}

	printf("\n====================================================\n");
}

void Chip8::SetVolume(float volumeLevel)
{
	audioVolume = volumeLevel * SHRT_MAX;

	if(audioVolume < 0.0f) audioVolume = 0.0f;
	else if(audioVolume > (float)SHRT_MAX) audioVolume = (float)SHRT_MAX;
}

bool Chip8::LoadProgram(const std::string &fileName)
{
	// Make sure the program is an acceptable size.
	struct stat status;
	int success = stat(fileName.c_str(), &status);
	if(success != 0 || status.st_size <= 0 || status.st_size > MAX_PROGRAM_SIZE)
	{
		if(success == -1)
		{
			printf("Failed to load program.. Missing or invalid file: %s\n", fileName.c_str());
		}else{
			printf("Failed to load program.. Program size of %ld bytes exceeds maximum size of %d bytes.\n", status.st_size, MAX_PROGRAM_SIZE);
		}
		
		return false;
	}

	std::ifstream input(fileName.c_str(), std::ios::in|std::ios::binary);
	if (!input.is_open())
	{
		printf("Failed to load program.. Failed to open file: %s\n", fileName.c_str());
		return false;
	}

	input.read((char *)&memory[PROGRAM_SPACE], MAX_PROGRAM_SIZE);
	printf("Loaded program.. %s (%ld bytes)\n", fileName.c_str(), (long int)input.gcount());

	Reset();

	return true;
}

void Chip8::SetKey(uint8_t key, bool pressed)
{
	if(pressed)
	{
		keys |= (1 << key);
	}else{
		keys &= ~(1 << key);
	}
}

void Chip8::ClearScreen()
{
	display.reset();
	screenUpdated = true;
}

void Chip8::DrawScreen()
{
	if(!init) return;
	if(!screenUpdated) return; // Don't draw the screen unless it has changed.

	for(int i=0; i<W*H; i++)
	{
		if(display[i])
		{
			pixels[i] = foreground;
		}else{
			pixels[i] = background;
		}
	}

	SDL_UpdateTexture(texture, NULL, pixels, W*sizeof(uint32_t));

	//SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	screenUpdated = false;
}

void Chip8::Run()
{
	if(!InitSDL())
	{
		CleanupSDL();

		printf("Failed to run: SDL setup failed!\n");
		return;
	}

	init = true; // Created the SDL window successfully!

	unsigned int insPerFrame = std::max(1u, ips/FPS/2);
	unsigned int consecutiveIns = 0;
	unsigned int framesFinished = 0;

	SDL_Event event;
	bool running = true;
	auto start = std::chrono::high_resolution_clock::now();

	printf("Running program at: %u IPS.. (%u)\n", ips, insPerFrame);

	static std::unordered_map<int, int> keymap = {
		{SDL_SCANCODE_1, 0x1}, {SDL_SCANCODE_2, 0x2}, {SDL_SCANCODE_3, 0x3}, {SDL_SCANCODE_4, 0xC},
		{SDL_SCANCODE_Q, 0x4}, {SDL_SCANCODE_W, 0x5}, {SDL_SCANCODE_E, 0x6}, {SDL_SCANCODE_R, 0xD},
		{SDL_SCANCODE_A, 0x7}, {SDL_SCANCODE_S, 0x8}, {SDL_SCANCODE_D, 0x9}, {SDL_SCANCODE_F, 0xE},
		{SDL_SCANCODE_Z, 0xA}, {SDL_SCANCODE_X, 0x0}, {SDL_SCANCODE_C, 0xB}, {SDL_SCANCODE_V, 0xF},
	};

	while(running && !halt)
	{
		// Execute CPU for consecutiveIns OR until the CPU is waiting for a key to be pressed.
		for(unsigned int i=0; i<consecutiveIns && !(waitingKey & WAITINGKEY_FLAG); i++)
		{
			ExecuteInstruction();
		}
		// Handle window events.
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_QUIT:
					running = false;
					break;
				case SDL_KEYUP:
				case SDL_KEYDOWN:
					auto key = keymap.find(event.key.keysym.scancode);
					if(key == keymap.end()) break;

					uint16_t flag = (1 << (key->second & 0xF));
					if(event.type == SDL_KEYDOWN)
					{
						keys |= flag;
					}else{
						keys &= ~flag;
					}

					if(event.type == SDL_KEYDOWN && waitingKey & WAITINGKEY_FLAG)
					{
						V[waitingKey & 0xF] = key->second & 0xF;
						waitingKey = 0;
					}
			}
		}
		
		std::chrono::duration<double> elapsedSeconds = std::chrono::high_resolution_clock::now() - start;
		int frames = int(elapsedSeconds.count() * FPS) - framesFinished;
		if(frames > 0)
		{
			framesFinished += frames;
			// Timer registers decrement at a rate of 60 Hz.
			delayTimer -= std::min(frames, int(delayTimer));
			soundTimer -= std::min(frames, int(soundTimer));

			DrawScreen();
		}

		consecutiveIns = std::max(1, frames) * insPerFrame;
		if(waitingKey & WAITINGKEY_FLAG || !frames) SDL_Delay(1000/FPS);
	}

	printf("Program terminated.\n");

	CleanupSDL(); // Finished running so destroy the window. SDL still remains initialized until the object is destroyed.
}

void Chip8::DumpRegisters()
{
	printf("Register dump:\n\t  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F\nV[]\t= ");
	for(int i=0; i<MAX_REGISTERS; i++) printf("%03X ", V[i]);
	printf("\nS[]\t= ");
	for(int i=0; i<MAX_REGISTERS; i++) printf("%03X ", stack[i]);

	printf("\nSP\t= 0x%X\nI\t= 0x%X\nPC\t= 0x%X\nDT\t= 0x%X\nST\t= 0x%X\n", SP, I, PC, delayTimer, soundTimer);
}

void Chip8::DumpDisplay()
{
	printf("Display dump:");

	for(int i=0; i<W*H; i++)
	{
		if(i % W == 0)
		{
			printf("\n%2d: ", (i/W)+1);
		}

		if(display[i])
			printf("X "); // Pixel set.
		else
			printf("- "); // Pixel unset.
	}
	printf("\n");
}

void Chip8::Halt(const char *reason)
{
	halt = true;

	printf("Program halted: %s\n", reason);

	if(debug)
	{
		DumpRegisters();
	}
}

void PrintDebugHelp()
{
	printf("Debug mode is enabled. Use the following commands to execute the program:\n h - display this message\n n - continue to next instruction\n r - show all register values\n c - continue until interrupted\n d - show display state\n q - Stop debugger\n");
}

// Return true to execute the next instruction, false otherwise.
bool Chip8::DebuggerHandler()
{
	// Crude implementation of a debugger.
	if(debugState == DebugState_Run) return true;

	static bool once = false;
	if(!once)
	{
		once = true;
		PrintDebugHelp();
	}

	std::string command;
	while(true)
	{
		printf(":");
		std::cin >> command;
		char c = command.at(0);
		if(c == 'h')
		{
			// Requested help.
			PrintDebugHelp();
		}else if(c == 'n')
		{
			// Execute the next instruction.
			break;
		}else if(c == 'r')
		{
			// Show the value of all registers.
			DumpRegisters();
		}else if(c == 'c')
		{
			// Continue executing instructions until interrupted.
			debugState = DebugState_Run;
			break;
		}else if(c == 'd')
		{
			// Show the display state.
			DumpDisplay();
		}else if(c == 'q')
		{
			halt = true;
			return false;
		}
	}

	return true;
}

void Chip8::ExecuteInstruction()
{
	if(halt) return;

	// Instructions should start on even addresses but not all CHIP-8 programs follow that convention.
	if(!(PC >= PROGRAM_SPACE && PC < MAX_MEMORY))
	{
		SANITY_CHECK(false, "PC set to invalid address");
		return;
	}

	if(debug)
	{
		if(!DebuggerHandler())
		{
			return;
		}
	}

	// All instructions are 2 bytes long and stored in big-endian fashion.
	uint16_t opCode = (memory[PC] << 8)|memory[PC+1];
	PC += 2;

	// wxyz wnnn wxkk
	uint8_t w = (opCode >> 12) & 0xF;
	uint8_t x = (opCode >> 8) & 0xF;
	uint8_t y = (opCode >> 4) & 0xF;
	uint8_t z = opCode & 0xF;
	uint8_t kk = opCode & 0xFF;
	uint16_t nnn = opCode & 0xFFF;

	if(opCode == 0x00E0)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "00E0 - CLS: Clear the display.");

		ClearScreen();
	}else if(opCode == 0x00EE)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "00EE - RET: Return from a subroutine.");
		SANITY_CHECK(SP > 0, "Stack out of bounds");

		PC = stack[SP--];
	}else if(w == 0x1)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "1nnn - JP addr: Jump to location nnn.");

		PC = nnn;
	}else if(w == 0x2)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "2nnn - CALL addr: Call subroutine at nnn.");
		SANITY_CHECK(SP < STACK_SIZE-1, "Stack overflow");

		stack[++SP] = PC;
		PC = nnn;
	}else if(w == 0x3)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "3xkk - SE Vx, byte: Skip next instruction if Vx = kk.");

		if(V[x] == kk) PC += 2;
	}else if(w == 0x4)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "4xkk - SNE Vx, byte: Skip next instruction if Vx != kk.");

		if(V[x] != kk) PC += 2;
	}else if(w == 0x5 && z == 0x0)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "5xy0 - SE Vx, Vy: Skip next instruction if Vx = Vy.");

		if(V[x] == V[y]) PC += 2;
	}else if(w == 0x6)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "6xkk - LD Vx, byte: Set Vx = kk.");

		V[x] = kk;
	}else if(w == 0x7)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "7xkk - ADD Vx, byte: Set Vx = Vx + kk.");

		V[x] += kk;
	}else if(w == 0x8 && z == 0x0)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy0 - LD Vx, Vy: Set Vx = Vy.");

		V[x] = V[y];
	}else if(w == 0x8 && z == 0x1)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy1 - OR Vx, Vy: Set Vx = Vx OR Vy.");

		V[x] |= V[y];
	}else if(w == 0x8 && z == 0x2)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy2 - AND Vx, Vy: Set Vx = Vx AND Vy.");

		V[x] &= V[y];
	}else if(w == 0x8 && z == 0x3)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy3 - XOR Vx, Vy: Set Vx = Vx XOR Vy.");

		V[x] ^= V[y];
	}else if(w == 0x8 && z == 0x4)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy4 - ADD Vx, Vy: Set Vx = Vx + Vy, set VF = carry.");

		uint16_t sum = V[x] + V[y];
		V[0xF] = (sum >> 8);
		V[x] = sum & 0xFF;
	}else if(w == 0x8 && z == 0x5)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy5 - SUB Vx, Vy: Set Vx = Vx - Vy, set VF = NOT borrow.");

		uint16_t sub = V[x] - V[y];
		V[0xF] = !(sub >> 8);
		V[x] = sub & 0xFF;
	}else if(w == 0x8 && z == 0x6)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy6 - SHR Vx {, Vy}: Set Vx = Vy SHR 1.");

		V[0xF] = V[y] & 0x1;
		V[x] = V[y] >> 1;
	}else if(w == 0x8 && z == 0x7)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xy7 - SUBN Vx, Vy: Set Vx = Vy - Vx, set VF = NOT borrow.");

		uint16_t sub = V[y] - V[x];
		V[0xF] = !(sub >> 8);
		V[x] = sub & 0xFF;
	}else if(w == 0x8 && z == 0xE)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "8xyE - SHL Vx {, Vy}: Set Vx = Vy SHL 1.");

		V[0xF] = V[y] >> 7;
		V[x] = V[y] << 1;
	}else if(w == 0x9 && z == 0x0)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "9xy0 - SNE Vx, Vy: Skip next instruction if Vx != Vy.");

		if(V[x] != V[y]) PC += 2;
	}else if(w == 0xA)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Annn - LD I, addr: Set I = nnn.");

		I = nnn;
	}else if(w == 0xB)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Bnnn - JP V0, addr: Jump to location nnn + V0.");

		PC = nnn + V[0];

		if(PC < PROGRAM_SPACE)
		{
			printf("PC = 0x%X\n", PC);
			getchar();
		}
	}else if(w == 0xC)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Cxkk - RND Vx, byte: Set Vx = random byte AND kk.");

		V[x] = std::uniform_int_distribution<unsigned int>(0, 255)(rng) & kk;
	}else if(w == 0xD)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Dxyn - DRW Vx, Vy, nibble: Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.");
		SANITY_CHECK(I+z < MAX_MEMORY, "Invalid memory access by DRW");
		
		V[0xF] = 0x0;
		uint8_t pixelX = V[x];
		uint8_t pixelY = V[y];
		for(int height=0; height<z; height++)
		{
			uint16_t rowStart = W * ((pixelY+height) % H);
			for(int bit=0; bit<8; bit++)
			{
				uint16_t cell = rowStart + ((pixelX+bit) % W);

				bool pixel = display[cell] ^ ((memory[(I+height) & 0xFFF] >> (7-bit)) & 0x1);
				if(!pixel && display[cell]) V[0xF] = 0x1; // Set VF to 1 if any pixels are unset.
				display[cell] = pixel;
			}
		}

		screenUpdated = true;
	}else if(w == 0xE && kk == 0x9E)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Ex9E - SKP Vx: Skip next instruction if key with the value of Vx is pressed.");

		if(keys & (1 << V[x])) PC += 2;
	}else if(w == 0xE && kk == 0xA1)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "ExA1 - SKNP Vx: Skip next instruction if key with the value of Vx is not pressed.");

		if(!(keys & (1 << V[x]))) PC += 2;
	}else if(w == 0xF && kk == 0x07)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx07 - LD Vx, DT: Set Vx = delay timer value.");

		V[x] = delayTimer;
	}else if(w == 0xF && kk == 0x0A)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx0A - LD Vx, K: Wait for a key press, store the value of the key in Vx.");

		waitingKey = WAITINGKEY_FLAG|x;
	}else if(w == 0xF && kk == 0x15)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx15 - LD DT, Vx: Set delay timer = Vx.");

		delayTimer = V[x];
	}else if(w == 0xF && kk == 0x18)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx18 - LD ST, Vx: Set sound timer = Vx.");

		soundTimer = V[x];
	}else if(w == 0xF && kk == 0x1E)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx1E - ADD I, Vx: Set I = I + Vx.");

		I += V[x];
	}else if(w == 0xF && kk == 0x29)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx29 - LD F, Vx: Set I = location of sprite for digit Vx.");

		I = &font[(V[x] & 0xF)*5] - memory;
	}else if(w == 0xF && kk == 0x33)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx33 - LD B, Vx: Store BCD representation of Vx in memory locations I, I+1, and I+2.");
		SANITY_CHECK(I+2 < MAX_MEMORY, "Invalid memory access by LD");

		memory[I] = (V[x] / 100) % 10;
		memory[I+1] = (V[x] / 10) % 10;
		memory[I+2] = V[x] % 10;
	}else if(w == 0xF && kk == 0x55)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx55 - LD [I], Vx: Store registers V0 through Vx in memory starting at location I.");
		SANITY_CHECK(I+x < MAX_MEMORY, "Invalid memory access by LD");

		for(int i=0; i<=x; i++)
		{
			memory[I+i] = V[i];
		}
		I += x+1;
	}else if(w == 0xF && kk == 0x65)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "Fx65 - LD Vx, [I]: Read registers V0 through Vx from memory starting at location I.");
		SANITY_CHECK(I+x < MAX_MEMORY, "Invalid memory access by LD");

		for(int i=0; i<=x; i++)
		{
			V[i] = memory[I+i];
		}
		I += x+1;
	}else if(w == 0x0)
	{
		PRINT_DEBUG_INSTRUCTION(PC, opCode, "0nnn - SYS addr: Jump to a machine code routine at nnn. Skipped instruction.");
	}else{
		SANITY_CHECK(false, "Unhandled opcode");
		printf("Unhandled opcode: 0x%04X\n", opCode);
	}
}
