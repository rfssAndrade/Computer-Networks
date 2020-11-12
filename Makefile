CC = gcc
CFLAGS = -g
DEPS = verify.h
SOURCES = pd.c user.c verify.c AS.c sockinfo.c
OBJS = $(SOURCES:%.c=%.o)
TARGETS = pd user AS

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
message.o: message.c message.h verify.h
user.o: user.c verify.h message.h
user: verify.o message.o user.o


### AS ###
verify.o: verify.c verify.h
sockinfo.o: sockinfo.c sockinfo.h
AS.o: AS.c verify.h sockinfo.h
AS: verify.o sockinfo.o AS.o


%.o:
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)