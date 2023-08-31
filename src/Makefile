all: wsn

wsn: init.c node.c base.c
	mpicc init.c node.c base.c -o wsn

run-small:
	mpirun -np 5 --oversubscribe wsn 2 2

run-med:
	mpirun -np 13 --oversubscribe wsn 3 4

run-large:
	mpirun -np 26 --oversubscribe wsn 5 5

clean:
	rm *.txt wsn

