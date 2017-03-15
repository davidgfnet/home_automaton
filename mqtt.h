
#include <MQTTAsync.h>
#include <MQTTClientPersistence.h>

#define def_callb(name, type) \
	static void name ## CB(void* context, type* response) { \
		((MQTTClient*)context)->name(response); \
	}

#define decl_callb(name, type) \
	static void name ## CB(void* context, type* response);


decl_callb(onConnect, MQTTAsync_successData)
decl_callb(onConnectFailure, MQTTAsync_failureData)
decl_callb(connectionLost, char)
int messageArrivedCB(void *context, char *topicName, int topicLen, MQTTAsync_message *message);

class MQTTClient {
public:

	MQTTClient(std::string uri, std::string clientid, std::string user, std::string pass)
	 : uri(uri), clientid(clientid), user(user), pass(pass) {

		
	}

	~MQTTClient() {
		//		MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
		//	disc_opts.onSuccess = onDisconnect;
		//if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)

		MQTTAsync_destroy(&client);
	}

	void connect() {
		auto rc = MQTTAsync_create(&this->client, uri.c_str(), clientid.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
		MQTTAsync_setCallbacks(client, this, connectionLostCB, messageArrivedCB, NULL);

		MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
		conn_opts.keepAliveInterval = 30;
		conn_opts.cleansession = 1;
		conn_opts.username = user.c_str();
		conn_opts.password = pass.c_str();
		conn_opts.onSuccess = onConnectCB;
		conn_opts.onFailure = onConnectFailureCB;
		conn_opts.context = this;

		if (MQTTAsync_connect(this->client, &conn_opts) != MQTTASYNC_SUCCESS)
			std::cerr << "MQTT connect() failed" << std::endl;
	}

	void onConnect(MQTTAsync_successData* response) {
		
	}

	void onConnectFailure(MQTTAsync_failureData* response) {
		
	}

	void connectionLost(char * cause) {
		
	}

	int messageArrived(char *topicName, int topicLen, MQTTAsync_message *message) {
		return 1;
	}

protected:
	MQTTAsync client;
	std::string uri, clientid, user, pass;
};

def_callb(onConnect, MQTTAsync_successData)
def_callb(onConnectFailure, MQTTAsync_failureData)
def_callb(connectionLost, char)

int messageArrivedCB(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
	return ((MQTTClient*)context)->messageArrived(topicName, topicLen, message);
}

