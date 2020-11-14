CC = gcc
CFLAGS = -g
DEPS = verify.h
SOURCES = pd.c user.c verify.c AS.c userinfo.c message.c FS.c
OBJS = $(SOURCES:%.c=%.o)
TARGETS = pd user AS FS

.PHONY: all clean

all: $(TARGETS)

$(TARGETS):
	$(CC) $(CFLAGS) $^ -o $@

### general ###
verify.o: verify.c verify.h
message.o: message.c message.h verify.h
userinfo.o: userinfo.c userinfo.h

### pd ###
pd.o: pd.c verify.h pd.h
pd: verify.o pd.o

### user ###
user.o: user.c verify.h message.h user.h
user: verify.o message.o user.o


### AS ###
AS.o: AS.c verify.h userinfo.h AS.h
AS: verify.o userinfo.o message.o AS.o

### FS ###
FS.o: FS.c verify.h userinfo.h message.h FS.h
FS: verify.o message.o userinfo.o FS.o


%.o:
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)