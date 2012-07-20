svn info
uname -a
export QT_STACK_SIZE=1048576
for i in 1 2 3 4 5 6 7 8 9 10
do
	/usr/bin/time -v ./matrixinvert 2048
	echo					
done
