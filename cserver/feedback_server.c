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
#define FIXED_FREQ_INIT 65536					// 1Hz
#define A_CONST_INIT 1			 
#define B_CONST_INIT 0					
#define SAMPLING_DIVIDER_INIT 1250  	// 100 kHz

#define MODE_MASK 192

int interrupted = 0;

typedef struct config_struct {
	uint8_t trigger;
	uint8_t mode;
	uint16_t CIC_divider;
	uint32_t fixed_freq;
	uint32_t start_freq;
	uint32_t stop_freq;
	uint32_t a_const;
	uint32_t interval;
	uint16_t b_const;
} config_t;

void signal_handler(int sig)
{
	interrupted = 1;
}

int main ()
{
	int fd, sock_server, sock_client;
	int position, limit, offset;

	// Shared memory pointers
	volatile uint32_t *rx_addr, *rx_cntr, *a_const, *fixed_phase, *start_freq, *stop_freq, *interval;
	volatile uint16_t *rx_rate, *b_const;
	volatile uint8_t *rx_rst;
	uint8_t trigger = 0;
	volatile void *cfg, *sts, *ram;
	cpu_set_t mask;
	struct sched_param param;
	struct sockaddr_in addr;
	uint32_t size;
	int YES = 1;
	int config_error = -10;
	bool reset_due = false;

	// Initialise config structs - current and next
	config_t fetched_config, current_config = 	{.trigger = 0,
												.mode = 1,
												.CIC_divider = SAMPLING_DIVIDER_INIT,
					    						.fixed_freq = FIXED_FREQ_INIT,
												.start_freq = 0,
												.stop_freq = 0,
												.a_const = A_CONST_INIT,
												.interval = 1,
												.b_const = B_CONST_INIT};

	// Config from Pavel Demin's adc_test. Comments are my understanding of their function. 
	memset(&param, 0, sizeof(param));     						// Initialize memory of scheduler parameter block to 0?
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);  // Set max priority for FIFO buffer?
	sched_setscheduler(0, SCHED_FIFO, &param);					// Set scheduler with sched priority
	
	//Pick CPU
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

	// PD's code relating to contiguous data section
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
	rx_cntr = (uint32_t *)(sts + 12);
	
	
	//Customisable parameter space
	fixed_phase = (uint32_t *)(cfg + 8);
	start_freq = (uint32_t *)(cfg + 8);
	stop_freq = (uint32_t *)(cfg + 12);
	a_const = (uint32_t *)(cfg + 12);
	interval = (uint32_t *)(cfg + 16);
	b_const = (uint16_t *)(cfg + 18);
	
	
	// Sets current read address at top of section
	*rx_addr = size;

	// Configure Socket
	if((sock_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket\n");
		return EXIT_FAILURE;
	}

	setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, (void *)&YES, sizeof(YES));

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
		/* set channel parameters */
		
		//Shared addresses toggled using mode bit
		if (current_config.mode == 0)
		{
			*fixed_phase = (uint32_t)floor(current_config.fixed_freq / 125.0e6 * (1<<30) + 0.5);
			*rx_rst = (uint8_t)((*rx_rst & (~MODE_MASK)) | (current_config.mode << 6));
			printf("State changed to %d\n", current_config.mode);
		} 
		else if (current_config.mode == 1)
		{
			*start_freq = current_config.start_freq;
			*stop_freq = current_config.stop_freq;
			*interval = current_config.interval;
			*rx_rst = (uint8_t)((*rx_rst & (~MODE_MASK)) | (current_config.mode << 6));
			printf("State changed to %d\n", current_config.mode);
		} 
		else if (current_config.mode == 2)
		{
			*a_const = current_config.a_const;
			*b_const = current_config.b_const;
			*rx_rst = (uint8_t)((*rx_rst & (~MODE_MASK)) | (current_config.mode << 6));
			printf("State changed to %d\n", current_config.mode);
		}
				
		printf("Saved config: \n"
				"trigger: %d \n"
				"state: %d\n"
				"CIC_divider: %d\n"
				"fixed_freq: %d\n"
				"start_freq: %d\n"
				"stop_freq: %d\n"
				"a_const: %d\n"
				"b_const: %d\n"
				"interval: %d\n\n",
				trigger,
				(*rx_rst & MODE_MASK) >> 6,
				*rx_rate,
				*fixed_phase,
				*start_freq,
				*stop_freq,
				*a_const,
				*b_const,
				*interval);

		//Non shared parameters and reset handling	
		//printf("params set\n");
		/* enter reset mode */
		reset_due = false;
		*rx_rst &= ~1;
		usleep(100);
		*rx_rst &= ~2;
		/* set sample rate */
		*rx_rate = current_config.CIC_divider;
		printf("reset complete\n");

		//Await connection from GUI
		if((sock_client = accept(sock_server, NULL, NULL)) < 0)
		{
			perror("accept\n");
			return EXIT_FAILURE;
		}
		printf("sock client accepted\n");

		//Set up interrupt handler
		signal(SIGINT, signal_handler);
		
		/* enter normal operating mode */
		
		// 32kb limit (functions as send "trigger")
		limit = 32*1024;
		
		while(!reset_due)
		{
			if (trigger)
			{
				// Enable RAM writer and CIC divider, send "go" signal to GUI
				printf("Triggered");
				if (~*rx_rst & 3) {
					*rx_rst |= 3;
					if(send(sock_client, (void *)&YES, sizeof(YES), MSG_NOSIGNAL) < 0) break;
				}

				/* read ram writer position */ 
				position = *rx_cntr;

				/* send 256 kB if ready, otherwise sleep 0.1 ms */
				if((limit > 0 && position > limit) || (limit == 0 && position < 32*1024))
				{
					offset = limit > 0 ? 0 : 256*1024;
					limit = limit > 0 ? 0 : 32*1024;
					// printf("sending\n");
					if(send(sock_client, ram + offset, 256*1024, MSG_NOSIGNAL) < 0) break;
				}
			}
			
			// Check for settings if not busy sending data
			else
			{	
				// For each field, check number makes sense and save to current config
				while(recv(sock_client, &fetched_config, sizeof(config_t), MSG_DONTWAIT) > 0)
				{	
					//TODO: Tidy away this into a function or some looping structure because it's unweildy
					// Is this a waste of time? Why not just overwrite the whole struct... - 9 assignments is minimal overhead
					
					//Print all fetched config
					printf("fetched config: \n"
							"trigger: %d\n"
							"state: %d\n"
							"CIC_divider: %d\n"
							"fixed_freq: %d\n"
							"start_freq: %d\n"
							"stop_freq: %d\n"
							"a_const: %d\n"
							"b_const: %d\n"
							"interval: %d\n\n",
							fetched_config.trigger,
							fetched_config.mode,
							fetched_config.CIC_divider,
							fetched_config.fixed_freq,
							fetched_config.start_freq,
							fetched_config.stop_freq,
							fetched_config.a_const,
							fetched_config.b_const,
							fetched_config.interval);
					
					
					if (fetched_config.trigger != current_config.trigger)
					{
						trigger = fetched_config.trigger;
					}
					
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
							reset_due = true;
						}
					}
		 			
		 			// Fixed phase
					if (fetched_config.fixed_freq != current_config.fixed_freq)
					{
						if (fetched_config.fixed_freq < 61440000)
						{
							current_config.fixed_freq = fetched_config.fixed_freq;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					// Start Freq
					if (fetched_config.start_freq != current_config.start_freq)
					{
						if (fetched_config.start_freq < 2000000)
						{
							current_config.start_freq = fetched_config.start_freq;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					//Stop Freq
					if (fetched_config.stop_freq != current_config.stop_freq)
					{
						if (fetched_config.stop_freq < 2000000)
						{
							current_config.stop_freq = fetched_config.stop_freq;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					//Interval
					if (fetched_config.interval != current_config.interval)
					{
						if (fetched_config.interval < 25000000)
						{
							current_config.interval = fetched_config.interval;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					// Multiplication constant (float)
					if (fetched_config.a_const != current_config.a_const)
					{
						if (fetched_config.a_const < 4294967295)
						{
							current_config.a_const = fetched_config.a_const;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					
					// addition constant
					if (fetched_config.b_const != current_config.b_const)
					{
						if (fetched_config.b_const < 32766)
						{
							current_config.b_const = fetched_config.b_const;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					// mode
					if (fetched_config.mode != current_config.mode)
					{
						if (fetched_config.mode < 4)
						{
							current_config.mode = fetched_config.mode;
							reset_due = true;
						}
						else {
							// Tell GUI that the numbers are wrong somehow
							// send(sock_client, &config_error, sizeof(config_error), MSG_NOSIGNAL) < 0
							reset_due = true;
						}
					}
					
					
				} 
				usleep(100);
			}
		}
		
		signal(SIGINT, SIG_DFL);
		close(sock_client);
	}
	/* enter reset mode */
	*rx_rst &= ~1;
	usleep(100);
	*rx_rst &= ~2;

	close(sock_server);

	return EXIT_SUCCESS;
}
