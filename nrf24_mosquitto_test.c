#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <mosquitto.h>

#include "drv.h"
#include "RF24.h"
#include "msg.pb-c.h"

/* */

void mqtt_callback_log(struct mosquitto *mqtt, void *user, int level, const char *str)
{
	printf("INFO MOSQUITTO<%d> %s\n", level, str);
}

void mqtt_callback_publish(struct mosquitto *mqtt, void *user, int mid)
{
	printf("INFO MOSQUITTO: message %d published\n", mid);
}

void mqtt_callback_connect(struct mosquitto *mqtt, void *user, int result)
{
	printf("INFO MOSQUITTO: connection response %d\n", result);
}

/* */

static struct rf24 nrf;

/* */

void nrf24_test_usage(char *name)
{
	printf("usage: %s [-h] -d <spidev>\n", name);
	printf("\t-h, --help\t\t\tthis help message\n");
	printf("\t-d, --device <spidev>\t\tspidev for nRF24L01, default is '/dev/spidev0.0\n");
	printf("\t--dynamic-payload\t\tenable dynamic payload support\n");
	printf("\t--payload-length <length>\tset static payload length to 0..32 bytes (default value is 32)\n");
	printf("\t--publish-message\t\tpublish messages to MQTT broker\n");
	printf("\t--peer1 <addr>\t\t\tnot yet supported\n");
	printf("\t--peer2 <addr>\t\t\tnot yet supported\n");
}

void dump_data(char *b, int n)
{
	int p;

	for(p = 0; p < n; p++) {
		printf("0x%02x ", *(b + p));
		if ((p > 0) && ((p % 64) == 0))
			printf("\n");
	}

	printf("\n");
}

void publish_data(struct mosquitto *m, char *b, int n)
{
	char mqtt_message[64];
	char mqtt_topic[64];
	NodeSensorList *msg;
	int i;

	msg = node_sensor_list__unpack(NULL, n, b);

	if (msg == NULL) {
		printf("error: can't unpack message\n");
		return;
	}

	printf("node = %lu\n", msg->node->node);
	for (i = 0; i < msg->n_sensor; i++) {
		memset(mqtt_message, 0x0, sizeof(mqtt_message));
		memset(mqtt_topic, 0x0, sizeof(mqtt_topic));
		printf("sensor[%lu] = %lu\n", msg->sensor[i]->type, msg->sensor[i]->data);
		snprintf(mqtt_message, sizeof(mqtt_message) - 1, "%lu", msg->sensor[i]->data);
		snprintf(mqtt_topic, sizeof(mqtt_message) - 1, "node%lu/sensor%lu",
			 msg->node->node, msg->sensor[i]->type);
		if(mosquitto_publish(m, NULL, mqtt_topic, strlen(mqtt_message), mqtt_message, 0, 0)) {
			printf("ERR: mosquitto could not publish message\n");
		}
	}

	node_sensor_list__free_unpacked(msg, NULL);

	return;
}

/* */

int main(int argc, char *argv[])
{
	uint8_t addr0[] = {'E', 'F', 'C', 'L', 'I'};
	uint8_t addr1[] = {'E', 'F', 'S', 'N', '1'};

	bool dynamic_payload = false;
	bool publish_message = false;

	uint8_t recv_buffer[32];
	int recv_length = 32;

	enum rf24_rx_status ret;
	struct rf24 *pnrf;
	int pipe;

	char *host = "localhost";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;

	struct mosquitto *mqtt = NULL;
	char *mqtt_id = "nrf24hub";

	/* command line options */

	char *spidev_name = "/dev/spidev0.0";

	char peer[2][6] = {
		"EFCLI\0",
		"EFSN1\0",
	};

	int opt;
	const char opts[] = "d:h:";
	const struct option longopts[] = {
		{"device", required_argument, NULL, 'd'},
		{"peer1", required_argument, NULL, '0'},
		{"peer2", required_argument, NULL, '1'},
		{"dynamic-payload", no_argument, NULL, '2'},
		{"payload-length", required_argument, NULL, '3'},
		{"publish-message", no_argument, NULL, '4'},
		{"help", optional_argument, NULL, 'h'},
		{NULL,}
	};

	while (opt = getopt_long(argc, argv, opts, longopts, &opt), opt > 0) {
		switch (opt) {
			case 'd':
				spidev_name = strdup(optarg);
				break;
			case '0':
				if (strlen(optarg) >= 5)
					strncpy(peer[0], optarg, 5);
				break;
			case '1':
				if (strlen(optarg) >= 5)
					strncpy(peer[1], optarg, 5);
				break;
			case '2':
				dynamic_payload = true;
				break;
			case '3':
				recv_length = atoi(optarg);
				if ((recv_length < 1) || (recv_length > 32)) {
					printf("ERR: invalid static payload length %d\n", recv_length);
					nrf24_test_usage(argv[0]);
					exit(-1);
				}
				break;
			case '4':
				publish_message = true;
				break;
			case 'h':
			default:
				nrf24_test_usage(argv[0]);
				exit(0);
		}
	}

	/* setup mosquitto */

	mosquitto_lib_init();

	mqtt = mosquitto_new(mqtt_id, clean_session, NULL);
	if (!mqtt) {
		printf("ERR: can not allocate mosquitto context\n");
		exit(-1);
	}

	mosquitto_log_callback_set(mqtt, mqtt_callback_log);
	mosquitto_publish_callback_set(mqtt, mqtt_callback_publish);
	mosquitto_connect_callback_set(mqtt, mqtt_callback_connect);

	if(mosquitto_connect(mqtt, host, port, keepalive)) {
		printf("ERR: can not connect to mosquitto server %s:%d\n", host, port);
		exit(-1);
	}

	if (mosquitto_loop_start(mqtt)) {
		printf("ERR: can not start mosquitto thread\n");
		exit(-1);
	}

	/* setup SPI and GPIO */

	pnrf = &nrf;
	memset(pnrf, 0x0, sizeof(*pnrf));

	if (0 > nrf24_driver_setup(pnrf, spidev_name)) {
		printf("ERR: can't setup gpio\n");
		exit(-1);
	}

	/* setup nRF24L01 */

	rf24_init(pnrf);
	rf24_print_status(pnrf);

	/* */

	if (dynamic_payload)
		rf24_enable_dyn_payload(pnrf);
	else
		rf24_set_payload_size(pnrf, recv_length);

	rf24_setup_prx(pnrf, 0x0 /* pipe number */, addr0);
	rf24_setup_prx(pnrf, 0x1 /* pipe number */, addr1);

	rf24_start_prx(pnrf);
	rf24_print_status(pnrf);

	/* */

	while (1) {

		while(!rf24_rx_ready(pnrf, &pipe))
			usleep(100000);

		ret = rf24_rx_pipe_check(pnrf, pipe);

		if (ret != RF24_RX_OK) {
			printf("WARN: pipe check error 0x%02x\n", (int)pipe);
			rf24_flush_rx(pnrf);
			continue;
		}

		printf("INFO: data ready in pipe 0x%02x\n", pipe);
		memset(recv_buffer, 0x0, sizeof(recv_buffer));

		if (dynamic_payload) {
			recv_length = (int)rf24_get_dyn_payload_size(pnrf);
			if (recv_length == 0xff) {
				printf("WARN: failed to get dynamic payload length\n");
				rf24_flush_rx(pnrf);
				continue;
			}
		}

		ret = rf24_recv(pnrf, recv_buffer, recv_length);
		if (ret != RF24_RX_OK) {
			printf("WARN: failed to receive rx data: 0x%02x\n", ret);
			rf24_flush_rx(pnrf);
			continue;
		}

		if (publish_message)
			publish_data(mqtt, (char *)recv_buffer, recv_length);
		else
			dump_data((char *)recv_buffer, recv_length);
	}
}
