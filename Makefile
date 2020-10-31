CC = gcc
CFLAGS = -g
DEPS = verify.h
SOURCES = pd.c user.c verify.c
OBJS = $(SOURCES:%.c=%.o)
TARGETS = pd user

.PHONY: all clean

all: $(TARGETS)

$(TARGETS):
	$(CC) $(CFLAGS) $^ -o $@

### pd ###
verify.o: verify.c verify.h
pd.o: pd.c verify.h
pd: verify.o pd.o

### user ###
verify.o: verify.c verify.h
user.o: user.c verify.h
user: verify.o user.o

%.o:
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)