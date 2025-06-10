CFLAGS  = -std=c99

.PHONY: hsh
hsh: hsh.o
	@./hsh.o

hsh.o: *.c 
	@echo Compiling $@
	@$(CC) $(CFLAGS) *.c -o hsh.o
