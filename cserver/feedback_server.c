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
// #define FIXED_FREQ_INIT 65536			// 1Hz			
// #define SAMPLING_DIVIDER_INIT 1250  	// 100 kHz

// #define MODE_MASK 224
#define TRIG_BIT 2                         // bit 2
#define CONFIG_ACK 2                        // just a hard coded number to send back to GUI
#define CONTINUOUS_BIT 3					// bit 3
#define FAST_MODE_BIT 4					// bit 4
#define CH1_INPUT_MASK 1
#define CH1_MODE_MASK 30
#define CH2_INPUT_MASK 1
#define CH2_MODE_MASK 30
#define CBC_INPUT_MASK 0
#define CBC_VEL_EXT_MASK 1
#define CBC_DISP_EXT_MASK 2
#define CBC_POLY_TARGET_MASK 3

int interrupted = 0;



typedef struct system_pointers {
	volatile uint32_t *rx_addr;
	volatile uint32_t *rx_cntr;
	volatile uint8_t *rx_rst;
	void *ram;
} system_pointers_t;

// This is bit-packed, type-checked and range-bound in the python API, removing the need for a separate config struct type.
typedef struct parameter_pointers {
	volatile int8_t  *settings;
	volatile int8_t  *CH1_settings;
	volatile int8_t  *CH2_settings;
	volatile int8_t  *CBC_settings;
	volatile int32_t *param_a;
	volatile int32_t *param_b;
	volatile int32_t *param_c;
	volatile int32_t *param_d;
	volatile int32_t *param_e;
	volatile int32_t *param_f;
	volatile int32_t *param_g;
	volatile int32_t *param_h;
	volatile int32_t *param_i;
	volatile int32_t *param_j;
	volatile int32_t *param_k;
	volatile int32_t *param_l;
	volatile int32_t *param_m;
	volatile int32_t *param_n;
} params_t;

// This struct is to contain the values obtained from a fetch - it may not be required, as I could probably more 
// cleverly use the params_t struct more cleverly, but it's here for now.
typedef struct parameter_values {
	volatile int8_t  settings;
	volatile int8_t  CH1_settings;
	volatile int8_t  CH2_settings;
	volatile int8_t  CBC_settings;
	volatile int32_t param_a;
	volatile int32_t param_b;
	volatile int32_t param_c;
	volatile int32_t param_d;
	volatile int32_t param_e;
	volatile int32_t param_f;
	volatile int32_t param_g;
	volatile int32_t param_h;
	volatile int32_t param_i;
	volatile int32_t param_j;
	volatile int32_t param_k;
	volatile int32_t param_l;
	volatile int32_t param_m;
	volatile int32_t param_n;
} param_vals_t;

void signal_handler(int sig) {
	interrupted = 1;
}

// Receive message "header" bytes. Return acknowledge of config send if header ==0, otherwise echo back
// bytes_to_send to confirm a recording request
uint32_t get_socket_type(int sock_client)
{
	int32_t message = 0;
	uint32_t config_ack = CONFIG_ACK;

	if(recv(sock_client, &message, sizeof(message), 0) > 0) {
		
		printf("Request message: %d\n", message);

		if (message == 0) {
			if (send(sock_client, &config_ack, sizeof(config_ack), MSG_NOSIGNAL) == sizeof(config_ack)) {
				return message;
			} else {
				perror("Message ack send failed");
				return EXIT_FAILURE;
			}
		} else {
			if (send(sock_client, &message, sizeof(message), MSG_NOSIGNAL) == sizeof(message)) {
				return message;
			} else {
				perror("Message ack send failed");
				return EXIT_FAILURE;
			}
		}	
	} else { 
		perror("No message type received");
		return EXIT_FAILURE;
	}
}

uint32_t get_config(int sock_client, param_vals_t* current_config_struct, param_vals_t* fetched_config_struct, system_pointers_t *system_pointers) {
	
	//Block waiting for config struct
	 
	if(recv(sock_client, fetched_config_struct, sizeof(params_t), 0) > 0) {	
		//Print config struct from network and print individual elements according to interfaces.md, 
		//except for resets (which are not sent)
		printf("\nFetched Config: \n"
		"trigger: %d \n"
		"continuous_output: %d\n"
		"fast_mode: %d\n"
		"CH1_input_select: %d\n"
		"CH1_Feedback_mode: %d\n"
		"CH2_input_select: %d\n"
		"CH2_Feedback_mode: %d\n"
		"CBC_input_select: %d\n"
		"CBC_velocity_ext: %d\n"
		"CBC_displacement_ext: %d\n"
		"CBC_polynomial_target: %d\n"
		"param_a: %d\n"
		"param_b: %d\n"
		"param_c: %d\n"
		"param_d: %d\n"
		"param_e: %d\n"
		"param_f: %d\n"
		"param_g: %d\n"
		"param_h: %d\n"
		"param_i: %d\n"
		"param_j: %d\n"
		"param_k: %d\n"
		"param_l: %d\n"
		"param_m: %d\n"
		"param_n: %d\n\n",
		((fetched_config_struct->settings) & (1 << TRIG_BIT)) >> TRIG_BIT,
		((fetched_config_struct->settings) & (1 << CONTINUOUS_BIT)) >> CONTINUOUS_BIT,
		((fetched_config_struct->settings) & (1 << FAST_MODE_BIT)) >> FAST_MODE_BIT,
		(fetched_config_struct->CH1_settings & (1 << CH1_INPUT_MASK)) >> CH1_INPUT_MASK,
		(fetched_config_struct->CH1_settings & CH1_MODE_MASK) >> 1,
		(fetched_config_struct->CH2_settings & (1 << CH2_INPUT_MASK)) >> CH2_INPUT_MASK,
		(fetched_config_struct->CH2_settings & CH2_MODE_MASK) >> 1,
		(fetched_config_struct->CBC_settings & (1 << CBC_INPUT_MASK)) >> CBC_INPUT_MASK,
		(fetched_config_struct->CBC_settings & (1 << CBC_VEL_EXT_MASK)) >> CBC_VEL_EXT_MASK,
		(fetched_config_struct->CBC_settings & (1 << CBC_DISP_EXT_MASK)) >> CBC_DISP_EXT_MASK,
		(fetched_config_struct->CBC_settings & (1 << CBC_POLY_TARGET_MASK)) >> CBC_POLY_TARGET_MASK,
		(fetched_config_struct->param_a),
		(fetched_config_struct->param_b),
		(fetched_config_struct->param_c),
		(fetched_config_struct->param_d),
		(fetched_config_struct->param_e),
		(fetched_config_struct->param_f),
		(fetched_config_struct->param_g),
		(fetched_config_struct->param_h),
		(fetched_config_struct->param_i),
		(fetched_config_struct->param_j),
		(fetched_config_struct->param_k),
		(fetched_config_struct->param_l),
		(fetched_config_struct->param_m),
		(fetched_config_struct->param_n)
		);


		//Save to another local copy (this step was important when we were testing parameters individually, but now seems redundant, 
		//Leaving it in for now in case it protects against some unforeseen concurrency bug)
		*current_config_struct = *fetched_config_struct;
	}
}

uint32_t send_recording(int sock_client, int32_t bytes_to_send, system_pointers_t *system_pointers) {

	// Enable RAM writer and CIC divider, send "go" signal to GUI
	int position, limit, offset = 0;
	int buffer = 1; // set output buffer to 1

	limit = 32*1024;

	if (~(*(system_pointers->rx_rst) & 3)) {
		printf("Trigger on\n\n");
		
		//Send ack to GUI
		if(send(sock_client, (void *)&buffer, sizeof(buffer), MSG_DONTWAIT) < 0) {
			return -1;
		}

		//Turn on CIC compiler and RAM writer
		*(system_pointers->rx_rst) |= 3;

		//Trigger FPGA
		*(system_pointers->rx_rst) |= (1 << TRIG_BIT);
	}


	while (bytes_to_send > 0 && !interrupted) {
		
		// read ram writer position
		position = *(system_pointers->rx_cntr);

		// send 4MB if ready, otherwise sleep 0.1 ms 
		if((limit > 0 && position > limit) || (limit == 0 && position < 32*1024)) {
			offset = limit > 0 ? 0 : 4096*1024;
			limit = limit > 0 ? 0 : 32*1024;
			printf("bytes to send: %d \n", bytes_to_send);
			bytes_to_send -= send(sock_client, (system_pointers->ram) + offset, 4096*1024, MSG_NOSIGNAL);			
		} else {
			usleep(100);
		}
	}

	*(system_pointers->rx_rst) &= ~(1 << TRIG_BIT);
	printf("Trigger off \n\n");
	return 1;
}

int main () {
//// Variables declaration
	int fd; //file descriptor for memoryfile
	int sock_server; //Socket for Server
	int sock_client; //Client identefire 
	int optval=1; //Number of socket options

	volatile void *cfg, *sts; //Memory pointer
	struct sockaddr_in addr; //Server address struct

	uint32_t data_size;
	int32_t bytes_to_send, message_type;
	//int32_t config_error = -10;
	bool reset_due = false;

	// Initialise config structs - current and next
	param_vals_t fetched_config, current_config = {
		.settings = 0,
		.CH1_settings = 0,
		.CH2_settings = 0,
		.CBC_settings = 0,
		.param_a = 0,
		.param_b = 0,
		.param_c = 0,
		.param_d = 0,
		.param_e = 0,
		.param_f = 0,
		.param_g = 0,
		.param_h = 0,
		.param_i = 0,
		.param_j = 0,
		.param_k = 0,
		.param_l = 0,
		.param_m = 0,
		.param_n = 0
	};
	
//// write bitstream to FPGA
	system("cat /usr/src/v2.bit > /dev/xdevcfg ");

//// Shared memory configuration
	// Open GPIO memory section
	if((fd = open("/dev/mem", O_RDWR)) < 0)	{
		perror("open");
		return EXIT_FAILURE;
	}

	// Map Status and config addresses, close memory section once mapped
	sts = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x40000000);
	cfg = mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x40001000);
	close(fd);

	// Assign "system" pointers
	// Be aware: system_regs.rx_rst and params.settings occupy the same byte. The idea is that resets live in system_regs, and modes live in params, however the
	// two share a byte for legacy reasons.
	system_pointers_t system_regs ={.ram = 0,
									.rx_rst = (uint8_t *)(cfg + 0),
									.rx_addr = (uint32_t *)(cfg + 4),
									.rx_cntr = (uint32_t *)(sts + 12)};
	
	//Customisable parameter space
	params_t params = { .settings = (int8_t *)(cfg + 0),
						.CH1_settings = (int8_t *)(cfg + 1),
						.CH2_settings = (int8_t *)(cfg + 2),
						.CBC_settings = (int8_t *)(cfg + 3),
	     				.param_a = (int32_t *)(cfg + 8),
					    .param_b = (int32_t *)(cfg + 12),
					    .param_c = (int32_t *)(cfg + 16),
					    .param_d = (int32_t *)(cfg + 20),
					    .param_e = (int32_t *)(cfg + 24),
					    .param_f = (int32_t *)(cfg + 28),
					    .param_g = (int32_t *)(cfg + 32),
					    .param_h = (int32_t *)(cfg + 36),
					    .param_i = (int32_t *)(cfg + 40),
					    .param_j = (int32_t *)(cfg + 44),
					    .param_k = (int32_t *)(cfg + 48),
					    .param_l = (int32_t *)(cfg + 52),
					    .param_m = (int32_t *)(cfg + 56),
					    .param_n = (int32_t *)(cfg + 60)};	

	// Open contiguous data memory section
	if((fd = open("/dev/cma", O_RDWR)) < 0)	{
		perror("open");
		return EXIT_FAILURE;
	}

	// PD's code relating to contiguous data section
	data_size = 2048*sysconf(_SC_PAGESIZE);
	if(ioctl(fd, CMA_ALLOC, &data_size) < 0) {
		perror("ioctl");
		return EXIT_FAILURE;
	}
	system_regs.ram = mmap(NULL, 2048*sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	// Sets current read address at top of section
	*(system_regs.rx_addr) = data_size;

//// Socket Configuration
	//Create server socket
	if((sock_server = socket(AF_INET, SOCK_STREAM, 0)) < 0)	{
		perror("socket");
		return EXIT_FAILURE;
	}

	// Set socket options
	setsockopt(sock_server, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	// Setup listening address 
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(TCP_PORT);

	// Bind adress to socket
	if(bind(sock_server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return EXIT_FAILURE;
	}

	// Listening for incomming connections
	listen(sock_server, 1024);
	printf("Listening on port %i ...\n", TCP_PORT);

//// Main Loop
	while(!interrupted)	{
		// Reset RAM writer and filter
		*(system_regs.rx_rst) &= ~1;
		usleep(100);
		*(system_regs.rx_rst) &= ~2;

		
		// print saved channel parameters			
		printf("\nSaved Config (from shared mem): \n"
		"trigger: %d \n"
		"continuous_output: %d\n"
		"fast_mode: %d\n"
		"CH1_input_select: %d\n"
		"CH1_Feedback_mode: %d\n"
		"CH2_input_select: %d\n"
		"CH2_Feedback_mode: %d\n"
		"CBC_input_select: %d\n"
		"CBC_velocity_ext: %d\n"
		"CBC_displacement_ext: %d\n"
		"CBC_polynomial_target: %d\n"
		// "ram_address: %ld\n"
		"param_a: %d\n"
		"param_b: %d\n"
		"param_c: %d\n"
		"param_d: %d\n"
		"param_e: %d\n"
		"param_f: %d\n"
		"param_g: %d\n"
		"param_h: %d\n"
		"param_i: %d\n"
		"param_j: %d\n"
		"param_k: %d\n"
		"param_l: %d\n"
		"param_m: %d\n"
		"param_n: %d\n\n",
		(*(system_regs.rx_rst) & (1 << TRIG_BIT)) >> TRIG_BIT,
		(*(params.settings) & (1 << CONTINUOUS_BIT)) >> CONTINUOUS_BIT,
		(*(params.settings) & (1 << FAST_MODE_BIT)) >> FAST_MODE_BIT,
		(*(params.CH1_settings) & (1 << CH1_INPUT_MASK)) >> CH1_INPUT_MASK,
		(*(params.CH1_settings) & (CH1_MODE_MASK)) >> 1,
		(*(params.CH2_settings) & (1 << CH2_INPUT_MASK)) >> CH2_INPUT_MASK,
		(*(params.CH2_settings) & (CH2_MODE_MASK)) >> 1,
		(*(params.CBC_settings) & (1 << CBC_INPUT_MASK)) >> CBC_INPUT_MASK,
		(*(params.CBC_settings) & (1 << CBC_VEL_EXT_MASK)) >> CBC_VEL_EXT_MASK,
		(*(params.CBC_settings) & (1 << CBC_DISP_EXT_MASK)) >> CBC_DISP_EXT_MASK,
		(*(params.CBC_settings) & (1 << CBC_POLY_TARGET_MASK)) >> CBC_POLY_TARGET_MASK,
		// (*(system_regs.ram)),
		*(params.param_a),
		*(params.param_b),
		*(params.param_c),
		*(params.param_d),
		*(params.param_e),
		*(params.param_f),
		*(params.param_g),
		*(params.param_h),
		*(params.param_i),
		*(params.param_j),
		*(params.param_k),
		*(params.param_l),
		*(params.param_m),
		*(params.param_n)
		);

		printf("reset complete\n");
		reset_due = false;
		
		while (!reset_due && !interrupted) {
			// Await connection from GUI
			// Execution should block in this accept call until a client connects	
			if((sock_client = accept(sock_server, NULL, NULL)) < 0)	{
				perror("accept");
				return EXIT_FAILURE;
			}
			printf("sock client accepted\n");

			//Link lnterupt to signal_handler
			signal(SIGINT, signal_handler); 

			message_type = get_socket_type(sock_client);

			if (message_type == 0) {
				get_config(sock_client, &current_config, &fetched_config, &system_regs);

				bytes_to_send = 0;
				
				//Update system config parameters, preserving resets.
				*(params.settings) = (*(params.settings) & 0x03) | ((current_config.settings) & ~0x03);
                //update CH1 toggles
 				*(params.CH1_settings) = (current_config.CH1_settings);	
				//Update CH2 toggles
				*(params.CH2_settings) = (current_config.CH2_settings);	
				//UPdate CBC toggles
				*(params.CBC_settings) = (current_config.CBC_settings);
                 
                //Update numerical parameters  
		    	*(params.param_a) = current_config.param_a;
				*(params.param_b) = current_config.param_b;
				*(params.param_c) = current_config.param_c;
				*(params.param_d) = current_config.param_d;
				*(params.param_e) = current_config.param_e;
				*(params.param_f) = current_config.param_f;
				*(params.param_g) = current_config.param_g;
				*(params.param_h) = current_config.param_h;
				*(params.param_i) = current_config.param_i;
				*(params.param_j) = current_config.param_j;
				*(params.param_k) = current_config.param_k;
				*(params.param_l) = current_config.param_l;
				*(params.param_m) = current_config.param_m;
				*(params.param_n) = current_config.param_n;
				
				reset_due = true;
			}

			// Assume any other number is a number of bytes to receive
			else {
				bytes_to_send = message_type;

				if (send_recording(sock_client, bytes_to_send, &system_regs) < 1) {
					printf("send_recording error");
				}
				reset_due = true;				
			}
		}
		
		signal(SIGINT, SIG_DFL); //reset interrupt handler to default
		close(sock_client);
	}
	/* enter reset mode */
	*(system_regs.rx_rst) &= ~1;
	usleep(100);
	*(system_regs.rx_rst) &= ~2;

	close(sock_server);

	return EXIT_SUCCESS;
}
