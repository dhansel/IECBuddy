sk64test.exe: sk64test.oo drv-nl10.o drv-nl10-output.o drv-nl10-output-bmp.o drv-nl10-output-pdf.o
	g++ sk64test.oo drv-nl10.o drv-nl10-output.o drv-nl10-output-bmp.o drv-nl10-output-pdf.o -o sk64test.exe

sk64test.oo: sk64test.cpp ../protocol.h ceserial.h drv-nl10.h
	g++ -c sk64test.cpp -o sk64test.oo

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
