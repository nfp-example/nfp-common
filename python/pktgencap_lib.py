#!/usr/bin/env python
# To get the pcap python library, pip install pypcap
#import pcap
#
#x=pcap.pcap('a.pcap')
#def pcap_callback( timestamp, pkt_buffer ):
#    print timestamp, pkt_buffer
#
#x.loop(cnt=0,callback=pcap_callback)

#a Imports
import pktgen_lib
import ctypes

class c_pktgencap(object):
    def __init__(self):
        self.blah = ctypes.load_library("")
        pass
    def connect(self):
        self.blah.nfp_init()
        self.blah.start_client()
        pass
    def disconnect(self):
        pass

pktgencap = c_pktgencap()
pktgencap.connect()
pktgencap.load_pkts()
pktgencap.start_tx()
pktgencap.poll_complete()
pktgencap.disconnect()
