
INSTALL = install
PREFIX  = /usr/local/bin

po2lmo: po2lmo.o template_lmo.o
	$(CC) $(LDFLAGS) -o po2lmo po2lmo.o template_lmo.o

install:
	$(INSTALL) -m 755 po2lmo $(PREFIX)

clean:
	$(RM) po2lmo *.o
