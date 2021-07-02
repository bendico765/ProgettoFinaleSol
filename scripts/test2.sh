#!/bin/bash

# Parametri:
# $1: percorso dell'eseguibile server
# $2: percorso del file di configurazione 
# $3: percorso al file con i casi di test

# partenza server
$1 $2 &
SERVER_PID=$!

# attesa caricamento server
sleep 5

# lettura ed esecuzione dei casi di test
cat $3 | while read line
do
	$line
done

sleep 10

echo "Terminazione server"
kill -1 $SERVER_PID
