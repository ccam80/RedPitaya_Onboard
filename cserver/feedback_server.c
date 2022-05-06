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
#define TRIG_MASK 4
#define CONFIG_ACK 2

int interrupted = 0;

typedef struct config_struct {
	uint16_t trigger;
	uint16_t mode;
	uint16_t CIC_divider;
	int16_t b_const;
	uint32_t fixed_freq;
	uint32_t start_freq;
	uint32_t stop_freq;
	uint32_t a_const;
	uint32_t interval;
} config_t;

typedef struct system_pointers {
	volatile uint32_t *rx_addr;
	volatile uint32_t *rx_cntr;
	volatile uint8_t *rx_rst;
	volatile void * ram;
} system_pointers_t;

typedef struct parameters {
	volatile uint32_t *a_const;
	volatile uint32_t *fixed_phase;
	volatile uint32_t *start_freq;
	volatile uint32_t *stop_freq;
	volatile uint32_t *interval;
	volatile uint16_t *rx_rate;
	volatile uint16_t *b_const;
} params_t;

void signal_handler(int sig)
{
	interrupted = 1;
}

// Receive message "header" bytes. Return acknowledge of config send if header ==0, otherwise echo back
// bytes_to_send to confirm a recording request
uint32_t get_socket_type(int sock_client)
{
	int32_t message = 0;
	uint32_t config_ack = 2;

	if(recv(sock_client, &message, sizeof(message), 0) > 0)
	{
		printf("Request message: %d\n", message);

		if (message == 0)
		{
			if (send(sock_client, &config_ack, sizeof(config_ack), MSG_NOSIGNAL) == sizeof(config_ack)) 
			{
				return message;
			} else 
			{
				perror("Message ack send failed");
				return EXIT_FAILURE;
			}
		}
		else
		{
			if (send(sock_client, &message, sizeof(message), MSG_NOSIGNAL) == sizeof(message)) 
			{
				return message;
			} else 
			{
				perror("Message ack send failed");
				return EXIT_FAILURE;
			}
		}	
	}

	else
	{ 
		perror("No message type received");
		return EXIT_FAILURE;
	}
}

uint32_t get_config(int sock_client, config_t* current_config_struct, config_t* fetched_config_struct, system_pointers_t *system_pointers)
{
	//Block waiting for config struct
	// TODO: Do we need to call mutliple times to ensure we receive the whole thing?
	if(recv(sock_client, fetched_config_struct, sizeof(config_t), 0) > 0)
	{	
		// Can't guarantee checking whole struct for inequality due to padding
		// Can replace with a whole struct overwrite if required but this will depend on overhead
		//difference between conditional tests and write operations. 

		//TODO: Consider having trigger in its own branch to speed up a trigger operation 
		
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
				fetched_config_struct->trigger,
				fetched_config_struct->mode,
				fetched_config_struct->CIC_divider,
				fetched_config_struct->fixed_freq,
				fetched_config_struct->start_freq,
				fetched_config_struct->stop_freq,
				fetched_config_struct->a_const,
				fetched_config_struct->b_const,
				fetched_config_struct->interval);
		
		
		if (fetched_config_struct->trigger != current_config_struct->trigger)
		{
			if (fetched_config_struct->trigger == 0)
			{
				*(system_pointers->rx_rst) &= ~TRIG_MASK;
				printf("Trigger off \n\n");
			}
			current_config_struct->trigger = fetched_config_struct->trigger;
		}
		
		if (fetched_config_struct->CIC_divider != current_config_struct->CIC_divider &
		fetched_config_struct->CIC_divider < 6250)
		{
			if (fetched_config_struct->CIC_divider < 6250)
			{
				current_config_struct->CIC_divider = fetched_config_struct->CIC_divider;
			}
		}
		
		// Fixed phase
		if (fetched_config_struct->fixed_freq != current_config_struct->fixed_freq)
		{
			if (fetched_config_struct->fixed_freq < 61440000)
			{
				current_config_struct->fixed_freq = fetched_config_struct->fixed_freq;
			}
		}
		
		// Start Freq
		if (fetched_config_struct->start_freq != current_config_struct->start_freq)
		{
			if (fetched_config_struct->start_freq < 2000000)
			{
				current_config_struct->start_freq = fetched_config_struct->start_freq;
			}
		}
		
		//Stop Freq
		if (fetched_config_struct->stop_freq != current_config_struct->stop_freq)
		{
			if (fetched_config_struct->stop_freq < 2000000)
			{
				current_config_struct->stop_freq = fetched_config_struct->stop_freq;
			}
		}
		
		//Interval
		if (fetched_config_struct->interval != current_config_struct->interval)
		{
			if (fetched_config_struct->interval < 25000000)
			{
				current_config_struct->interval = fetched_config_struct->interval;
			}
		}
		
		// Multiplication constant (float)
		if (fetched_config_struct->a_const != current_config_struct->a_const)
		{
			if (fetched_config_struct->a_const < 4294967295)
			{
				current_config_struct->a_const = fetched_config_struct->a_const;
			}
		}
		
		
		// addition constant
		if (fetched_config_struct->b_const != current_config_struct->b_const)
		{
			if (fetched_config_struct->b_const < 32766)
			{
				current_config_struct->b_const = fetched_config_struct->b_const;
			}
		}
		
		// mode
		if (fetched_config_struct->mode != current_config_struct->mode)
		{
			if (fetched_config_struct->mode < 4)
			{
				current_config_struct->mode = fetched_config_struct->mode;
			}
		}

	}	
}

uint32_t send_recording(int sock_client, int32_t bytes_to_send, system_pointers_t *system_pointers)
{
	// Enable RAM writer and CIC divider, send "go" signal to GUI
	int position, limit, offset = 0;
	int32_t YES = 1;

	limit = 32*1024;

		if (~(*(system_pointers->rx_rst) & 3)) {
			printf("Trigger on \n\n");
			//Send ack to GUI
			if(send(sock_client, (void *)&YES, sizeof(YES), MSG_DONTWAIT) < 0) {
				return -1;
			}

			//Turn on CIC compiler and RAM writer
			*(system_pointers->rx_rst) |= 3;

			//Trigger FPGA
			*(system_pointers->rx_rst) |= TRIG_MASK;

			//Reroute interrupts
			signal(SIGINT, signal_handler);
			
		}


	while (bytes_to_send > 0)
	{
		/* read ram writer position */ 

		position = *(system_pointers->rx_cntr);
		/* send 256 kB if ready, otherwise sleep 0.1 ms */
		if((limit > 0 && position > limit) || (limit == 0 && position < 32*1024))
		{
			offset = limit > 0 ? 0 : 256*1024;
			limit = limit > 0 ? 0 : 32*1024;
			// printf("sending\n");
			printf("\n bytes to send: %d \n", bytes_to_send);
			bytes_to_send -= send(sock_client, (system_pointers->ram) + offset, 256*1024, MSG_NOSIGNAL);			
		}

		else
		{
			usleep(100);
		}
	}

	return 1;
}

int main ()
{
	int fd, sock_server, sock_client;

	volatile void *cfg, *sts; // *ram;
	cpu_set_t mask;
	struct sched_param param;
	struct sockaddr_in addr;

	uint32_t size;
	int32_t bytes_to_send, message_type;
	int32_t YES = 1;
	int32_t config_error = -10;
	bool reset_due = false;

	// write bitstream to FPGA
	system("cat ../feedback.bit > /dev/xdevcfg ");

	// Initialise config structs - current and next
	config_t fetched_config, current_config = 	{.trigger = 0,
												.mode = 0,
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



	// Assign "system" pointers
	system_pointers_t system_regs = {.ram = 0,
									.rx_rst = (uint8_t *)(cfg + 0),
									.rx_addr = (uint32_t *)(cfg + 4),
									.rx_cntr = (uint32_t *)(sts + 12)};
	
	//Customisable parameter space
	params_t params = {.rx_rate = (uint16_t *)(cfg + 2),
					   .fixed_phase = (uint32_t *)(cfg + 8),
					   .start_freq = (uint32_t *)(cfg + 8),
					   .stop_freq = (uint32_t *)(cfg + 12),
					   .a_const = (uint32_t *)(cfg + 12),
					   .interval = (uint32_t *)(cfg + 16),
					   .b_const = (uint16_t *)(cfg + 18)};	

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

	system_regs.ram = mmap(NULL, 128*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	// Sets current read address at top of section
	*(system_regs.rx_addr) = size;

	// Create server socket
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

	signal(SIGINT, signal_handler);
	listen(sock_server, 1024);

	while(!interrupted)
	{
		/* print saved channel parameters */				
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
				(*(system_regs.rx_rst) & TRIG_MASK) >> 2,
				(*(system_regs.rx_rst) & MODE_MASK) >> 6,
				125.0e6 / *(params.rx_rate),
				*(params.fixed_phase),
				*(params.start_freq),
				*(params.stop_freq),
				*(params.a_const),
				*(params.b_const),
				*(params.interval)
				);

		//Reset RAM writer and filter
		*(system_regs.rx_rst) &= ~1;
		usleep(100);
		*(system_regs.rx_rst) &= ~2;
		/* set sample rate */
		*(params.rx_rate) = current_config.CIC_divider;
		printf("reset complete\n");
		reset_due = false;
		//Await connection from GUI

		while (!reset_due)
		{
			// Execution should block in this accept call until a client connects		
			if((sock_client = accept(sock_server, NULL, NULL)) < 0)
			{
				perror("accept\n");
				return EXIT_FAILURE;
			}
			printf("sock client accepted\n");

			message_type = get_socket_type(sock_client);

			if (message_type == 0)
			{
				get_config(sock_client, &current_config, &fetched_config, &system_regs);

				bytes_to_send = 0;

				if (current_config.mode == 0)
				{
					*(params.fixed_phase) = (uint32_t)floor(current_config.fixed_freq / 125.0e6 * (1<<30) + 0.5);
					*(system_regs.rx_rst) = (uint8_t)((*(system_regs.rx_rst) & (~MODE_MASK)) | (current_config.mode << 6));
					printf("State changed to %d\n", current_config.mode);
				} 
				else if (current_config.mode == 1)
				{
					*(params.start_freq) = current_config.start_freq;
					*(params.stop_freq) = current_config.stop_freq;
					*(params.interval) = current_config.interval;
					*(system_regs.rx_rst) = (uint8_t)((*(system_regs.rx_rst) & (~MODE_MASK)) | (current_config.mode << 6));
					printf("State changed to %d\n", current_config.mode);
				} 
				else if (current_config.mode == 2)
				{
					*(params.a_const) = current_config.a_const;
					*(params.b_const) = current_config.b_const;
					*(system_regs.rx_rst) = (uint8_t)((*(system_regs.rx_rst) & (~MODE_MASK)) | (current_config.mode << 6));
					printf("State changed to %d\n", current_config.mode);
				}
				reset_due = true;
			}

			// Assume any other number is a number of bytes to receive
			else
			{
				bytes_to_send = message_type;

				if (send_recording(sock_client, bytes_to_send, &system_regs) < 1)
				{
					printf("send_recording error");
				}
				
				reset_due = true;				
			}

		}
		
		signal(SIGINT, SIG_DFL);
		close(sock_client);
	}
	/* enter reset mode */
	*(system_regs.rx_rst) &= ~1;
	usleep(100);
	*(system_regs.rx_rst) &= ~2;

	close(sock_server);

	return EXIT_SUCCESS;
}
