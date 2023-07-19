# FPGA
The shered memory consisting of two files (mem and cma) both located in the dev folder.
To get it working like this the SD card image of Parvel Demin is nessecery, it can be installed to the RedPitaya according the description in the LED blinker tutorial:
http://pavel-demin.github.io/red-pitaya-notes/led-blinker/

## sts
Located in mem file with memory offset of 0x40000000 and range of 4k

Bit      | Offset | Signal   
-------- | ------ | -------- 
[31:0]   | 0      | reserved for reset   
[95:32]  | 4      | DNA --> device ID
[127:96] | 12     | writer position

## cfg
Located in mem file with memory offset of 0x40001000 and range of 4k

Note: Channel 1/Channel 2 refer to the output/DAC channels of the RedPitaya, input channels are selected by the input_select bit

Bit      | Byte Offset | Signal                          | Substructure
-------- | ----------- | ------------------------------- | ------
[7:0]    | 0           | "reset"                         | [0:0]=cma_memory_reset
&nbsp;   |             |                                 | [1:1]=ram_writer_reset
&nbsp;   |             |                                 | [2:2]=Feedback_trigger
&nbsp;   |             |                                 | [3:3]=continuous_output
&nbsp;   |             |                                 | [4:4]=fast_mode
[15:8]   | 1           | "Channel 1 settings"            | [8:8]  = CH1 input_select
&nbsp;   |             |                                 | [12:9] = CH1 Feedback_mode
[23:16]  | 2           | "Channel 2 settings"            | [16:16]  = CH2 input_select
&nbsp;   |             |                                 | [20:17] = CH2 Feedback_mode
[31:24]  | 3           | "CBC settings"                  | [24:24]  = input select: (1  (disp ADC1, vel ADC2))
&nbsp;   |             |                                 | [25:25]  = velocity (1 external, 0 differentiated)
&nbsp;   |             |                                 | [26:26]  = displacement (1 external, 0 integrated)
&nbsp;   |             |                                 | [27:27]  = polynomial target: (0 displacement, 1 velocity)
[63:32]  | 4           | RAM_adress						 | [63:32] = address of ram writer
[511:64] | 8           | Feedback_config_bus             | [95:64] (Offset 8)=param_a/ fixed_phase/ start_freqency / CBC RHAT_START
&nbsp;   |             |                                 | [127:96] (Offset 12)=param_b/ amplitude/ stop_freqency / CBC RHAT_INTERVAL
&nbsp;   |             |                                 | [159:128] (Offset 16)=param_c/ amplitude / CBC FREQ_START
&nbsp;   |             |                                 | [191:160] (Offset 20)=param_d / CBC  FREQ_INTERVAL
&nbsp;   |             |                                 | [223:192] (Offset 24)=param_e / CBC KP
&nbsp;   |             |                                 | [255:224] (Offset 28)=param_f / CBC KD
&nbsp;   |             |                                 | [287:256] (Offset 32)=param_g / CH2  param_a / CBC A_START
&nbsp;   |             |                                 | [319:288] (Offset 36)=param_h / CH2 param_b / CBC_A_INTERVAL
&nbsp;   |             |                                 | [351:320] (Offset 40)=param_i / CH2 param_c / CBC_B_START
&nbsp;   |             |                                 | [383:352] (Offset 44)=param_j / CH2 param_d / CBC_B_INTERVAL
&nbsp;   |             |                                 | [415:384] (Offset 48)=param_k / CH2 param_e / CBC_C_START
&nbsp;   |             |                                 | [447:416] (Offset 52)=param_l / CH2 param_f / CBC_C_INTERVAL
&nbsp;   |             |                                 | [479:448] (Offset 56)=param_m / CBC_D_START
&nbsp;   |             |                                 | [511:480] (Offset 60)=param_n / CBC_D_INTERVAL


## ram
Located in cma file with memory offset of 0x00000000 and range of 512M


