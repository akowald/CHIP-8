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
#include <tclap/CmdLine.h>

#include "chip8.h"

class PixelConstraint : public TCLAP::Constraint<int>
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
	virtual bool check(const int &value) const
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

int main(int argc, char** argv)
{
	try{
		TCLAP::CmdLine cmd("Description of program", ' ', "0.1");
		
		TCLAP::UnlabeledValueArg<std::string> filePath("run", "Provide a relative or absolute path.", true, "", "Path to CHIP-8 program", cmd, false);
		TCLAP::SwitchArg listAudioSwitch("d", "list-audio-devices", "List the available audio devices.", cmd, false);
		TCLAP::ValueArg<std::string> audioDevice("a", "audio-device", "Provide the name of the audio device to use from the output of -d.", false, "", "device name", cmd);
		PixelConstraint pc;
		TCLAP::ValueArg<int> pixelScale("p", "pixel-scale", "Amount to scale each pixel in the 64x32 display.", false, 16, &pc, cmd);
		IPSConstraint ic;
		TCLAP::ValueArg<uint32_t> ips("i", "ips", "Number of instructions to execute per second. Default: 600", false, 600, &ic, cmd);
		VolumeConstraint vc;
		TCLAP::ValueArg<float> volume("v", "volume", "Volume level from 0 to 1. Default: 0.1", false, 0.1f, &vc, cmd);

		cmd.parse(argc, argv);

		Chip8 chip8(pixelScale.getValue(), audioDevice.getValue());

		chip8.SetIPS(ips.getValue());
		chip8.SetVolume(volume.getValue());
		if(listAudioSwitch.getValue())
		{
			chip8.ShowAudioDevices();
		}
		if(audioDevice.getValue().length() > 0)
		{
			chip8.SetPreferredAudioDevice(audioDevice.getValue());
		}

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