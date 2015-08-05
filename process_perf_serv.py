#! /usr/bin/python

import os
import sys


pfile="camio_perf_server.perf"

points = { 
    "1":"get_new_buffers", 
    "2":"-->camio_chan_wr_buff_req()" }

get_new_normal_start = 0;
get_new_normal_end   = 0;

i = 0
for line in open(pfile):
    if i < 1:
        i += 1
        continue
    if i > 100 * 1000:
        break;
    (mode,id,cond,ts,data) = line[:-1].split(",")

    if(mode == "A" and id=="30"):
        get_new_buffers_start = ts
    
    else: #if(mode == "O" and id=="40"):
        cycles  = int(ts) - int(get_new_buffers_start)
        time    = cycles / 3.1
        print("%s,%s %3.3f (%s)" % (mode, id, time,data))   
        #print("%3.3f" % (time))   
         
         
        i += 1   
