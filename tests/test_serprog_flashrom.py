"""Actual flashrom binary against production TCP/Flash/addrspi and emulated NOR only."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

server_binary, flashrom, logs = sys.argv[1:]
logs=Path(logs); logs.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1',0)); port=probe.getsockname()[1]
    server=subprocess.Popen([server_binary,str(port),str(tmp/'nor.bin')],stdout=subprocess.PIPE,text=True)
    assert server.stdout.readline().strip()=='READY'
    pattern=bytes((i*37+i//251)&255 for i in range(128*1024))
    (tmp/'pattern.bin').write_bytes(pattern)
    results=[]
    try:
        for label,args in [
            ('probe',[]),
            ('read-erased',['-r',str(tmp/'erased.bin')]),
            ('write',['-w',str(tmp/'pattern.bin')]),
            ('verify',['-v',str(tmp/'pattern.bin')]),
            ('read-programmed',['-r',str(tmp/'read.bin')]),
            ('erase',['-E']),
            ('read-after-erase',['-r',str(tmp/'final.bin')]),
        ]:
            cmd=[flashrom,'-p',f'serprog:ip=127.0.0.1:{port}','-c','W25X10','-VV',*args]
            run=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
            (logs/f'{label}.log').write_text(run.stdout+run.stderr)
            assert run.returncode==0,(label,run.stdout[-2000:],run.stderr[-2000:])
            results.append({'operation':label,'exit_code':run.returncode})
        assert (tmp/'erased.bin').read_bytes()==b'\xff'*len(pattern)
        assert (tmp/'read.bin').read_bytes()==pattern
        assert (tmp/'final.bin').read_bytes()==b'\xff'*len(pattern)
        # Disconnect immediately after starting a one-second internal erase.
        with socket.create_connection(('127.0.0.1',port),timeout=2) as client:
            for opcode in (6,0xc7):
                client.sendall(bytes([19,1,0,0,0,0,0,opcode]))
                assert client.recv(1)==b'\x06'
        time.sleep(0.05)
        with socket.create_connection(('127.0.0.1',port),timeout=2) as competitor:
            assert competitor.recv(1)==b''  # Busy chip remains leased after EOF.
        time.sleep(1.2)
        with socket.create_connection(('127.0.0.1',port),timeout=2) as next_session:
            next_session.sendall(b'\x00')
            assert next_session.recv(1)==b'\x06'
        time.sleep(0.2)
    finally:
        server.terminate()
        output=server.communicate(timeout=10)[0]
        assert server.returncode==0,output
    stats=json.loads(output.strip().splitlines()[-1])
    assert stats['programs']>0 and stats['erases']>0 and stats['released']==9,stats
    report={'result':'pass','client_version':subprocess.check_output([flashrom,'--version'],text=True).splitlines()[0],
            'client_binary':os.path.realpath(flashrom),'operations':results,'nor_emulator':stats,
            'transport':'production moduliq_serprog + CPLDFlash + addrspi, POSIX socket adapter, inert SPI',
            'disconnect_during_erase':'pass; competitor rejected while busy, reconnect after idle',
            'hardware_programmed':False}
    (logs/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
