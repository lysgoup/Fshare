TEST = -DMAIN

ifdef thread
	LIBS += -lpthread
endif

ifdef main
	TEST =
endif

ifdef debug
	DBUG = -DDEBUG
endif

%: %.c
	gcc -o $@ $^ $(LIBS) $(TEST) $(DBUG)

clean:
	rm fshare fshared