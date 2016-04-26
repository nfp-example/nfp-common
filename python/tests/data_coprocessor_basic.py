#!/usr/bin/env python
#a Imports
import unittest
import subprocess
import fcntl
import os
import time
import tempfile
import re

#a Test
#c Basic tests
class nonblocking_stdfile(object):
    def __init__(self, f, line_callback=None):
        self.f = f
        fl = fcntl.fcntl(self.f, fcntl.F_GETFL)
        fcntl.fcntl(self.f, fcntl.F_SETFL, fl | os.O_NONBLOCK)
        self.data = ""
        self.log = []
        self.line_callback = line_callback
        pass
    def read_char(self):
        try:
            c = self.f.read(1)
        except IOError,exc:
            if exc.errno==11:# in ['[Errno 11] Resource temporarily unavailable']:
                return None
            raise
            pass
        if c=="":
            return None
        return c
    def read(self):
        c = self.read_char()
        if c is None:
            return False
        self.data += c
        if c=='\n':
            line = self.data.strip()
            self.log.append(line)
            if self.line_callback:
                self.line_callback(line)
                pass
            self.data = ""
            return True
        return True
    def flush_read(self):
        fl = fcntl.fcntl(self.f, fcntl.F_GETFL)
        fcntl.fcntl(self.f, fcntl.F_SETFL, fl &~ os.O_NONBLOCK)
        while self.read():
            pass
        pass
    def show_log(self):
        for l in self.log:
            print l
            pass
        pass

class TestBase(unittest.TestCase):
    host_bin_dir = "./host/bin/"
    log_re = re.compile("\s*([0-9]+):\s*([0-9]+):([0-9a-f]+), ([0-9a-f]+), ([0-9a-f]+), ([0-9a-f]+)")
    nffw_re = re.compile("nfp6000_nffw.c:")
    def run_process(self, host_app, host_app_args, timeout=1000.0, verbose=None ):
        if verbose is None:
            verbose = 0
            if "VERBOSE" in os.environ and os.environ["VERBOSE"]!="":
                verbose = int(os.environ["VERBOSE"])
                pass
            pass
                
        time_in = time.clock()
        host_test = subprocess.Popen(host_app_args,
                                     bufsize=1, # line buffered
                                     executable=self.host_bin_dir+host_app,
                                     stdin=None,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE,
                                     preexec_fn=None,
                                     close_fds=False,
                                     shell=False,
                                     cwd=None,
                                     env=None,
                                     universal_newlines=True, # Make stdout/stderr text streams
                                     startupinfo=None, creationflags=0)
        rc = None
        def line_callback(line, verbose=verbose):
            is_nffw = self.nffw_re.search(line)
            if (is_nffw is None) or (verbose>4):
                print line
                pass
            pass
        stdout = nonblocking_stdfile(host_test.stdout, line_callback=line_callback)
        stderr = nonblocking_stdfile(host_test.stderr, line_callback=line_callback)
        while rc is None:
            rc = host_test.poll()
            while stdout.read():
                pass
            while stderr.read():
                pass
            pass
            t = time.clock()
            if t-time_in > timeout:
                print >> sys.stderr,"Timeout"
                host_test.terminate()
                rc = None
                break
        pass
        time.sleep(1)
        stdout.flush_read()
        stderr.flush_read()
        time_out = time.clock()
        time_taken = time_out-time_in
        print "Test completed with rc %d in time %s"%(rc,str(time_taken))
        return (rc, stdout, stderr, time_taken)
    def run_without_log(self, host_app, host_app_args, **kwargs):
        run_output = self.run_process(host_app, [""]+host_app_args, **kwargs) 
        rc = run_output[0]
        stdout = run_output[1]
        stderr = run_output[2]
        time_taken = run_output[3]
        if rc is None:
            self.assertEqual(rc,0, ('Timed out (expected to finish before %sseconds)'%str(time_taken)))
            pass
        self.assertEqual(rc,0, ('Expected return code of 0, got %d'%rc))
        pass
    def run_with_log(self, host_app, host_app_args, log_check_callback, **kwargs):
        if "LOGFILE" in os.environ and os.environ["LOGFILE"]!="":
            log_filename = os.environ["LOGFILE"]
            log_file = open(log_filename,"rw")
            pass
        else:
            log_file = tempfile.NamedTemporaryFile()
            log_filename = log_file.name
            pass
        run_output = self.run_process(host_app, [""]+host_app_args+["-L",log_filename], **kwargs) 
        rc = run_output[0]
        stdout = run_output[1]
        stderr = run_output[2]
        time_taken = run_output[3]
        if rc is None:
            self.assertEqual(rc,0, ('Timed out (expected to finish before %sseconds)'%str(time_taken)))
            pass
        self.assertEqual(rc,0, ('Expected return code of 0, got %d'%rc))
        log_file.seek(0)
        for l in log_file:
            m = self.log_re.match(l)
            self.assertNotEqual(m,None,"Log file line incorrect format %s"%l)
            if m is not None:
                iteration = int(m.group(1))
                batch = int(m.group(2))
                data = (int(m.group(3),16),
                        int(m.group(4),16),
                        int(m.group(5),16),
                        int(m.group(6),16))
                log_check_callback(l,iteration,batch,data)
                pass
            pass
        pass

class NullTests(TestBase):
    def test_null_small_batch(self):
        self.run_without_log("data_coprocessor_basic",["-i","1","-b","100"],timeout=10.0)
        pass
    def test_null_small_batch_specify_firmware(self):
        self.run_without_log("data_coprocessor_basic",["-i","1","-b","100","--firmware","firmware/nffw/data_coproc_null_one.nffw"],timeout=10.0)
        pass
    def test_null_many_batches_with_many_firmware(self):
        self.run_without_log("data_coprocessor_basic",["-i","1000","-b","250","--firmware","firmware/nffw/data_coproc_null_many.nffw"],timeout=10.0)
        return
    def test_null_many_batches(self):
        #host_bin_dir+"data_coprocessor_basic",["-i","1000","-b","250","-L","fred.log"]
        self.run_without_log("data_coprocessor_basic",["-i","1000","-b","250"],timeout=10.0)
        return
    def test_null_small_batch_with_log(self):
        def check_log(line, iteration,batch,data):
            self.assertEqual(data[3],batch,"Bad data for %d:%d:%s"%(iteration, batch, line))
            pass
        self.run_with_log("data_coprocessor_basic",["-i","1","-b","100"],check_log,timeout=10.0)
        pass
    def test_null_many_small_batches_with_log(self):
        def check_log(line, iteration,batch,data):
            self.assertEqual(data[3],batch,"Bad data for %d:%d:%s"%(iteration, batch, line))
            pass
        self.run_with_log("data_coprocessor_basic",["-i","100","-b","10"],check_log,timeout=10.0)
        pass

class FetchSumTests(TestBase):
    def fetch_sum_n(self, n, args):
        def check_log(line, iteration,batch,data):
            self.assertEqual(data[3],(n*(n-1)/2)&0xff,"Bad data for %d:%d:%s"%(iteration, batch, line))
            pass
        self.run_with_log("data_coprocessor_basic",args,check_log,timeout=30.0)
        pass
    def test_fetch_sum_small_96(self):
        self.fetch_sum_n(96,args=["-i","1","-b","100","-S","96","--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_small_97(self):
        self.fetch_sum_n(97,args=["-i","1","-b","100","-S","97","--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_small_75(self):
        self.fetch_sum_n(75,args=["-i","1","-b","100","-S","75","--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_small_offset_1_to_15(self):
        for i in range(15):
            self.fetch_sum_n(256+i,args=["-i","1","-b","100","-S","%d"%(256+i),"--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
            pass
        pass
    def test_fetch_sum_small_1k(self):
        self.fetch_sum_n(1024,args=["-i","1","-b","100","-S","1024","--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_small_1k1(self):
        self.fetch_sum_n(1025,args=["-i","1","-b","100","-S","1025","--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_small_1k1_many(self):
        self.fetch_sum_n(1025,args=["-i","1","-b","10","-S","1025","--firmware","firmware/nffw/data_coproc_fetch_sum_many.nffw"])
        pass
    def test_fetch_sum_one_1M(self):
        self.fetch_sum_n(1024*1024,args=["-i","1","-b","1","-S","%d"%(1024*1024),"--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_small_1M(self):
        self.fetch_sum_n(1024*1024,args=["-i","1","-b","100","-S","%d"%(1024*1024),"--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def xtest_fetch_sum_small_1M(self):
        self.fetch_sum_n(1024*1024,args=["-i","1","-b","100","-S","%d"%(1024*1024),"--firmware","firmware/nffw/data_coproc_fetch_sum_one.nffw"])
        pass
    def test_fetch_sum_many_1M(self):
        self.fetch_sum_n(1024*1024,args=["-i","1","-b","10","-S","%d"%(1024*1024),"--firmware","firmware/nffw/data_coproc_fetch_sum_many.nffw"])
        pass

#a Toplevel
def prune(test_class):
    if "TEST_RE" in os.environ:
        test_re = re.compile(os.environ["TEST_RE"])
        attrs = test_class.__dict__.keys()
        for a in attrs:
            if a[:5]=="test_":
                m = test_re.search(a)
                if m is None:
                    delattr(test_class,a)
                    pass
                pass
            pass
        pass
    pass
    
suite = unittest.TestSuite()
for s in [ NullTests,
           FetchSumTests,
           ]:
    prune(s)
    suite.addTest(unittest.TestLoader().loadTestsFromTestCase(s))
    pass

if __name__ == '__main__':
    unittest.TextTestRunner(verbosity=2).run(suite)
