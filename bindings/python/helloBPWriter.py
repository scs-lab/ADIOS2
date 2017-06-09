# clang-format off
#
# Distributed under the OSI-approved Apache License, Version 2.0.  See
# accompanying file Copyright.txt for details.
#
# test_hello.py
#  Created on: Feb 2, 2017
#      Author: wfg

from mpi4py import MPI
from adios2py import *
import numpy as np

# User data
myArray = np.array([1., 2., 3., 4., 5., 6.])

# ADIOS
adios = ADIOS(MPI.COMM_WORLD, adiosDebugON)

# IO
bpIO = adios.DeclareIO("BPN2N")

# Variable
ioArray = bpIO.DefineVariable(
    "bpArray", [myArray.size], [0], [myArray.size], adiosConstantDims)

# Engine
bpFileWriter = bpIO.Open("myArray.bp", adiosOpenModeWrite)
# bpFileWriter = bpIO.Open("myArray.bp", adiosOpenModeWrite,
# MPI.COMM_WORLD) //doesn't work
bpFileWriter.Write(ioArray, myArray)
bpFileWriter.Close()
