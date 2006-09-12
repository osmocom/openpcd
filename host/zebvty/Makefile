#include ../../makevars

OBJS=vector.o command.o buffer.o vty.o
CFLAGS+=-fPIC
NAME=zebvty

all: lib$(NAME).a lib$(NAME).so

lib$(NAME).a: $(OBJS)
	$(AR) r $@ $^

lib$(NAME).so: $(OBJS)
	$(LD) -x --shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $^

clean:
	@rm -f *.o lib$(NAME).a lib$(NAME).so
