CC = gcc
CFLAGS = -g
DEPS = verify.h
OBJ = pd.o user.o

.PHONY: all clean

%.o:
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	@echo Cleaning...
	rm -f $(OBJS) $(TARGETS)