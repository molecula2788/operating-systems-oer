#!/bin/bash

MYSQL_ROOT_PASSWORD=eiv2Siezofe7quahcido
MYSQL_SO_CLOUD_PASSWORD=iK3ahthae3ieZ6gohkay

echo 'Setting up db'

sudo rm -rf db-data

docker run --rm -v $PWD/db-data:/var/lib/mysql \
       -e MYSQL_ROOT_PASSWORD=$MYSQL_ROOT_PASSWORD \
       -e MYSQL_USER=so-cloud \
       -e MYSQL_PASSWORD=$MYSQL_SO_CLOUD_PASSWORD \
       -e MYSQL_DATABASE=so-cloud \
       -e MYSQL_HOST=db \
       mariadb:10.7 &> log &

pid=$!

while true; do
    n=$(grep 'mariadb.org binary distribution' log | wc -l)

    if [ $n = 2 ]; then
	break
    fi

    sleep 0.5
done

kill -QUIT $pid
wait $pid

echo 'Creating tables'

docker run --name mysql-tmp --rm -v $PWD/db-data:/var/lib/mysql mariadb:10.7 &> log &
pid=$!

while true; do
    n=$(grep 'mariadb.org binary distribution' log | wc -l)

    if [ $n = 1 ]; then
	break
    fi

    sleep 0.5
done

docker exec -i mysql-tmp mysql -u so-cloud -p$MYSQL_SO_CLOUD_PASSWORD so-cloud < db.sql

docker exec -i mysql-tmp mysql -u so-cloud -p$MYSQL_SO_CLOUD_PASSWORD so-cloud -e "INSERT INTO network(name, bridge_interface_idx, ip, mask) values('default', 0, 3232235520, 4294901760)"

kill -QUIT $pid
wait $pid

rm -f log
