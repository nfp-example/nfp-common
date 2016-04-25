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
    def __init__(self, f, verbose=True):
        self.f = f
        fl = fcntl.fcntl(self.f, fcntl.F_GETFL)
        fcntl.fcntl(self.f, fcntl.F_SETFL, fl | os.O_NONBLOCK)
        self.data = ""
        self.log = []
        self.verbose = verbose
        pass
    def read_char(self):
        try:
            c = self.f.read(1)
        except IOError,exc:
            if exc.errno==11:# in ['[Errno 11] Resource temporarily unavailable']:
                return None
            pass
        return c
    def read(self):
        c = self.read_char()
        if c is None:
            return False
        self.data += c
        if c=='\n':
            line = self.data.strip()
            self.log.append(line)
            if self.verbose:
                print "1>",line
            self.data = ""
            return True
        return False
    def show_log(self):
        for l in self.log:
            print l
            pass
        pass

class BasicTests(unittest.TestCase):
    host_bin_dir = "./host/bin/"
    log_re = re.compile("\s*([0-9]+):\s*([0-9]+):([0-9a-f]+), ([0-9a-f]+), ([0-9a-f]+), ([0-9a-f]+)")
    def run_process(self, host_app, host_app_args, timeout=1000.0, verbose=False ):
        time_in = time.clock()
        host_test = subprocess.Popen(host_app_args,
                                     bufsize=1, # line buffered
                                     executable=self.host_bin_dir+host_app,
                                     stdin=None,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE,
                                     preexec_fn=None, close_fds=False, shell=False,
                                     cwd=None, env=None,
                                     universal_newlines=False, # standard newlines only for stdout/stderr
                                     startupinfo=None, creationflags=0)
        rc = None
        stdout = nonblocking_stdfile(host_test.stdout, verbose=verbose)
        stderr = nonblocking_stdfile(host_test.stderr, verbose=verbose)
        while rc is None:
            rc = host_test.poll()
            while stdout.read():
                pass
            while stderr.read():
                pass
            pass
            t = time.clock()
            if t-time_in > timeout:
               host_test.terminate()
               rc = None
               break
        pass
        while stdout.read():
            pass
        while stderr.read():
            pass
        time_out = time.clock()
        time_taken = time_out-time_in
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
        log_file = tempfile.NamedTemporaryFile()
        run_output = self.run_process(host_app, [""]+host_app_args+["-L",log_file.name], **kwargs) 
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
    def test_small_batch(self):
        self.run_without_log("data_coprocessor_basic",["-i","1","-b","100"],timeout=10.0)
        pass
    def test_many_batches(self):
        #host_bin_dir+"data_coprocessor_basic",["-i","1000","-b","250","-L","fred.log"]
        self.run_without_log("data_coprocessor_basic",["-i","1000","-b","250"],timeout=10.0)
        return
    def test_small_batch_with_log(self):
        def check_log(line, iteration,batch,data):
            self.assertEqual(data[3],batch,"Bad data for %d:%d:%s"%(iteration, batch, line))
            pass
        self.run_with_log("data_coprocessor_basic",["-i","1","-b","100"],check_log,timeout=10.0)
        pass
    def test_many_small_batches_with_log(self):
        def check_log(line, iteration,batch,data):
            self.assertEqual(data[3],batch,"Bad data for %d:%d:%s"%(iteration, batch, line))
            pass
        self.run_with_log("data_coprocessor_basic",["-i","100","-b","10"],check_log,timeout=10.0)
        pass

#a Toplevel
loader = unittest.TestLoader().loadTestsFromTestCase
suites = [ loader(BasicTests),
           ]

if __name__ == '__main__':
    unittest.main()
