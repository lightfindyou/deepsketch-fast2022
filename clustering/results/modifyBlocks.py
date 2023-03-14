import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from torchvision import datasets, transforms
import random
import numpy as np
import shutil

import hashlib
import pickle
import time
from tqdm import tqdm

import sys

path = sys.argv[1]

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

import os

fileprefix="modifyBlocks"
logfile = open(fileprefix + ".log", 'w')

def log(logstr):
    print(logstr)
    logfile.write(logstr + '\n')

# Read data
from glob import glob
dirs = glob(os.path.join(path, '*/'))
dirNum = len(dirs)

for i in range(dirNum):
    files = glob(os.path.join(dirs[i], '*'))
    fileNum = len(files)
    if fileNum < 20 :
        targetFileNum = random.randint(21,39)
        #generage new files
        for j in range(fileNum, targetFileNum):
            fileIdx = random.randint(0, fileNum-1)
            srcFileName = files[fileIdx]
            destFileName = os.path.join(dirs[i], "0000"+str(j))
            shutil.copyfile(srcFileName, destFileName)
            changeNum = random.randint(1,3)
            f = open(destFileName, "rb+")
            originalFileSize = os.path.getsize(destFileName)
            fileSize = originalFileSize 
            #update each new file with random data for 1~3 times
            for k in range (changeNum):
                changeLen = random.randint(1, min(200, originalFileSize//3))
#                data = random.randbytes(changeLen)
                cbytes = os.urandom(changeLen)
                changePos = random.randint(0, fileSize - len(cbytes) - 1)
                f.seek(changePos)
                overwrite = random.randint(0, 1) or fileSize + len(cbytes) > 8192
                if( overwrite ): #overwrite data
                    f.write(cbytes)
                else:
                    fileData = f.read()
                    f.seek(changePos)
                    f.write(cbytes)
                    f.write(fileData)
                    fileSize += len(cbytes)
            f.close()
#            newFileSize = os.path.getsize(destFileName)
#            if(newFileSize > 8192):
#                print("file: "+destFileName)
        files = glob(os.path.join(dirs[i], '*'))
        curFileNum = len(files)
        log("%-*s: file number %d to %d" %(20, str(dirs[i]), fileNum, curFileNum) )
