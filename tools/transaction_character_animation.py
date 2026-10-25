"""Standard-library producer lock and recoverable atomic file publication.

Every participating reader takes locked_output before opening published files.
A killed publisher leaves a journal; the next lock owner rolls back the entire
set before yielding to readers. Lock files are permanent OS-lock inodes.
"""
from contextlib import contextmanager
import errno
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import time
import uuid


@contextmanager
def locked_output(output, timeout=300):
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=True)
    # Locks must never become files in a cooked/shipping asset directory.
    registry=Path(tempfile.gettempdir())/'human-resources-animation-locks'
    registry.mkdir(parents=True,exist_ok=True)
    key=hashlib.sha256(os.path.normcase(str(output)).casefold().encode()).hexdigest()
    path=registry/(key+'.lock')
    with path.open('a+b') as stream:
        if path.stat().st_size==0:stream.write(b'\0');stream.flush()
        deadline=time.monotonic()+timeout
        while True:
            try:
                stream.seek(0)
                if os.name=='nt':
                    import msvcrt
                    msvcrt.locking(stream.fileno(),msvcrt.LK_NBLCK,1)
                else:
                    import fcntl
                    fcntl.flock(stream.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
                break
            except OSError as error:
                if error.errno not in (errno.EACCES,errno.EAGAIN,errno.EDEADLK):raise
                if time.monotonic()>=deadline:raise TimeoutError(f'Animation producer lock timed out: {path}') from error
                time.sleep(.05)
        try:
            recover(output)
            yield output
        finally:
            stream.seek(0)
            if os.name=='nt':
                import msvcrt
                msvcrt.locking(stream.fileno(),msvcrt.LK_UNLCK,1)
            else:
                import fcntl
                fcntl.flock(stream.fileno(),fcntl.LOCK_UN)


def atomic_bytes(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    fd,name=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
    try:
        with os.fdopen(fd,'wb') as stream:
            stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.replace(name,path)
    finally:
        Path(name).unlink(missing_ok=True)


def relative_name(name):
    path=Path(name)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        raise ValueError('Invalid animation publication path')
    return path


def recover(output):
    journal=output/'.animation-transaction.json'
    if not journal.exists():return
    record=json.loads(journal.read_text())
    parent=output.parent if record.get('sibling_backup') else output
    backup=parent/relative_name(record['backup'])
    for name,existed in record['files'].items():
        name=relative_name(name);target=output/name
        if existed:atomic_bytes(target,(backup/name).read_bytes())
        else:target.unlink(missing_ok=True)
    journal.unlink()
    shutil.rmtree(backup)


@contextmanager
def staging(output):
    # Same filesystem as publication; unique names never collide across jobs.
    output=Path(output)
    with tempfile.TemporaryDirectory(prefix='.'+output.name+'-animation-stage-',dir=output.parent) as name:
        yield Path(name)


def publish(output,stage,names,after_replace=None):
    """Call only under locked_output. Roll back on exception or next lock entry."""
    output,stage=Path(output),Path(stage)
    names=[relative_name(n) for n in names]
    if len(set(names))!=len(names):raise ValueError('Duplicate publication target')
    if any(not (stage/n).is_file() for n in names):raise ValueError('Missing staged animation output')
    backup=output.parent/('.'+output.name+'-animation-backup-'+uuid.uuid4().hex);backup.mkdir()
    record={'backup':backup.name,'sibling_backup':True,'files':{}}
    try:
        for name in names:
            target=output/name;record['files'][name.as_posix()]=target.is_file()
            if target.is_file():
                saved=backup/name;saved.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(target,saved)
        journal=output/'.animation-transaction.json'
        atomic_bytes(journal,(json.dumps(record)+'\n').encode())
        for index,name in enumerate(names):
            target=output/name;target.parent.mkdir(parents=True,exist_ok=True)
            os.replace(stage/name,target)
            if after_replace:after_replace(index)
        journal.unlink()  # Commit only after every file has been installed.
    except BaseException:
        recover(output)
        raise
    finally:
        if not (output/'.animation-transaction.json').exists():shutil.rmtree(backup,ignore_errors=True)


def snapshot(output,stage,directories):
    for name in directories:
        source=output/name
        if source.exists():shutil.copytree(source,stage/name,dirs_exist_ok=True)
