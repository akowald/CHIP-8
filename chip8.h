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
#pragma once

#include <cstdint>
#include <string>
#include <random>
#include <bitset>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Chip8
{
public:
	Chip8();
	~Chip8();

	bool LoadProgram(const std::string &fileName);
	void Run();
	void SetBackgroundColor(uint32_t color);
	void SetForegroundColor(uint32_t color);
	void SetIPS(uint32_t ips) { this->ips = ips; };
	void SetPixelScale(unsigned int pixelScale) { this->pixelScale = pixelScale; };
	void SetPreferredAudioDevice(const std::string &audioDevice) { preferredAudio = audioDevice; };

	void ShowAudioDevices();
	void SetVolume(float volumeLevel);
	void EnableDebug(bool enable) { debug = enable; };
private:
	static constexpr int W = 64; // Width of the screen in pixels.
	static constexpr int H = 32; // Height of the screen in pixels.
	static constexpr int MAX_MEMORY = 0x1000; // Total memory available to the interpreter.
	static constexpr int PROGRAM_SPACE = 0x200; // Program space is 0x200 and onwards.
	static constexpr int MAX_PROGRAM_SIZE = MAX_MEMORY - PROGRAM_SPACE;
	static constexpr int STACK_SIZE = 16;
	static constexpr int MAX_REGISTERS = 16;
	static constexpr uint8_t WAITINGKEY_FLAG = 0x10;
	static constexpr unsigned int FPS = 60;

	std::mt19937 rng;
	std::string preferredAudio;

	union
	{
		// Capable of accessing up to 4KB of RAM.
		uint8_t memory[MAX_MEMORY] = {0};
		// The first 512 bytes are reserved for use by the interpreter. (0x00 to 0x1FF)
		struct
		{
			// 16 general purpose 8-bit registers.
			uint8_t V[MAX_REGISTERS];
			// 16-bit register generally used to store memory addresses.
			uint16_t I;

			uint16_t PC;
			uint16_t stack[STACK_SIZE];
			uint8_t SP;
			uint8_t waitingKey;
			
			// Two special purpose 8-bit registers.
			uint8_t delayTimer;
			uint8_t soundTimer;
			// Font data for characters 0-F. Sprite size is 5 bytes.
			uint8_t font[16*5];
			// Data for the monochrome display.
			std::bitset<W*H> display;
			// Bit field for currently pressed keys.
			uint16_t keys;
		};
	};

	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;

	uint32_t *pixels;
	uint32_t background;
	uint32_t foreground;

	uint32_t ips;

	bool init;
	bool screenUpdated;
	bool halt;
	bool debug;
	int debugState;
	unsigned int pixelScale;

	void Reset();
	void ExecuteInstruction();
	void SetKey(uint8_t key, bool pressed);

	bool InitSDL();
	void CleanupSDL();

	void ClearScreen();
	void DrawScreen();
	void DumpRegisters();
	void DumpDisplay();
	void Halt(const char *reason);
	bool DebuggerHandler();

	static void AudioCallback(void *userdata, uint8_t *stream, int len);
	void SawtoothWave(uint8_t *stream, int len);
	double audioLevel;
	double audioStep;
	float audioVolume;
	uint32_t audioDevice;
};