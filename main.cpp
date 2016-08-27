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
#include <iostream>
#include <unordered_map>
#include <tclap/CmdLine.h>

#include "chip8.h"

class PixelConstraint : public TCLAP::Constraint<unsigned int>
{
public:
	virtual std::string description() const
	{
		return "Must be between 1-1000.";
	}
	virtual std::string shortID() const
	{
		return "amount";
	}
	virtual bool check(const unsigned int &value) const
	{
		return value >= 1 && value <= 1000;
	}
};

class IPSConstraint : public TCLAP::Constraint<uint32_t>
{
public:
	virtual std::string description() const
	{
		return "Cannot be lower than 60.";
	}
	virtual std::string shortID() const
	{
		return "ips";
	}
	virtual bool check(const uint32_t &value) const
	{
		return value >= 60;
	}
};

class VolumeConstraint : public TCLAP::Constraint<float>
{
public:
	virtual std::string description() const
	{
		return "Must be between 0-1.";
	}
	virtual std::string shortID() const
	{
		return "volume";
	}
	virtual bool check(const float &value) const
	{
		return value >= 0.0f && value <= 1.0f;
	}
};

class HexStringConstraint : public TCLAP::Constraint<std::string>
{
public:
	virtual std::string description() const
	{
		return "Must be a hexadecimal number between 0-FFFFFF.";
	}
	virtual std::string shortID() const
	{
		return "RRGGBB";
	}
	virtual bool check(const std::string &value) const
	{
		try
		{
			unsigned long result = std::stoul(value, nullptr, 16);
			if(result > 0xFFFFFF) return false;
		}catch(std::exception&)
		{
			return false;
		}

		return true;
	}
};

struct ColorScheme
{
	unsigned int bg;
	unsigned int fg;
};

static std::unordered_map<std::string, ColorScheme> schemes = {
	{"autumn", ColorScheme{0x996600, 0xFFCC00}},
	{"deep blue", ColorScheme{0x000080,0xFFFFFF}},
};

std::string GetColorSchemeList()
{
	std::string list = "Available color schemes: ";
	bool first = true;
	for(auto it = schemes.begin(); it != schemes.end(); it++)
	{
		if(first)
		{
			first = false;
			list += it->first;
		}else{
			list += ", " + it->first;
		}
	}

	return list;
}

class ColorSchemeConstraint : public TCLAP::Constraint<std::string>
{
public:
	virtual std::string description() const
	{
		return GetColorSchemeList();
	}
	virtual std::string shortID() const
	{
		return "color scheme";
	}
	virtual bool check(const std::string &value) const
	{
		std::string scheme = value;
		std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);

		return schemes.find(scheme) != schemes.end();
	}
};

int main(int argc, char** argv)
{
	try{
		TCLAP::CmdLine cmd("A CHIP-8 interpreter written in C++.", ' ', "0.1");
		
		TCLAP::UnlabeledValueArg<std::string> filePath("run", "Provide a relative or absolute path.", true, "", "Path to CHIP-8 program", cmd, false);
		TCLAP::SwitchArg listAudioDevices("l", "list-audio-devices", "List the available audio devices.", cmd, false);
		TCLAP::ValueArg<std::string> audioDevice("a", "audio-device", "Provide the name of the audio device to use from the output of -l.", false, "", "device name", cmd);
		PixelConstraint pc;
		TCLAP::ValueArg<unsigned int> pixelScale("p", "pixel-scale", "Amount to scale each pixel in the 64x32 display. Default: 16", false, 16, &pc, cmd);
		IPSConstraint ic;
		TCLAP::ValueArg<uint32_t> ips("i", "ips", "Number of instructions to execute per second. Default: 600", false, 600, &ic, cmd);
		VolumeConstraint vc;
		TCLAP::ValueArg<float> volume("v", "volume", "Volume level from 0 to 1. Default: 0.1", false, 0.1f, &vc, cmd);
		TCLAP::SwitchArg debugMode("d", "debug", "Enable debuging mode.", cmd, false);
		HexStringConstraint hc;
		TCLAP::ValueArg<std::string> background("b", "background", "Background color in RRGGBB hexadecimal format.", false, "", &hc, cmd);
		TCLAP::ValueArg<std::string> foreground("f", "foreground", "Foreground color in RRGGBB hexadecimal format.", false, "", &hc, cmd);
		ColorSchemeConstraint csc;
		TCLAP::ValueArg<std::string> colorScheme("c", "color-scheme", GetColorSchemeList(), false, "", &csc, cmd);

		cmd.parse(argc, argv);

		Chip8 chip8;

		chip8.SetIPS(ips.getValue());
		chip8.SetVolume(volume.getValue());
		chip8.EnableDebug(debugMode.getValue());
		chip8.SetPixelScale(pixelScale.getValue());
		
		if(audioDevice.isSet()) chip8.SetPreferredAudioDevice(audioDevice.getValue());

		if(background.isSet()) chip8.SetBackgroundColor(std::stoul(background.getValue(), nullptr, 16));
		if(foreground.isSet()) chip8.SetForegroundColor(std::stoul(foreground.getValue(), nullptr, 16));

		if(colorScheme.isSet())
		{
			std::string scheme = colorScheme.getValue();
			std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);

			auto key = schemes.find(scheme);
			if(key != schemes.end())
			{
				chip8.SetBackgroundColor(key->second.bg);
				chip8.SetForegroundColor(key->second.fg);
			}
		}

		if(listAudioDevices.getValue()) chip8.ShowAudioDevices();

		if(chip8.LoadProgram(filePath.getValue()))
		{
			chip8.Run();
		}
	}catch(TCLAP::ArgException &e)
	{
		std::cerr << "Error: " << e.error() << " for " << e.argId() << std::endl;
	}

	return 0;
}