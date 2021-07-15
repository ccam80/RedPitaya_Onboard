#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TCP_PORT 1001

#define CMA_ALLOC _IOWR('Z', 0, uint32_t)

// Starting RP Config
#define CH1_AMPL_INIT 32000				// Almost max
#define CH1_FREQ_INIT 1					// 1Hz
#define CH2_AMPL_INIT 32000				// Almost max
#define CH2_FREQ_INIT 1					// 1Hz
#define SAMPLING_DIVIDER_INIT 1250  	// 100 kHz

int interrupted = 0;

typedef struct config_struct {
	uint16_t CIC_divider;
	uint32_t ch1_freq;
	uint32_t ch2_freq;
	uint16_t ch1_ampl;
	uint16_t ch2_ampl;
} config_t;

void signal_handler(int sig)
{
	interrupted = 1;
}

int main ()
{
	int fd, sock_server, sock_client;
	int position, limit, offset;
	volatile uint32_t *rx_addr, *rx_cntr, *ch1_increment, *ch2_increment;
	volatile uint16_t *rx_rate, *ch1_ampl, *ch2_ampl;
	volatile uint8_t *rx_rst;
	volatile void *cfg, *sts, *ram;
	cpu_set_t mask;
	struct sched_param param;
	struct sockaddr_in addr;
	uint32_t size;
	int yes = 1;
	int config_error = -10;
	bool reset_due = false;

	config_t current_config, fetched_config = {SAMPLING_DIVIDER_INIT,
											   CH1_FREQ_INIT,
											   CH2_FREQ_INIT,
											   CH1_AMPL_INIT,
											   CH2_AMPL_INIT};

	// Pavel's config stuff - don not understand so do not touch. Seems important to have a CPU.
	memset(&param, 0, sizeof(param));
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &param);
	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);

	// Open GPIO memory section
	if((fd = open("/dev/mem", O_RDWR)) < 0)
	{
		perror("open\n");
		return EXIT_FAILURE;
	}

	// Map Status and config addresses, close memory section once mapped
	sts = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x40000000);
	cfg = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x40001000);
	close(fd);

	// Open contiguous data memory section
	if((fd = open("/dev/cma", O_RDWR)) < 0)
	{
		perror("open\n");
		return EXIT_FAILURE;
	}

	// This seems important... PD's code relating to contiguous data section
	size = 128*sysconf(_SC_PAGESIZE);
	if(ioctl(fd, CMA_ALLOC, &size) < 0)
	{
		perror("ioctl\n");
		return EXIT_FAILURE;
	}

	// Map shared zone
	ram = mmap(NULL, 128*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	// Assign GPIO/Config/Status pointers
	rx_rst = (uint8_t *)(cfg + 0);
	rx_rate = (uint16_t *)(cfg + 2);
	rx_addr = (uint32_t *)(cfg + 4);
	ch1_ampl = (uint16_t *)(cfg + 16);
	ch1_increment = (uint32_t *)(cfg + 8);
	ch2_ampl = (uint16_t *)(cfg + 18);
	ch2_increment = (uint32_t *)(cfg + 12);
	rx_cntr = (uint32_t *)(sts + 12);

	// PD's assignment - think this sets current read address at top of section
	*rx_addr = size;

	// Configure Socket
	if((sock_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket\n");
		return EXIT_FAILURE;
	}

	setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes));

	/* setup listening address */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(TCP_PORT);

	if(bind(sock_server, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind\n");
		return EXIT_FAILURE;
	}

	listen(sock_server, 1024);

	while(!interrupted)
	{
		/* enter reset mode */
		printf("reset entered\n");
		*rx_rst &= ~1;
		usleep(100);
		*rx_rst &= ~2;
		/* set sample rate */
		*rx_rate = current_config.CIC_divider;

		/* set channel parameters */
		*ch1_increment = (uint32_t)floor(current_config.ch1_freq / 125.0e6 * (1<<30) + 0.5);
		*ch2_increment = (uint32_t)floor(current_config.ch2_freq  / 125.0e6 * (1<<30) + 0.5);
		*ch1_ampl = current_config.ch1_ampl;
		*ch2_ampl = current_config.ch2_ampl;

		if((sock_client = accept(sock_server, NULL, NULL)) < 0)
		{
			perror("accept\n");
			return EXIT_FAILURE;
		}

		signal(SIGINT, signal_handler);

		/* enter normal operating mode */
		*rx_rst |= 3;

		limit = 32*1024;

		while(!reset_due)
		{
			/* read ram writer position */
			position = *rx_cntr;

			/* send 256 kB if ready, otherwise sleep 0.1 ms */
			if((limit > 0 && position > limit) || (limit == 0 && position < 32*1024))
			{
				offset = limit > 0 ? 0 : 256*1024;
				limit = limit > 0 ? 0 : 32*1024;
				if(send(sock_client, ram + offset, 256*1024, MSG_NOSIGNAL) < 0) break;
				printf("Send success\n");

				while(recv(sock_client, &fetched_config, sizeof(config_t), MSG_DONTWAIT) > 0)
				{
					printf("received\n");
					if (fetched_config.CIC_divider != current_config.CIC_divider &
					fetched_config.CIC_divider < 6250)
					{
						if (fetched_config.CIC_divider < 6250)
						{
							current_config.CIC_divider = fetched_config.CIC_divider;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
						}
					}
		 			
					if (fetched_config.ch1_freq != current_config.ch1_freq)
					{
						if (fetched_config.ch1_freq < 61440000)
						{
							current_config.ch1_freq = fetched_config.ch1_freq;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
						}
					}

					if (fetched_config.ch2_freq != current_config.ch2_freq)
					{
						if (fetched_config.ch2_freq < 61440000)
						{
							current_config.ch2_freq = fetched_config.ch2_freq;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
						}
					}
					
					if (fetched_config.ch1_ampl != current_config.ch1_ampl)
					{
						if (fetched_config.ch1_ampl < 32766)
						{
							current_config.ch1_ampl = fetched_config.ch1_ampl;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
						}
					}

					if (fetched_config.ch2_ampl != current_config.ch2_ampl)
					{
						if (fetched_config.ch2_ampl < 32766)
						{
							current_config.ch2_ampl = fetched_config.ch2_ampl;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
						}
					}
					printf("received\n");
				} 
				printf("next layer up\n");
			}
			else
			{
				usleep(100);
			}
		}
		printf("sigint\n");
		signal(SIGINT, SIG_DFL);
		close(sock_client);
	}
	printf("final reset\n");
	/* enter reset mode */
	*rx_rst &= ~1;
	usleep(100);
	*rx_rst &= ~2;

	close(sock_server);

	return EXIT_SUCCESS;
}
