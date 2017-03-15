
all:
	python webgen.py index.html
	g++ -o automaton page.cc main.cc async_mqtt_client/mqtt.cc -lcxxhttpserver -lgnutls -lgcrypt -Wall -O0 -ggdb

