#!/bin/bash

sleep 2; 
echo -n START; 
while true; do 
  echo -n '.'
  scrot '%Y-%m-%d-%H:%M:%S.png' &
  sleep 0.9
done;
