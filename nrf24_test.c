#include <inttypes.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "gpio.h"
#include "spi.h"

#include "RF24.h"

/* */

#ifdef SUNXI_KERNEL /* linux-sunxi-3.4 definitions */

#define PCDUINO_NRF24_CE        7
#define PCDUINO_NRF24_CE_NAME   "gpio7_ph9"

#define PCDUINO_NRF24_CSN       8
#define PCDUINO_NRF24_CSN_NAME  "gpio8_ph10"

#define PCDUINO_RFM69_CSN       9
#define PCDUINO_RFM69_CSN_NAME  "gpio9_ph5"

#define PCDUINO_NRF24_IRQ       2
#define PCDUINO_NRF24_IRQ_NAME  "gpio2_ph7"

#define PCDUINO_NRF24_SPI	"/dev/spidev0.0"

#else /* linux-master-4.0.0 definitions */

#define PCDUINO_NRF24_CE        233
#define PCDUINO_NRF24_CE_NAME   "gpio233"

#define PCDUINO_NRF24_CSN       234
#define PCDUINO_NRF24_CSN_NAME  "gpio234"

#define PCDUINO_RFM69_CSN       229
#define PCDUINO_RFM69_CSN_NAME  "gpio229"

#define PCDUINO_NRF24_IRQ       231
#define PCDUINO_NRF24_IRQ_NAME  "gpio231"

#define PCDUINO_NRF24_SPI	"/dev/spidev"

#endif

/* */

void radio_gpio_setup(void)
{
	pcduino_gpio_setup(PCDUINO_NRF24_CE, PCDUINO_NRF24_CE_NAME, DIR_OUT);
	pcduino_gpio_setup(PCDUINO_NRF24_CSN, PCDUINO_NRF24_CSN_NAME, DIR_OUT);
	pcduino_gpio_setup(PCDUINO_NRF24_IRQ, PCDUINO_NRF24_IRQ_NAME, DIR_IN);

    /* disable spi chip select for RFM69 */
	pcduino_gpio_setup(PCDUINO_RFM69_CSN, PCDUINO_RFM69_CSN_NAME, DIR_OUT);
	pcduino_gpio_write(PCDUINO_RFM69_CSN_NAME, 1);
}

void radio_spi_setup(char *name)
{
	pcduino_spi_open(name);
	pcduino_spi_info();
	pcduino_spi_init();
	pcduino_spi_info();

	return;
}

/* */

void f_csn(int level)
{
	pcduino_gpio_write(PCDUINO_NRF24_CSN_NAME, level);
}

void f_ce(int level)
{
	pcduino_gpio_write(PCDUINO_NRF24_CE_NAME, level);
}

void f_spi_set_speed(int khz)
{
	/* not implemented */
}

uint8_t f_spi_xfer(uint8_t dat)
{
	return pcduino_spi_xfer_fdx(dat);
}

struct rf24 nrf = {
	.csn = f_csn,
	.ce = f_ce,
	.spi_set_speed = f_spi_set_speed,
	.spi_xfer = f_spi_xfer,
};

/* */

void nrf24_test_usage(char *name)
{
	printf("usage: %s [-h] -d <spidev>\n", name);
	printf("\t-h, --help			this help message\n");
	printf("\t-d, --device <spidev>		spidev for nRF24L01, default is '/dev/spidev0.0\n");
	printf("\t--peer1 <addr>			not yet supported\n");
	printf("\t--peer2 <addr>			not yet supported\n");
}

/* */

int main(int argc, char *argv[])
{
	uint8_t addr0[] = {'E', 'F', 'C', 'L', 'I'};
	uint8_t addr1[] = {'E', 'F', 'S', 'N', '1'};

	uint8_t buf[20];

	uint32_t more_data;
	uint8_t pipe;

	struct rf24 *pnrf;

	/* command line options */

	char *spidev_name = PCDUINO_NRF24_SPI;

	char peer[2][6] = {
			"EFCLI\0",
			"EFSN1\0",
	};

	int opt;
	const char opts[] = "d:h";
    const struct option longopts[] = {
        {"device", required_argument, NULL, 'd'},
        {"help",   optional_argument, NULL, 'h'},
        {"peer1",  required_argument, NULL, '0'},
        {"peer2",  required_argument, NULL, '1'},
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
			case 'h':
			default:
				nrf24_test_usage(argv[0]);
				exit(0);
        }
    }

	/* setup SPI and GPIO */

	radio_gpio_setup();
	radio_spi_setup(spidev_name);

	/* setup nRF24L01 */

	rf24_init(&nrf);
	pnrf = &nrf;

	rf24_print_status(rf24_get_status(pnrf));

	/* */

	rf24_set_payload_size(pnrf, sizeof(buf));

	rf24_open_reading_pipe(pnrf, 0x0 /* pipe number */, addr0);
	rf24_open_reading_pipe(pnrf, 0x1 /* pipe number */, addr1);

	rf24_start_listening(pnrf);
	rf24_print_status(rf24_get_status(pnrf));
    rf24_print_details(pnrf);

	/* */

	while (1) {

		if (rf24_available(pnrf, &pipe)) {

			if ((pipe < 0) || (pipe > 5)) {
				printf("WARN: invalid pipe number 0x%02x\n", (int) pipe);
			} else {
				printf("INFO: data ready in pipe 0x%02x\n", pipe);
				memset(buf, 0x0, sizeof(buf));
				more_data = rf24_read(pnrf, buf, sizeof(buf));
				printf("INFO: data [%s]\n", buf);
				if (!more_data)
					printf("WARN: RX_FIFO not empty: %d\n", more_data);
			}
		}

		usleep(100000);
	}
}
