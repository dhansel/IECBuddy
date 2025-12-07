ifeq ($(OS),Windows_NT)
  LIB=-l setupapi
  EXT=.exe
endif

SKTool.exe: SKTool.oo commfun.oo utilfun.oo protocol.oo
	g++ SKTool.oo commfun.oo utilfun.oo protocol.oo $(LIB) -o SKTool$(EXT)

SKTool.oo: SKTool.cpp commfun.h utilfun.h
	g++ -c SKTool.cpp -o SKTool.oo

commfun.oo: commfun.cpp commfun.h ceserial.h ../protocol.h
	g++ -c commfun.cpp -o commfun.oo

utilfun.oo: utilfun.cpp utilfun.h
	g++ -c utilfun.cpp -o utilfun.oo

protocol.oo: ../protocol.cpp ../protocol.h
	g++ -c ../protocol.cpp -o protocol.oo

clean:
	rm -f *.oo *.o *~ *.exe
