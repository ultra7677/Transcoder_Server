#!/bin/bash
#g++ -std=c++11 -g transcoder.cpp decoding.cpp gop.cpp encoding.cpp core_binding.cpp -o main -L /usr/local/lib -lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil -lm -lpthread -lx264
g++ -std=c++11 -g transcoder.cpp decoding.cpp gop.cpp encoding.cpp core_binding.cpp init.cpp server.cpp -o main -l json-c -L /usr/local/lib -lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil -lm -lpthread -lx264 2>&1
