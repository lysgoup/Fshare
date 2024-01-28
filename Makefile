ifdef thread
	LIBS += -lpthread
endif

ifdef main
	TEST = -DMAIN
endif

ifdef debug
	DBUG = -DDEBUG
endif

%: %.c
	gcc -o $@ $^ $(LIBS) $(TEST) $(DBUG)

clean:
	rm client server