
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
	repWeeklySaturday,
	repEnd
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
	struct sched_override {
		time_t start;               // When the event starts!
		unsigned duration_minutes;  // How long it lasts in minutes
		bStatus state;              // Value: ON or OFF
		int device;                 // Device affected (-1 means all of them)
	};

	Scheduler(std::vector<ButtonDevice> *buttons) : buttons(buttons) {
		needs_push = true;          // The first time make sure to push status
		default_status = buttonOFF; // Default status when not specified (FIXME)
	}

	void addEvents(int device, unsigned hour, unsigned minute, unsigned duration, unsigned repeat) {
		if (repeat >= repEnd || duration > 7*24*60) return;
		sched_event ev;
		ev.start.hour = hour;
		ev.start.minute = minute;
		ev.duration_minutes = duration;
		ev.state = buttonON;
		ev.repeat = (eventCycle)repeat;
		ev.device = device;
		items.push_back(ev);
	}

	void editEvent(unsigned idx, int device, unsigned hour, unsigned minute, unsigned duration, unsigned repeat) {
		if (idx < 0 || idx >= items.size()) return;

		items[idx].start.hour = hour;
		items[idx].start.minute = minute;
		items[idx].duration_minutes = duration;
		items[idx].state = buttonON;
		items[idx].repeat = (eventCycle)repeat;
		items[idx].device = device;
	}

	void delEvent(unsigned idx) {
		if (idx < items.size())
			items.erase(items.begin() + idx);
	}

	void addOverride(int device, unsigned offset, unsigned duration, unsigned status) {
		sched_override ov;
		ov.start = time(0) + offset * 60;
		ov.duration_minutes = duration;
		ov.device = device;
		ov.state = status ? buttonON : buttonOFF;
		overrides.push_back(ov);
	}

	void delOverride(unsigned idx) {
		if (idx < overrides.size())
			overrides.erase(overrides.begin() + idx);
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
		entries = schedcfg["schedoverr"];
		for (const auto entry: entries) {
			auto fields = split(entry, ',');
			if (fields.size() == 4) {
				sched_override ov;
				ov.start = atoi(fields[0].c_str());
				ov.duration_minutes = atoi(fields[1].c_str());
				ov.state = atoi(fields[2].c_str()) ? buttonON : buttonOFF;
				ov.device = atoi(fields[3].c_str());
				overrides.push_back(ov);
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
		for (const auto ov: overrides) {
			outfile << "schedoverr=";
			outfile << ov.start << ",";
			outfile << ov.duration_minutes << ",";
			outfile << ov.state << ",";
			outfile << ov.device << std::endl;
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

		// Overrides!
		for (const auto ov: overrides) {
			if (now > ov.start && now < ov.start + ov.duration_minutes * 60) {
				if (ov.device < 0)
					fstate = std::vector<bStatus>(buttons->size(), ov.state);
				else
					fstate[ov.device] = ov.state;
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
	std::vector <sched_override> overrides;

	bool checkOverlap(uint32_t tdelta, unsigned dow, const sched_event &ev) {
		uint32_t tstart = (((dow * 24 + ev.start.hour) * 60) + ev.start.minute) * 60;
		uint32_t tend   = tstart + ev.duration_minutes * 60;
		// If the event finishes after the week is over, do a double check
		const unsigned wsecs = 7*24*60*60;
		if (tend > wsecs) {
			// By definition tstart2 will be < 0, thus tdelta > 0, check is always true
			int tend2 = tend - wsecs;
			if (tdelta < tend2)
				return true;  // Carry over from an event last week :)
		}

		return (tdelta >= tstart && tdelta < tend);
	}
};


#endif

