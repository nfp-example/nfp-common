import ctypes

class a(object):
 def __init__(self,device):
    if device:
        nfp_id = int(device)
    else:
        nfp_id = 0
    self.nfp_id = nfp_id

    # minimal ctypes set up to libnfp
    self.libnfp = ctypes.cdll.LoadLibrary("libnfp.so")
    self.libnfp.rettype = ctypes.c_void_p
    self.nfp = self.libnfp.nfp_cpp_from_device_id(nfp_id)
    
    self.libnfp.nfp_xpb_readl.argtypes = [ctypes.c_void_p,
                                          ctypes.c_uint, ctypes.c_void_p]
    self.libnfp.nfp_xpb_writel.argtypes = [ctypes.c_void_p,
                                           ctypes.c_uint, ctypes.c_uint]
    self.libnfp.nfp_cpp_readl.argtypes = [ctypes.c_void_p,
                                          ctypes.c_uint, ctypes.c_ulonglong,
                                          ctypes.c_void_p]
    self.libnfp.nfp_cpp_writel.argtypes = [ctypes.c_void_p, ctypes.c_uint,
                                           ctypes.c_ulonglong, ctypes.c_int]


val = ctypes.c_uint()
b = a(0)
c = a(0)
print b.libnfp.nfp_xpb_readl(b.nfp, 0x040f0004,ctypes.byref(val))
print b.libnfp.nfp_xpb_writel(b.nfp, 0x040f0000, 0)
print "%08x"%val.value
print c.libnfp.nfp_xpb_readl(b.nfp, 0x040f0004,ctypes.byref(val))
print "%08x"%val.value
#b.libnfp.nfp_cpp_writel(b.nfp, 0x07000101, 0x8100000000, 0)
#c.libnfp.nfp_cpp_writel(b.nfp, 0x07000101, 0x8100000000, 1)
print "%08x"%b.nfp
#print "%08x"%c.nfp
print b.libnfp.nfp_cpp_readl(b.nfp, 0x07000000, 0x8100000000,ctypes.byref(val))
print "%08x"%val.value
#print c.libnfp.nfp_cpp_readl(c.nfp, 0x07000001, 0x8100000000, ctypes.byref(val))
print "%08x"%val.value
