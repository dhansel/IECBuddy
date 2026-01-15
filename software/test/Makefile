sk64test.exe: sk64test.oo commfun.oo utilfun.oo drv-nl10.o drv-nl10-output.o drv-nl10-output-bmp.o drv-nl10-output-pdf.o protocol.oo
	g++ sk64test.oo commfun.oo utilfun.oo protocol.oo drv-nl10.o drv-nl10-output.o drv-nl10-output-bmp.o drv-nl10-output-pdf.o -lsetupapi -o sk64test.exe

sk64test.oo: sk64test.cpp drv-nl10.h ../SKTool/commfun.h ../SKTool/utilfun.h ../IECBuddy/protocol.h
	g++ -c sk64test.cpp -o sk64test.oo

commfun.oo: ../SKTool/commfun.cpp ../SKTool/commfun.h ceserial.h ../IECBuddy/protocol.h
	g++ -c ../SKTool/commfun.cpp -o commfun.oo

utilfun.oo: ../SKTool/utilfun.cpp ../SKTool/utilfun.h
	g++ -c ../SKTool/utilfun.cpp -o utilfun.oo

protocol.oo: ../IECBuddy/protocol.cpp ../IECBuddy/protocol.h
	g++ -c ../IECBuddy/protocol.cpp -o protocol.oo

drv-nl10.o: drv-nl10.c drv-nl10.h drv-nl10-output.h
	gcc -c drv-nl10.c -o drv-nl10.o

drv-nl10-output.o: drv-nl10-output.c
	gcc -c drv-nl10-output.c -o drv-nl10-output.o

drv-nl10-output-bmp.o: drv-nl10-output-bmp.c
	gcc -c drv-nl10-output-bmp.c -o drv-nl10-output-bmp.o

drv-nl10-output-pdf.o: drv-nl10-output-pdf.c
	gcc -c drv-nl10-output-pdf.c -o drv-nl10-output-pdf.o

clean:
	rm -f sk64test.exe *.oo *.o *~
