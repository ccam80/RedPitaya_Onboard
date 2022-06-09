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

Bit      | Byte Offset | Signal                          | Substructure
-------- | ----------- | ------------------------------- | ------
[7:0]    | 0           | "reset"                         | [0:0]=cma_memory_reset
&nbsp;   |             |                                 | [1:1]=ram_writer_reset
&nbsp;   |             |                                 | [2:2]=Feedback_trigger
&nbsp;   |             |                                 | [7:6]=Feedback_mode)   
[31:16]  | 2           | sample rate (CIC Filter Config --> f=125MHz/value) |
[63:32]  | 4           | RAM_adress --> set address of writer |
[159:64] | 8           | Feedback_config_bus             | [95:64] (Offset 8)=fixed_phase
&nbsp;   |             |                                 | [95:64] (Offset 8)=start_freqency
&nbsp;   |             |                                 | [127:96] (Offset 12)=stop_freqency
&nbsp;   |             |                                 | [127:96] (Offset 12)=a_const-->MULT_CONST
&nbsp;   |             |                                 | [159:128] (Offset 16)=interval
&nbsp;   |             |                                 | [159:144] (Offset 18)=b_const-->ADD_CONST

Duplicated bit meaning in Feedback_config_bus are filtered by mode selection

## ram
Located in cma file with memory offset of 0x00000000 and range of 512M


