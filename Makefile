#######################################################################################
## Main makefile project configuration
## PROJECT = <name of the target to be built>
## MCU_CC_FLAGS = <one of the CC_FLAGS above>
## MCU_LIB_PATH = <one of the LIB_PATH above>
## OPTIMIZE_FOR = < SIZE or nothing >
## DEBUG_LEVEL = < -g compiler option or nothing >
## OPTIM_LEVEL = < -O compiler option or nothing >
#######################################################################################
TARGET    =   websocket_listener
OBJDIR    =   output

SRC_DIR   =   src \
                    
		  
INCLUDE_DIR = inc \

PROJECT_INC_LIB = $(addprefix -I, $(INCLUDE_DIR))



FILES     =   $(shell find $(SRC_DIR) \( -name \*.c \) -printf '%T@\t%p\n' | sort -k 1nr | cut -f2-)
SRC_FILES =   $(FILES:./%=%)
OB_FILES  = $(SRC_FILES:.c=.o)

OBJS_FILES =  $(addprefix $(OBJDIR)/, $(OB_FILES:./%=%))
OBJ			= $(OBJS_FILES) 

OBJDIRCOM =   $(addprefix $(OBJDIR)/, $(SRC_DIR))
SRCDIRCOM =   $(SRC_DIR)


GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags)
CFLAGS    =   -DAPP_VERSION=\"$(GIT_VERSION)\"  
LDFLAGS   =   -lpthread -lm -lwebsockets

###############################################################################
# Makefile execution
###############################################################################
.PHONY: all clean

all: $(OBJDIRCOM) $(TARGET)

$(OBJDIRCOM):
	mkdir -p $(OBJDIRCOM)
	cd $(OBJDIR)
	@echo $(OBJS_FILES)

$(OBJDIR)/%.o: %.c
#$(CC) $(CFLAGS) $(LDFLAGS) -I$(INCLUDE_DIR) -O0 -g -Wall -c -pedantic -ansi $< -o $@ # Insane checking 
	$(CC) $(CFLAGS) $(LDFLAGS) $(PROJECT_INC_LIB) -O0 -g -Wall -std=gnu99 -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) -o $(OBJDIR)/$@ $^ $(LDFLAGS) 

clean:
	@rm -f $(TARGET) $(wildcard *.o)
	@rm -rf $(OBJDIR)	
