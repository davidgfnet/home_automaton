
#ifndef __UTIL__H__
#define __UTIL__H__

#include <iomanip>

static std::string trim(std::string line) {
	while (line.size() && (line[0] == ' ' || line[0] == '\t'))
		line = line.substr(1);
	while (line.size() && (line[line.size()-1] == ' ' || line[line.size()-1] == '\t'))
		line = line.substr(0, line.size()-1);
	return line;
}

static std::string fmtTime(unsigned hour, unsigned minute) {
	std::string h = hour < 10 ? "0" + std::to_string(hour) : std::to_string(hour);
	std::string m = minute < 10 ? "0" + std::to_string(minute) : std::to_string(minute);
	return h + ":" + m;
}

static std::string fmtDateTime(std::time_t t) {
	std::tm tm = *std::localtime(&t);
	std::stringstream ss;
	ss << std::put_time(&tm, "%d %b %H:%M");
	return ss.str();
}

static std::string fmtDuration(unsigned d) {
	if (d >= 60) {
		unsigned h = d / 60;
		unsigned m = d % 60;
		return std::to_string(h) + "h" + (m > 0 ? std::to_string(m) + "m" : "");
	}
	return std::to_string(d) + "m";
}

static const std::vector<std::string> split(std::string s, char c) {
	std::vector<std::string> ret;

	auto pos = s.find(c);
	while (pos != std::string::npos) {
		ret.push_back(s.substr(0, pos));
		s = s.substr(pos+1);
		pos = s.find(c);
	}
	ret.push_back(s);
	
	return ret;
}

static std::unordered_map< std::string, std::vector<std::string> > parsefile(std::string file) {
	std::unordered_map< std::string, std::vector<std::string> > ret;
	std::ifstream infile(file);

	std::string line;
	while (std::getline(infile, line)) {
		line = trim(line);

		auto c = line.find('=');
		if (c != std::string::npos)
			ret[trim(line.substr(0, c))].push_back(trim(line.substr(c+1)));
	}
	return ret;
}

#endif

