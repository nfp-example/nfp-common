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
import dpkt
import struct

#a Flow
#c c_pkt
class c_pkt(object):
    def __init__(self, buf):
        self.buf = buf
        self.length = len(buf)
        self.buffer_ofs = None
        self.malloc = None
        pass
    def pkt_data(self):
        return self.buf
    def __len__(self):
        return self.length
    def set_malloc(self, m):
        self.malloc = m
        pass
    def get_malloc(self):
        return self.malloc
    pass

#c c_flow
class c_flow(object):
    #f __init__
    def __init__(self, ntuple, ts_fn=None, script=None):
        self.pkts = []
        self.ntuple = ntuple
        self.ts_fn = ts_fn
        self.script = script
        pass
    #f add_pkt
    def add_pkt(self, ts, pkt):
        self.pkts.append((ts,pkt))
        pass
    #f __len__
    def __len__(self):
        return len(self.pkts)
    def __str__(self):
        return 'flow %s'%(str(self.ntuple))
    #f pkt_alloc_memory
    def pkt_alloc_memory(self, pkt_num, malloc_fn):
        """
        Allocate memory for the 'pkt_num'th packet in the flow
        using malloc_fn callback, and give that to packet
        """
        (ts,pkt) = self.pkts[pkt_num]
        m = malloc_fn(len(pkt),pkt)
        pkt.set_malloc(m)
        pass
    #f pkt_memory
    def pkt_memory(self, pkt_num):
        """
        Return previous malloc for a packet
        """
        (ts,pkt) = self.pkts[pkt_num]
        return pkt.get_malloc()
    #f iterate
    def iterate(self, callback):
        i = 0
        for (ts,p) in self.pkts:
            if self.ts_fn is not None: ts=self.ts_fn(ts)
            callback(ts, self, i, self.script)
            i+=1
            pass
        pass
    #f Done

#c c_flow_set
class c_flow_set(object):
    #f __init__
    def __init__(self):
        self.flows = {}
        pass
    #f read_pcap
    def read_pcap(self, filename, include_tcp=True, include_udp=True, tcp_filter=None, udp_filter=None):
        in_file = file(filename, 'rb')
        pcap = dpkt.pcap.Reader(in_file)
        ts0 = None
        for ts, buf in pcap:
            ts = int(1000*1000*1000*ts)
            if ts0 is None: ts0=ts
            eth = dpkt.ethernet.Ethernet(buf)
            if eth.type==0x800:
                ip = eth.data
                ip0 = int_field(ip.src,4)
                ip1 = int_field(ip.dst,4)
                if (ip.p==6)  and (include_tcp):
                    tcp = ip.data
                    if (tcp_filter is None) or tcp_filter(eth,ip,tcp):
                        pkt = c_pkt(buf)
                        self.add_pkt_to_flow(ts-ts0, ip0, ip1, tcp.sport, tcp.dport, pkt)
                        pass
                    pass
                elif (ip.p==17)  and (include_udp):
                    udp = ip.data
                    if (udp_filter is None) or udp_filter(eth,ip,udp):
                        pkt = c_pkt(buf)
                        self.add_pkt_to_flow(ts-ts0, ip0, ip1, udp.sport, udp.dport, pkt)
                        pass
                    pass
                pass
            pass
        in_file.close()
        return
    #f add_pkt_to_flow
    def add_pkt_to_flow(self, ts, ip0, ip1, port0, port1, pkt):
        if ip0>ip1:
            return self.add_pkt_to_flow(ts, ip1, ip0, port1, port0, pkt)
        k = (ip0, ip1, port0, port1)
        if k not in self.flows:
            self.flows[k] = c_flow(k)
            pass
        self.flows[k].add_pkt(ts, pkt)
        pass
    #f display
    def display(self):
        for f in self.flows:
            print f, len(self.flows[f])
            pass
        pass
    pass

#c c_schedule
class c_schedule(object):
    #f __init__
    def __init__(self):
        self.pkts = []
        pass
    #f add_flow_pkt
    def add_flow_pkt(self, ts, flow, pkt_num, script=None):
        self.pkts.append( (ts, (flow, pkt_num, script)) )
        pass
    #f add_flow_set
    def add_flow_set(self, flows):
        """
        FIXME - use __iter__ in flow_set
        """
        for f in flows.flows:
            flow = flows.flows[f]
            flow.iterate(self.add_flow_pkt)
            pass
        pass
    #f sort
    def sort(self):
        def cmp(x,y):
            if x < y: return -1
            if x > y: return 1
            return 0
        self.pkts.sort(cmp=cmp)
        pass
    #f display
    def display(self):
        for (t,p) in self.pkts:
            (flow, pkt_num, script) = p
            print t, flow, pkt_num
            pass
        pass
    #f memory_reset
    def memory_reset(self):
        self.memory = {"small":{}, "large":{}}
        pass
    #f memory_allocate_pkt
    def memory_allocate_pkt(self, size, pkt):
        region = "large"
        if size<=192: region="small"
        self.memory[region][pkt] = (size, None)
        return (region,pkt)
    #f memory_resolve
    def memory_resolve(self):
        offset = 0
        for (r,s,o) in [("large",2048,64), ("small",256,64)]:
            for p in self.memory[r]:
                (size,a) = self.memory[r][p]
                self.memory[r][p] = (size, offset+o)
                size += o
                size = (size + s-1) &~ (s-1)
                offset += size
                pass
            pass
        pass
    #f memory_allocate
    def memory_allocate(self):
        self.memory_reset()
        for (t,p) in self.pkts:
            (flow, pkt_num, script) = p
            flow.pkt_alloc_memory(pkt_num, self.memory_allocate_pkt)
            pass
        self.memory_resolve()
        pass
    #f build_pktgen_schedule
    def build_pktgen_schedule(self):
        self.pktgen_schedule = []
        ts0 = self.pkts[0][0]
        l = len(self.pkts)
        l = (l+7) &~ 7
        for b in range(l/8):
            batch = []
            for i in range(8):
                if (b*8+i >= len(self.pkts)):
                    batch.append(None)
                    pass
                else:
                    (t, p) = self.pkts[b*8+i]
                    t -= ts0
                    (flow, pkt_num, script) = p
                    (region, pkt) = flow.pkt_memory(pkt_num)
                    batch.append( (t, self.memory[region][pkt][1], len(pkt), script, pkt) )
                    pass
                pass
            self.pktgen_schedule.append(batch)
            pass
        pass
    #f display_pktgen_schedule
    def display_pktgen_schedule(self):
        j = 0
        for b in self.pktgen_schedule:
            print "Batch %d"%j
            for i in range(8):
                batch_str = ""
                if b[i] is not None:
                    batch_str = "%20f %5d %08x %s"%(
                                  b[i][0]/(1000.0*1000*1000),b[i][2],b[i][1],str(b[i][3]))
                    pass
                print "   %d:%s"%(i,batch_str)
                pass
            j += 1
            pass
        pass
    #f make_pktgen_tarfile
    def make_pktgen_tarfile(self, filename):
        import tarfile
        import tempfile
        def add_to_tarfile(tf,f,name):
            f.seek(0)
            tarinfo = tf.gettarinfo(arcname=name,fileobj=f)
            tf.addfile(tarinfo,fileobj=f)
            pass
        memfile = tempfile.TemporaryFile()
        schedfile = tempfile.TemporaryFile()
        schedfile.seek(0)
        schedfile.write(struct.pack('<II',
                                    len(self.pkts),
                                    len(self.pktgen_schedule)))
        for i in range(len(self.pktgen_schedule)):
            b = self.pktgen_schedule[i]
            batch_data = ""
            for batch_entry in b:
                if batch_entry is None:
                    batch_data += struct.pack('<4I',0,0,0,0)
                    pass
                else:
                    batch_data += struct.pack('<IB3BIHH',
                                              batch_entry[0] & 0xffffffff,
                                              (batch_entry[0] >> 32)&0xff,
                                              0, 0, 0, # Script ofs 24
                                              (batch_entry[1] >> 8) & 0xffffffff,
                                              batch_entry[2] & 0xffff,
                                              0 )
                    memfile.seek(batch_entry[1])
                    memfile.write(batch_entry[4].pkt_data())
                    pass
                pass
            schedfile.seek(64 + i*128)
            schedfile.write(batch_data)
            pass
        tf = tarfile.open(name=filename, mode='w:gz')
        add_to_tarfile(tf,memfile,'mem')
        add_to_tarfile(tf,schedfile,'sched')
        schedfile.close()
        memfile.close()
        tf.close()
        pass
    #f Done


#a Useful functions
#f int_field
def int_field(s,n=4):
    v = 0
    for i in struct.unpack('B'*n,s):
        v = (v<<8) | i
        pass
    return v

#a Read
flows = c_flow_set()
def tcp_filter(eth,ip,tcp):
    srv_ip = 0x5bbd5821
    srv_ip = 0x5762f608
    if (tcp.sport!=80) and (tcp.dport!=80):
        return False
    ip_src = int_field(ip.src,4)
    ip_dst = int_field(ip.dst,4)
    #print "%08x,%08x"%(ip_src,ip_dst)
    if (ip_src!=srv_ip) and (ip_dst!=srv_ip): return False
    return True
flows.read_pcap('a.pcap',tcp_filter=tcp_filter,include_udp=False)
flows.display()

sched = c_schedule()
sched.add_flow_set(flows)
sched.sort()
#sched.display()
sched.memory_allocate()
sched.build_pktgen_schedule()
sched.display_pktgen_schedule()
sched.make_pktgen_tarfile('pktgen.tgz')

# Output needs to be packet data aligned to 256B boundaries+64B
# Packet/flow schedule is sets of 8 packet/flow things
#  each is time (40 bit), packet #, length, script offset
#  packet # is None if no packet (last in schedule?)
# Main work schedule is list of 
