#!/usr/bin/env python
#a Imports
import unittest
import subprocess
import fcntl
import os
import time

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
        run_output = self.run_process(host_app, host_app_args, **kwargs) 
        rc = run_output[0]
        stdout = run_output[1]
        stderr = run_output[2]
        time_taken = run_output[3]
        if rc is None:
            self.assertEqual(rc,0, ('Timed out (expected to finish before %sseconds)'%str(time_taken)))
            pass
        self.assertEqual(rc,0, ('Expected return code of 0, got %d'%rc))
        pass
    def check_rotation(self,q,value_angle,value_axis):
        (angle, axis) = q.to_rotation(degrees=True)
        self.assertTrue(abs(angle-value_angle)<epsilon, 'Angle mismatches (%s, %s)'%(str(angle),str(value_angle)))
        self.assertEqual(len(value_axis),len(axis), 'BUG: Length of axis test value is not 3!!')
        for i in range(len(axis)):
            self.assertTrue(abs(value_axis[i]-axis[i])<epsilon, 'Coordinate %d mismatches (%s, %s)'%(i,str(axis),str(value_axis)))
            pass
        pass
    def test_small_batch(self):
        self.run_without_log("data_coprocessor_basic",["-i","1","-b","100"])
        pass
    def test_many_batches(self):
        #host_bin_dir+"data_coprocessor_basic",["-i","1000","-b","250","-L","fred.log"]
        self.run_without_log("data_coprocessor_basic",["-i","1000","-b","250"])
        return

#a Toplevel
loader = unittest.TestLoader().loadTestsFromTestCase
suites = [ loader(BasicTests),
           ]

if __name__ == '__main__':
    unittest.main()
