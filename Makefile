MCU = attiny25
#MCU = attiny85

F_CPU = 8000000   	# 8 MHz

AVRDUDE_PROGRAMMER = usbtiny
#AVRDUDE_PROGRAMMER = avrispv2

AVRDUDE_PORT = usb	# programmer connected to USB

# Default target.

all: 	begin gccversion \
	white_night_code.hex \
	finished end

# Program the device
program-rgb: rgb.hex
program-rps: rock-paper-scissors.hex
program-basic: basic.hex
program-rgb: 38khz_pwm_code.hex
program-white_night_code: white_night_code.hex
program-timer_test: timer_test.hex

# this is necessary if you're burning the AVR for the first time...
# sets the proper fuse for 8MHz internal oscillator with no clk div
burn-fuse:
	$(AVRDUDE) $(AVRDUDE_FLAGS) -B 250 -u -U lfuse:w:0xa2:m -U hfuse:w:0xdf:m

# this programs the dependant hex file using our default avrdude flags
program-%:
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_FLASH)$<

FORMAT = ihex 		# create a .hex file



OPT = s			# assembly-level optimization



# Optional compiler flags.

#  -g:        generate debugging information (for GDB, or for COFF conversion)

#  -O*:       optimization level

#  -f...:     tuning, see gcc manual and avr-libc documentation

#  -Wall...:  warning level

#  -Wa,...:   tell GCC to pass this to the assembler.

#    -ahlms:  create assembler listing

CFLAGS = -g -O$(OPT) \
-funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
-Wall -Wstrict-prototypes \
-DF_CPU=$(F_CPU) \
-Wa,-adhlns=$(<:.c=.lst) \
$(patsubst %,-I%,$(EXTRAINCDIRS)) \
-mmcu=$(MCU)

# Set a "language standard" compiler flag.
CFLAGS += -std=gnu99

# Optional assembler flags.
#  -Wa,...:   tell GCC to pass this to the assembler.
#  -ahlms:    create listing
#  -gstabs:   have the assembler create line number information; note that
#             for use in COFF files, additional information about filenames
#             and function names needs to be present in the assembler source
#             files -- see avr-libc docs [FIXME: not yet described there]
ASFLAGS = -Wa,-adhlns=$(<:.S=.lst),-gstabs 





# Optional linker flags.

#  -Wl,...:   tell GCC to pass this to linker.

#  -Map:      create map file

#  --cref:    add cross reference to  map file

LDFLAGS = -Wl,-Map=$(TARGET).map,--cref





# ---------------------------------------------------------------------------

# Programming support using avrdude.

AVRDUDE = avrdude



# Programming support using avrdude. Settings and variables.



AVRDUDE_WRITE_FLASH = -U flash:w:



AVRDUDE_FLAGS = -F -p $(MCU) -P $(AVRDUDE_PORT) -c $(AVRDUDE_PROGRAMMER)



# ---------------------------------------------------------------------------



# Define directories, if needed.

DIRAVR = c:/progra~1/winavr

DIRAVRBIN = $(DIRAVR)/bin

DIRAVRUTILS = $(DIRAVR)/utils/bin

DIRINC = .

DIRLIB = $(DIRAVR)/avr/lib





# Define programs and commands.

SHELL = sh



CC = avr-gcc



OBJCOPY = avr-objcopy

OBJDUMP = avr-objdump

SIZE = avr-size



REMOVE = rm -f

COPY = cp



# Define Messages

# English

MSG_ERRORS_NONE = Errors: none

MSG_BEGIN = -------- begin --------

MSG_END = --------  end  --------

MSG_SIZE_BEFORE = Size before: 

MSG_SIZE_AFTER = Size after:

MSG_FLASH = Creating load file for Flash:

MSG_EXTENDED_LISTING = Creating Extended Listing:

MSG_SYMBOL_TABLE = Creating Symbol Table:

MSG_LINKING = Linking:

MSG_COMPILING = Compiling:

MSG_ASSEMBLING = Assembling:

MSG_CLEANING = Cleaning project:





# Define all object files.

OBJ = $(SRC:.c=.o) $(ASRC:.S=.o) 



# Define all listing files.

LST = $(ASRC:.S=.lst) $(SRC:.c=.lst)



# Combine all necessary flags and optional flags.

# Add target processor to flags.

ALL_CFLAGS = -I. $(CFLAGS)

ALL_ASFLAGS = -mmcu=$(MCU) -I. -x assembler-with-cpp $(ASFLAGS)









# Eye candy.

# AVR Studio 3.x does not check make's exit code but relies on

# the following magic strings to be generated by the compile job.

begin:

	@echo

	@echo $(MSG_BEGIN)



finished:

	@echo $(MSG_ERRORS_NONE)



end:

	@echo $(MSG_END)

	@echo







# Display compiler version information.

gccversion : 

	@$(CC) --version





# Create final output files (.hex) from ELF output file.

%.hex: %.elf

	@echo

	@echo $(MSG_FLASH) $@

	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@



# Link: create ELF output file from object files.

.SECONDARY : $(TARGET).elf

.PRECIOUS : $(OBJ)

%.elf: %.o

	@echo

	@echo $(MSG_LINKING) $@

	$(CC) $(ALL_CFLAGS) $< --output $@ $(LDFLAGS)





# Compile: create object files from C source files.

%.o : %.c

	@echo

	@echo $(MSG_COMPILING) $<

	$(CC) -c $(ALL_CFLAGS) $< -o $@





# Compile: create assembler files from C source files.

%.s : %.c

	$(CC) -S $(ALL_CFLAGS) $< -o $@





# Assemble: create object files from assembler source files.

%.o : %.S

	@echo

	@echo $(MSG_ASSEMBLING) $<

	$(CC) -c $(ALL_ASFLAGS) $< -o $@











# Target: clean project.

clean: begin clean_list finished end



clean_list :

	@echo

	@echo $(MSG_CLEANING)

	$(REMOVE) *.hex

	$(REMOVE) *.lst

	$(REMOVE) *.obj

	$(REMOVE) *.elf

	$(REMOVE) *.o



# Listing of phony targets.

.PHONY : all begin finish end \

	clean clean_list program

