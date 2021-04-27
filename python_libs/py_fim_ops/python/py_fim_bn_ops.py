import torch
import os

rocmpath = os.environ['ROCM_PATH']
torch.ops.load_library(os.path.join(rocmpath,"lib","libpy_fim_bn.so"))
py_fim_bn = torch.ops.custom_ops.py_fim_bn