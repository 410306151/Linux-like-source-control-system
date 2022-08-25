ALL: AOS_HW2
AOS_HW2:
	gcc server-MultiProcess.c -o server-MultiProcess
	gcc server-MultiThread.c -pthread -o server-MultiThread
	gcc client.c -o client