
// Home Automaton
// A system to control IoT switches using a schedule
// over MQTT protocol

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <mutex>

#include "webui.h"
#include "devices.h"
#include "util.h"
#include "scheduler.h"
#include "async_mqtt_client/mqtt.h"

using namespace std;

mutex globalmutex;

bool finish = false;
void sig_handler(int signo) {
	cerr << "Caught signal, shutting down!" << endl;
	finish = true;
}

int main(int argc, char **argv) {
	// Load config file
	auto cfg_map = parsefile(argv[1]);
	vector<ButtonDevice> buttons = ButtonDevice::parseCfg(cfg_map["device"]);

	// Create web interface
	int port = atoi(cfg_map["httpport"][0].c_str());
	int threads = cfg_map.count("httpthreads") ? atoi(cfg_map["httpthreads"][0].c_str()) : 4;
	int connection_limit = cfg_map.count("httpconnlimit") ? atoi(cfg_map["httpconnlimit"][0].c_str()) : 32;

	rest_server server;
	server.set_log_file(stderr);
	server.set_max_connections(connection_limit);
	server.set_threads(threads);

	// Load schedule
	Scheduler sched(&buttons);
	sched.readSched(argv[2]);

	webui service(&buttons, &sched);
	if (!server.start(&service, port, false)) {
		cerr <<  "Cannot start REST server!" << endl;
		return 1;
	}

	signal(SIGINT, sig_handler);

	// Create MQTT connection and start looping for events
	int sockfd = -1;
	AsyncMQTTClient client("");
	while (!finish) {
		// Reconnect if needed to
		if (sockfd < 0) {
			// Recreate the client, we will push schedule anyway thus enforcing any lost pushes
			globalmutex.lock();
			client = AsyncMQTTClient("homeauto_" + to_string(getpid()), cfg_map["mqttuser"][0], cfg_map["mqttpass"][0]);
			sockfd = socket(AF_INET, SOCK_STREAM, 0);

			// Subscribe to the necessary events
			for (const auto button: buttons)
				client.subscribe(button.relay_path, 0);
			globalmutex.unlock();

			// Resolve DNS and connect
			struct hostent* hostinfo = gethostbyname(cfg_map["mqtthost"][0].c_str());
			if (hostinfo) {
				struct sockaddr_in server;
				memcpy(&server.sin_addr, hostinfo->h_addr_list[0], hostinfo->h_length);
				server.sin_family = AF_INET;
				server.sin_port = htons(atoi(cfg_map["mqttport"][0].c_str()));

				if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) >= 0) {
					cerr << "Connected to the MQTT server" << endl;
				}
				else {
					cerr << "Could not connect to the MQTT server!" << endl;
					close(sockfd); sockfd = -1; sleep(5);
				}
			}
			else {
				cerr << "Could not resolve IP for " << cfg_map["mqtthost"][0] << endl;
				close(sockfd); sockfd = -1; sleep(5);
			}
		}

		// Select and wait
		struct pollfd pfd = { .fd = sockfd, .events = POLLIN | POLLERR };
		if (client.hasOutput())
			pfd.events |= POLLOUT;
		poll(&pfd, 1, 5); // Do wait up to 5s, need to wake to update schedule!

		globalmutex.lock();
		// Proces input data
		char tmpb[1024];
		int r = recv(sockfd, tmpb, sizeof(tmpb), MSG_DONTWAIT);
		if (r > 0)
			client.inputCallback(string(tmpb, r));
		else if (r == 0 || (r < 0 && errno != EWOULDBLOCK && errno != EAGAIN)) {
			cerr << "read() closed/error in the connection" << endl;
			close(sockfd); sockfd = -1;
		}

		if (client.isConnected()) {
			// Sync states and push/retrieve updates!
			string topic, value;
			while (client.getMessage(topic, value)) {
				for (auto button: buttons) {
					if (button.relay_path == topic) {
						button.setValue(value);
						break;
					}
				}
			}
		}

		// Work schedule! Push any changes
		auto topush = sched.process();
		for (const auto b: topush)
			client.publish(buttons[b].relay_path, buttons[b].status == buttonON ? "1" : "0", 0);

		// Process any output data
		string tosend = client.getOutputBuffer(16*1024);
		int w = send(sockfd, tosend.data(), tosend.size(), MSG_DONTWAIT);
		if (w > 0)
			client.consumeOuput(w);
		else if (r < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
			cerr << "write() error in the connection" << endl;
			close(sockfd); sockfd = -1;
		}
		globalmutex.unlock();
	}

	// Shutdown!
	server.stop();

	// Serialize schedule to disk
	sched.writeSched(argv[2]);

	return 0;
}


