#! /usr/bin/python

import os
import sys


pfile="camio_perf_server.perf"

points = { 
    "1":"get_new_buffers", 
    "2":"-->camio_chan_wr_buff_req()" }

get_new_normal_start = 0;
get_new_normal_end   = 0;
thing = 0

i = 0
last = -1
start = 0
for line in open(pfile):
    if i < 100 * 1000:
        i+=1
        continue
    if i > 110 * 1000:
        break
    (mode,id,cond,ts,data) = line[:-1].split(",")

#    if(mode == "A" and id=="4" and cond =="0"):
#        start = int(ts)
#        last = 0
#        print("\n#%s%5s [%3s,%3s] - %3.3f ++%3.3f" % (mode,id,cond,data,start,start-thing) )  
#        thing = start
#    
#    #if(mode == "O" and (id=="100")):
#    elif(last >= 0):
#    if mode == "A" and id=="4" and cond =="0":
    if 1:
        if(start == 0):
            start=int(ts)
        cycles  = int(ts) - start
        time    = cycles / 3.1
        delta = time - last
        last = time
        print "$%s%5s [%3s,%3s] - %5.2f +%4.2f" % (mode,id,cond,data,time,delta), 
        if(delta > 900):
            print "<---"
        else:
            print
        #print("%3.3f (%s,%s (OID=%s))" % (time,cond,data,id))   
        #print("%3.3f" % (time))   
                  
        i += 1
#    else:       
#        print("DROPPED $%s%5s [%3s,%3s]" % (mode,id,cond,data))   
            
