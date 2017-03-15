
#ifndef __SCHEDULER__H___
#define __SCHEDULER__H___

#include "devices.h"

enum eventCycle {
	repDaily,          // Just every day at the particular time
	repWeeklySunday,   // Like before but also on that particular day
	repWeeklyMonday,
	repWeeklyTuesday,
	repWeeklyWednesday,
	repWeeklyThursday,
	repWeeklyFriday,
	repWeeklySaturday
};

const std::vector <std::string> cycleStr = {
	"Daily",
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
};


class Scheduler {
public:

	struct sched_time {
		unsigned hour, minute;
	};
	struct sched_event {
		sched_time start;           // When the event starts!
		unsigned duration_minutes;  // How long it lasts in minutes
		bStatus state;              // Value: ON or OFF
		eventCycle repeat;          // When to repeat the thing
		int device;                 // Device affected (-1 means all of them)
	};

	Scheduler(std::vector<ButtonDevice> *buttons) : buttons(buttons) {
		needs_push = true;          // The first time make sure to push status
		default_status = buttonOFF; // Default status when not specified (FIXME)
	}

	void addEvents(int device, unsigned hour, unsigned minute, unsigned duration, unsigned repeat) {
		sched_event ev;
		ev.start.hour = hour;
		ev.start.minute = minute;
		ev.duration_minutes = duration;
		ev.state = buttonON;
		ev.repeat = (eventCycle)repeat;
		ev.device = device;
		items.push_back(ev);
	}

	void readSched(std::string file) {
		auto schedcfg = parsefile(file);
		auto entries = schedcfg["schedevent"];
		for (const auto entry: entries) {
			auto fields = split(entry, ',');
			if (fields.size() == 5) {
				sched_event ev;
				ev.start.hour = atoi(fields[0].c_str());
				ev.start.minute = atoi(fields[1].c_str());
				ev.duration_minutes = atoi(fields[2].c_str());
				ev.state = buttonON;
				ev.repeat = (eventCycle)atoi(fields[3].c_str());
				ev.device = atoi(fields[4].c_str());
				items.push_back(ev);
			}
		}
	}

	void writeSched(std::string file) {
		std::ofstream outfile(file);
		for (const auto ev: items) {
			outfile << "schedevent=";
			outfile << ev.start.hour << ",";
			outfile << ev.start.minute << ",";
			outfile << ev.duration_minutes << ",";
			outfile << (unsigned)ev.repeat << ",";
			outfile << ev.device << std::endl;
		}
	}
	
	std::vector<unsigned> process() {
		std::vector<unsigned> ret;

		// Get current time
		std::time_t now = std::time(nullptr);
		std::tm *tinfo = gmtime(&now);

		// Secs since sunday
		uint32_t secs00 = (((tinfo->tm_wday * 24) + tinfo->tm_hour) * 60 + tinfo->tm_min) * 60 + tinfo->tm_sec;

		std::vector<bStatus> fstate(buttons->size(), default_status);
		for (const auto it: items) {
			if (it.repeat == repDaily) {
				for (unsigned i = 0; i < 7; i++) {
					if (checkOverlap(secs00, i, it)) {
						if (it.device < 0)
							fstate = std::vector<bStatus>(buttons->size(), it.state);
						else
							fstate[it.device] = it.state;
					}
				}
			}
			else if (checkOverlap(secs00, it.repeat - repWeeklySunday, it)) {
				if (it.device < 0)
					fstate = std::vector<bStatus>(buttons->size(), it.state);
				else
					fstate[it.device] = it.state;
			}
		}

		// Update buttons
		std::vector<bool> updated(buttons->size(), false);
		for (unsigned i = 0; i < buttons->size(); i++) {
			updated[i] = (buttons->at(i).status != fstate[i]);
			buttons->at(i).status = fstate[i];
			needs_push = needs_push or updated[i];
			if (updated[i])
				ret.push_back(i);
		}

		// Push update
		return ret;
	}

	// State of the system
	bool needs_push;
	bStatus default_status;

	// External devices
	std::vector<ButtonDevice> *buttons;

	// Schedule stuff
	std::vector <sched_event> items;

	bool checkOverlap(uint32_t tdelta, unsigned dow, const sched_event &ev) {
		uint32_t tstart = (((dow * 24 + ev.start.hour) * 60) + ev.start.minute) * 60;
		uint32_t tend   = tstart + ev.duration_minutes * 60;
		return (tdelta >= tstart && tdelta < tend);
	}
};


#endif
