NAME = sdlog2_dump

CC = gcc

SRCS = sdlog2_dump.c
SOURCES = $(foreach file, $(SRCS),$(wildcard $(file)))

C_SRCS   = $(filter %.c, $(SOURCES))
CPP_SRCS = $(filter %.cpp,$(SOURCES))

C_OBJS   = $(C_SRCS:%.c=%.o)
CPP_OBJS = $(CPP_SRCS:%.cpp=%.o

IFLAGS	= -I.


CFLAGS += -O -Wall
LDFLAGS += -lpthread -lm -lrt

all:$(NAME)

$(NAME):$(C_OBJS)
	$(CC) $(IFLAGS) -o $@ $(C_OBJS) $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

clean:
	@rm -vf *.o *~

