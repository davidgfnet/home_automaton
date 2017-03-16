
#include <regex>
#include <mutex>

#include <cxxhttpsrv/microrestd.h>
#include "page.h"
#include "devices.h"
#include "scheduler.h"

using namespace cxxhttpsrv;

extern std::mutex globalmutex;

class webui : public rest_service {
public:

	webui(std::vector<ButtonDevice> *buttons, Scheduler *sched)
	  : buttons(buttons), sched(sched) {

	}

protected:

	typedef std::unordered_map <std::string, std::string> VarMap;
	typedef std::vector <VarMap> VarMapList;
	typedef VarMapList (webui::*htmlgenerator)();

	const std::unordered_map<std::string, htmlgenerator> listmap = {
		{"device_status", &webui::deviceStatus},
		{"schedule_items", &webui::schedEvents},
		{"override_items", &webui::schedOverrides},
		{"dev_list_picker", &webui::deviceListPicker},
	};

	virtual bool handle(rest_request& req) override {
		if (req.method != "GET" && req.method != "POST") return req.respond_method_not_allowed("GET, POST");

		if (req.url == "/")
			return returnSection(req, "status");
		else if (req.url == "/schedule")
			return returnSection(req, "schedule");
		else if (req.url == "/overrides")
			return returnSection(req, "overrides");
		else if (req.url == "/addevent")
			return addEvent(req);
		else if (req.url == "/addoverride")
			return addOverride(req);
		else if (req.url == "/deloverride")
			return delOverride(req);

		return req.respond_not_found();
	}

	bool addOverride(rest_request& req) {
		globalmutex.lock();
		sched->addOverride(atoi(req.params["pluglist"].c_str()),
			               atoi(req.params["offset"].c_str()),
			               atoi(req.params["duration"].c_str()),
			               atoi(req.params["status"].c_str()));
		globalmutex.unlock();
		return returnSection(req, "overrides");
	}
	bool delOverride(rest_request& req) {
		globalmutex.lock();
		sched->delOverride(atoi(req.params["id"].c_str()));
		globalmutex.unlock();
		return returnSection(req, "overrides");
	}

	bool addEvent(rest_request& req) {
		globalmutex.lock();
		if (req.params["editid"] == "") {
			sched->addEvents(atoi(req.params["pluglist"].c_str()),
				             atoi(req.params["hour"].c_str()),
				             atoi(req.params["minute"].c_str()),
				             atoi(req.params["duration"].c_str()),
				             atoi(req.params["repeat"].c_str()));
		} else {
			sched->editEvent(atoi(req.params["editid"].c_str()),
			                 atoi(req.params["pluglist"].c_str()),
			                 atoi(req.params["hour"].c_str()),
			                 atoi(req.params["minute"].c_str()),
			                 atoi(req.params["duration"].c_str()),
			                 atoi(req.params["repeat"].c_str()));
		}
		globalmutex.unlock();

		// Just render schedule page again
		return returnSection(req, "schedule");
	}

	std::string replaceVars(std::string text, const VarMap & vars) {
		for (const auto var: vars)
			text = std::regex_replace(text, std::regex("\\{" + var.first + "\\}"), var.second);
		return text;
	}

	VarMapList deviceStatus() {
		VarMapList ret;
		globalmutex.lock();

		for (unsigned i = 0; i < buttons->size(); i++) {
			VarMap var;
			var["PLUG_ID"] = std::to_string(i);
			var["PLUG_NAME"] = buttons->at(i).name;
			var["PLUG_STATE"] = buttons->at(i).status == buttonON ? "on" : "off";
			ret.push_back(var);
		}

		globalmutex.unlock();
		return ret;
	}

	VarMapList deviceListPicker() {
		VarMapList ret = deviceStatus();
		ret.push_back({{"PLUG_ID", "-1"}, {"PLUG_NAME", "ALL"}});

		return ret;
	}

	VarMapList schedEvents() {
		VarMapList ret;
		globalmutex.lock();

		for (unsigned i = 0; i < sched->items.size(); i++) {
			const auto b = sched->items.at(i);
			VarMap var;
			var["SCHED_START"] = fmtTime(b.start.hour, b.start.minute);
			var["SCHED_START_HOUR"] = std::to_string(b.start.hour);
			var["SCHED_START_MINUTE"] = std::to_string(b.start.minute);
			var["SCHED_END"] = fmtTime(b.start.hour + (b.start.minute + b.duration_minutes) / 60, (b.start.minute + b.duration_minutes) % 60);
			var["SCHED_DUR_H"] = fmtDuration(b.duration_minutes);
			var["SCHED_DUR_MIN"] = std::to_string(b.duration_minutes);
			var["SCHED_CYCLE"] = cycleStr[b.repeat];
			var["SCHED_CYCLEID"] = std::to_string(b.repeat);
			var["SCHED_DEVNAME"] = b.device < 0 ? "All" : buttons->at(b.device).name;
			var["SCHED_DEVID"] = std::to_string(b.device);
			var["SCHED_ID"] = std::to_string(i);
			ret.push_back(var);
		}

		globalmutex.unlock();
		return ret;
	}

	VarMapList schedOverrides() {
		VarMapList ret;
		globalmutex.lock();

		for (unsigned i = 0; i < sched->overrides.size(); i++) {
			const auto b = sched->overrides.at(i);
			VarMap var;
			var["OVERRIDE_START"] = fmtDateTime(b.start);
			var["OVERRIDE_END"] = fmtDateTime(b.start + b.duration_minutes * 60);
			var["OVERRIDE_DUR_H"] = fmtDuration(b.duration_minutes);
			var["OVERRIDE_STATE"] = b.state == buttonON ? "ON" : "OFF";
			var["OVERRIDE_DEVNAME"] = b.device < 0 ? "All" : buttons->at(b.device).name;
			var["OVERRIDE_DEVID"] = std::to_string(b.device);
			var["OVERRIDE_ID"] = std::to_string(i);
			ret.push_back(var);
		}

		globalmutex.unlock();
		return ret;
	}

	std::string expandPage(const std::vector<p_entry> & toexpand, std::string sname) {
		std::string ret;
		for (const auto entry: toexpand) {
			if (entry.type == "list") {
				auto glist = listmap.at(entry.name);
				auto items = (this->*glist)();
				std::string ittext = (entry.subs.size()) ? expandPage(entry.subs, sname) : entry.content;
				for (const auto it : items)
					ret += replaceVars(ittext, it);
			}
			else if (entry.type != "section" || sname == entry.name) {
				if (entry.subs.size())
					ret += expandPage(entry.subs, sname);
				else
					ret += entry.content;
			}
		}
		return ret;
	}

	// Returns the index file filtering one of the sections
	bool returnSection(rest_request& req, std::string sname) {
		return req.respond("text/html", expandPage(page_html, sname));
	}

	std::vector<ButtonDevice> *buttons;
	Scheduler *sched;
};

