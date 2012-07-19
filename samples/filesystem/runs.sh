svn info
uname -a

for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 161 17 18 19 20 21 21 23 24 25 26 27 28 29 30
do
	/usr/bin/time -v ./filesystem 1024 1048576
	echo					
done
