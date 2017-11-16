#/usr/bin/python

import os
import time

server_host = '10.0.0.7'
#server_host = '192.168.1.57'
#try_num = 2
try_num = 3
#thread_list = [1,2,4,8]
thread_list = [2]
#thread_list = [4,8]
batch_size_list = [1,8,16,32,64,128,256]
#transfer_size_list = [16,32,64,128,256,512,1024,2048]
transfer_size_list = [16,32,64,128,256,512,1024]
for i in range(try_num):
    for th_num in thread_list:
        for tr_size in transfer_size_list:
            for bt_size in batch_size_list:
                cmd = './test_RDMA_pthread '
                cmd = cmd + "open " + server_host + " pool.set " + str(th_num) + " " + str(tr_size) + " " + str(bt_size) + " >> /tmp/thkim_result/thkim_result_"+str(th_num)+".txt"
                print(cmd)
                os.system(cmd)
#                time.sleep(10)





