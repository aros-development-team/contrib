EXE = gljuggler

CFLAGS = -O2 -Wall $(shell sdl-config --cflags)
LDFLAGS = $(shell sdl-config --libs) -lGL -lm

OBJS	= $(EXE).o

$(EXE):	$(OBJS)

$(EXE).o: $(EXE).c

clean:
	rm -f $(EXE) *.o

test: $(EXE)
	./$(EXE) 1024x768 #1200x1200


WORK = /ram
VER = 1.0.0
DDIR = gljuggler-$(VER)

package:  clean
	rm -rf $(WORK)/$(DDIR)
	mkdir $(WORK)/$(DDIR)
	cp *.c Makefile* $(WORK)/$(DDIR)
	#cp *.h *.m $(WORK)/$(DDIR)
	#cp -a data $(WORK)/$(DDIR)
	cp ChangeLog README $(WORK)/$(DDIR)
	cd $(WORK) && tar czf $(DDIR).tgz $(DDIR)
