#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Real set ownership and origin tests through the public Goose interface."""
import errno
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
COMMAND=sys.argv[1:]
assert COMMAND
class Driver:
    def __init__(self,paths):
        self.proc=subprocess.Popen([*COMMAND,*map(str,paths)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
        self.pending=b''
        try: assert self.line()=='READY'
        except BaseException:
            self.proc.kill();self.proc.communicate(timeout=10);raise
    def line(self):
        end=time.monotonic()+15
        while b'\n' not in self.pending:
            left=end-time.monotonic();assert left>0 and select.select([self.proc.stdout],[],[],left)[0],'set driver timeout'
            data=os.read(self.proc.stdout.fileno(),65536);assert data,'set driver closed stdout'
            self.pending+=data
        value,self.pending=self.pending.split(b'\n',1);return value.decode('ascii')
    def call(self,text='next'):
        self.proc.stdin.write(text.encode()+b'\n');return self.line()
    def drain(self):
        rows=[]
        for _ in range(20000):
            line=self.call()
            if line in ('IDLE','END'):return rows,line
            index,mask,cookie,name=line.split(' ',3);rows.append((int(index),int(mask),int(cookie),bytes.fromhex(name)))
        raise AssertionError('event stream did not drain within bound')
    def close(self):
        try:
            assert self.call('close')=='CLOSED'
            _,error=self.proc.communicate(timeout=10)
            if 'run' in COMMAND:error=error.replace(b"building the package watch's C\n",b'')
            assert self.proc.returncode==0 and not error,error
        finally:
            if self.proc.poll() is None:self.proc.kill();self.proc.communicate(timeout=10)

def rejected(paths,kind,index,os_error=None):
    result=subprocess.run([*COMMAND,*map(str,paths)],capture_output=True,timeout=20)
    assert result.returncode==0,result
    words=result.stdout.decode().strip().split()
    assert words[0]=='OPEN_ERROR' and words[1]=='Error.'+kind and int(words[3])==index,words
    if os_error is not None:assert int(words[2])==os_error,words

with tempfile.TemporaryDirectory(prefix='watch-set-') as temp:
    root=Path(temp);dirs=[root/str(i) for i in range(3)]
    for d in dirs:d.mkdir()
    driver=Driver(dirs)
    try:
        assert driver.call('bad')=='INVALID' and driver.call('wait')=='IDLE'
        for i,d in enumerate(dirs):(d/f'file{i}.goose').write_bytes(b'x')
        assert driver.call('live')=='LIVE'
        rows,end=driver.drain();assert end=='IDLE'
        for i in range(3):assert any(index==i and mask&256 and name==f'file{i}.goose'.encode() for index,mask,_,name in rows),rows
        (dirs[0]/'file0.goose').rename(dirs[1]/'moved.goose')
        rows,_=driver.drain();before=[(index,c) for index,m,c,_ in rows if m&64];after=[(index,c) for index,m,c,_ in rows if m&128]
        assert before and after and before[0][0]==0 and after[0][0]==1 and before[0][1]==after[0][1]!=0
        for raw in (b'bad-\xff\n\x1b',b'x'*255):
            path=os.fsencode(dirs[2])+b'/'+raw;fd=os.open(path,os.O_CREAT|os.O_WRONLY,0o600);os.close(fd)
            rows,_=driver.drain();assert any(i==2 and m&256 and n==raw for i,m,_,n in rows)
            os.unlink(path);driver.drain()
        assert driver.call('reverse')=='REOPENED'
        (dirs[0]/'reordered.goose').touch();rows,_=driver.drain();assert any(i==2 and m&256 and n==b'reordered.goose' for i,m,_,n in rows)
        # Remove a watched directory: deliver its terminal event, then stable End.
        for file in dirs[2].iterdir():file.unlink()
        driver.drain();dirs[2].rmdir();rows,end=driver.drain()
        assert any(i==0 and m&32768 for i,m,_,_ in rows) and end=='END',rows
        assert driver.call()=='END'
    finally:driver.close()
    dirs[2].mkdir()
    rejected([dirs[0],dirs[0]],'InvalidInput',1)
    alias=root/'alias';alias.symlink_to(root,target_is_directory=True)
    rejected([dirs[0],alias/'0'],'SystemError',1,errno.EEXIST)
    rejected([dirs[0],root/'absent'],'SystemError',1,errno.ENOENT)
    rejected([alias],'SystemError',0)
    rejected([dirs[0],str(dirs[1])+'/'],'InvalidInput',1)
    rejected([dirs[0],'x'*4096],'InvalidInput',1)
    # The complete limit uses unique real directories, never a partial prefix.
    many=[]
    for i in range(256):d=root/f'd{i:03}';d.mkdir();many.append(d)
    driver=Driver(many)
    try:
        many[-1].joinpath('last.goose').touch()
        index,mask,_,name=driver.call().split(' ',3)
        assert int(index)==255 and int(mask)&256 and bytes.fromhex(name)==b'last.goose'
        # close now checks that this nonempty returned name survives owner close.
    finally:driver.close()
print('set origin, aliases, rename, byte names, terminal, reordering and max-count tests passed')
