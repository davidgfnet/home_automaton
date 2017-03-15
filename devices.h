
#ifndef __DEVICES__H__
#define __DEVICES__H__

#include <iostream>
#include <fstream>

#include "util.h"

enum bStatus { buttonUnknown, buttonON, buttonOFF };

class ButtonDevice {
public:
	static std::vector<ButtonDevice> parseCfg(std::vector<std::string> devices) {
		std::vector<ButtonDevice> ret;
		for (const auto dev: devices) {
			auto c = dev.find(',');
			if (c != std::string::npos) {
				ret.push_back(ButtonDevice(
				    trim(dev.substr(0, c)), trim(dev.substr(c+1))));
			}
		}
		return ret;
	}

	ButtonDevice(std::string name, std::string relay_path)
	  : name(name), relay_path(relay_path), status(buttonUnknown) {}

	void setValue(std::string v) {
		if (v == "1")
			status = buttonON;
		else
			status = buttonOFF;
	}

	std::string name;
	std::string relay_path;
	bStatus status;
};

#endif

