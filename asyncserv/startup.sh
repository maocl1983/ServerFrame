kill -TERM `ps -ef | grep "AsynServ_Dev ./bench.conf" | grep -v grep | awk '{print $2}'`


if [ $# = 1 -a "$1" = "123" ]
then
	echo "del log"
	rm -f log/*
fi

sleep 1
./AsynServ_Dev ./bench.conf

if $1="123"
