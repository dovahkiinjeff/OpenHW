"""OBIG v1 writer/validator. Stored, sorted, 64-bit-offset resource package."""
from pathlib import Path
import argparse
import json
import os
import sqlite3
import struct
import zlib
from extract_remastered_big import safe_path

HEADER = struct.Struct('<8sIIQ')
ENTRY = struct.Struct('<256sQII')
MAGIC = b'OBIGPK01'


def key(name):
    result = safe_path(str(name).replace('\\', '/')).as_posix().lower()
    encoded = result.encode('ascii')
    if len(encoded) > 255:
        raise ValueError('OBIG path exceeds 255 bytes: '+result)
    return result


def source_files(root, predicate=None):
    root = Path(root).resolve()
    files = {}
    for path in root.rglob('*'):
        if not path.is_file():
            continue
        if path.is_symlink() or not path.resolve().is_relative_to(root):
            raise ValueError('OBIG source escapes root')
        name = key(path.relative_to(root))
        if predicate and not predicate(name):
            continue
        if name in files:
            raise ValueError('Duplicate OBIG path: '+name)
        if path.stat().st_size > 0x7fffffff:
            raise ValueError('Individual resource exceeds engine file-size limit')
        files[name] = path
    if not files or len(files) > 1000000:
        raise ValueError('Invalid resource count')
    return files


def pack(root, output, *, predicate=None):
    root, output = Path(root).resolve(), Path(output).resolve()
    files = source_files(root, predicate)
    temp = output.with_suffix('.obig.pending')
    output.parent.mkdir(parents=True, exist_ok=True)
    offset = HEADER.size + ENTRY.size*len(files)
    with temp.open('w+b') as stream:
        stream.write(HEADER.pack(MAGIC, 1, len(files), offset))
        stream.seek(offset)
        index = []
        for name, source in sorted(files.items()):
            start, crc = stream.tell(), 0
            with source.open('rb') as incoming:
                while chunk := incoming.read(8*1024*1024):
                    stream.write(chunk)
                    crc = zlib.crc32(chunk, crc)
            index.append((name.encode('ascii'), start, stream.tell()-start, crc))
        stream.seek(HEADER.size)
        for row in index:
            stream.write(ENTRY.pack(*row))
    validate(temp, deep=True)
    temp.replace(output)
    return len(files)


def pack_consuming(root, output, journal, *, scratch, checkpoint_bytes=64*1024*1024, progress=None):
    """Pack disposable conversion output without retaining a second full tree.

    The immutable manifest and completed offsets live in a durable SQLite
    journal. Each batch is read back and synced before its journal transaction
    commits; only then may its loose inputs be removed. A stopped installer
    resumes at the last committed batch, truncating any uncommitted tail.
    This is deliberately opt-in and restricted to an explicit scratch folder.
    """
    root, output, journal, scratch = map(lambda p: Path(p).resolve(), (root, output, journal, scratch))
    if root == scratch or not root.is_relative_to(scratch) or not journal.is_relative_to(scratch):
        raise ValueError('Consuming pack requires a contained disposable tree and journal')
    if output.is_relative_to(scratch) or checkpoint_bytes <= 0:
        raise ValueError('Consuming pack output must be outside scratch')
    pending = output.with_suffix('.obig.pending')
    output.parent.mkdir(parents=True, exist_ok=True)
    journal.parent.mkdir(parents=True, exist_ok=True)
    if not journal.exists():
        files = source_files(root)
        initial = journal.with_suffix(journal.suffix+'.initial')
        initial.unlink(missing_ok=True)
        db = sqlite3.connect(initial)
        try:
            db.execute('PRAGMA synchronous=FULL')
            db.execute('CREATE TABLE metadata (value TEXT NOT NULL)')
            db.execute('CREATE TABLE entries (name TEXT PRIMARY KEY, relative TEXT NOT NULL, size INTEGER NOT NULL, mtime INTEGER NOT NULL, offset INTEGER, crc INTEGER)')
            db.execute('INSERT INTO metadata VALUES (?)', (json.dumps(dict(schema=1, root=str(root), output=str(output))),))
            db.executemany('INSERT INTO entries(name,relative,size,mtime) VALUES (?,?,?,?)',
                ((name, str(path.relative_to(root)), path.stat().st_size, path.stat().st_mtime_ns)
                 for name, path in sorted(files.items())))
            db.commit()
        finally:
            db.close()
        initial.replace(journal)
    db = sqlite3.connect(journal)
    try:
        db.execute('PRAGMA synchronous=FULL')
        metadata = json.loads(db.execute('SELECT value FROM metadata').fetchone()[0])
        if metadata != dict(schema=1, root=str(root), output=str(output)):
            raise ValueError('Consuming pack checkpoint belongs to another installation')
        rows = db.execute('SELECT name,relative,size,mtime,offset,crc FROM entries ORDER BY name').fetchall()
        if not 0 < len(rows) <= 1000000:
            raise ValueError('Invalid consuming pack manifest')
        data = HEADER.size+ENTRY.size*len(rows)
        end, waiting = data, False
        for name, relative, size, mtime, offset, crc in rows:
            path = root/safe_path(relative)
            if key(relative) != name or not path.resolve().is_relative_to(root) or path.is_symlink():
                raise ValueError('Unsafe consuming pack source')
            if not 0 <= size <= 0x7fffffff:
                raise ValueError('Invalid consuming pack source size')
            if offset is None:
                waiting = True
                if not path.is_file() or (path.stat().st_size, path.stat().st_mtime_ns) != (size, mtime):
                    raise ValueError('Missing or changed uncommitted pack source: '+name)
            elif waiting or offset != end or crc is None:
                raise ValueError('Invalid consuming pack checkpoint order')
            else:
                end += size
        # An interruption after the atomic rename is also a successful resume.
        if not pending.exists() and all(row[4] is not None for row in rows) and output.is_file():
            expected = {row[0]: (row[4], row[2], row[5]) for row in rows}
            if validate(output, deep=True) != expected:
                raise ValueError('Completed archive differs from pack checkpoint')
            return len(rows)
        if not pending.exists():
            if end != data:
                raise ValueError('Missing pending archive for committed pack checkpoint')
            with pending.open('w+b') as stream:
                stream.write(HEADER.pack(MAGIC, 1, len(rows), data))
                stream.truncate(data)
                stream.flush(); os.fsync(stream.fileno())
        with pending.open('r+b') as stream:
            if HEADER.unpack(stream.read(HEADER.size)) != (MAGIC, 1, len(rows), data) or pending.stat().st_size < end:
                raise ValueError('Truncated or incompatible pending archive')
            def verify_payload(offset, size, expected):
                stream.seek(offset)
                actual, remaining = 0, size
                while remaining:
                    chunk = stream.read(min(remaining, 8*1024*1024))
                    if not chunk:
                        raise ValueError('Truncated pending resource')
                    actual = zlib.crc32(chunk, actual); remaining -= len(chunk)
                if actual != expected:
                    raise ValueError('Pending resource checksum mismatch')
            for name, relative, size, mtime, offset, crc in rows:
                if offset is None:break
                verify_payload(offset, size, crc)
                (root/safe_path(relative)).unlink(missing_ok=True)
            stream.truncate(end); stream.seek(end)
            batch, batch_bytes = [], 0
            def commit_batch():
                nonlocal batch, batch_bytes
                if not batch:return
                tail = stream.tell()
                stream.flush(); os.fsync(stream.fileno())
                for name, relative, size, offset, crc in batch:
                    verify_payload(offset, size, crc)
                with db:
                    db.executemany('UPDATE entries SET offset=?,crc=? WHERE name=?',
                        ((offset, crc, name) for name, relative, size, offset, crc in batch))
                for name, relative, size, offset, crc in batch:
                    (root/safe_path(relative)).unlink(missing_ok=True)
                stream.seek(tail)
                batch, batch_bytes = [], 0
                if progress:progress(tail)
            for name, relative, size, mtime, offset, crc in rows:
                if offset is not None:continue
                source = root/safe_path(relative)
                start, actual = stream.tell(), 0
                with source.open('rb') as incoming:
                    while chunk := incoming.read(8*1024*1024):
                        stream.write(chunk); actual = zlib.crc32(chunk, actual)
                if stream.tell()-start != size or source.stat().st_mtime_ns != mtime:
                    raise ValueError('Pack source changed during copy: '+name)
                batch.append((name, relative, size, start, actual)); batch_bytes += size
                if batch_bytes >= checkpoint_bytes:commit_batch()
            commit_batch()
            stream.seek(HEADER.size)
            for name, relative, size, mtime, offset, crc in db.execute('SELECT name,relative,size,mtime,offset,crc FROM entries ORDER BY name'):
                stream.write(ENTRY.pack(name.encode('ascii'), offset, size, crc))
            stream.flush(); os.fsync(stream.fileno())
        validate(pending, deep=True)
        pending.replace(output)
        return len(rows)
    finally:
        db.close()


def validate(path, deep=False):
    path = Path(path)
    with path.open('rb') as stream:
        magic, version, count, data = HEADER.unpack(stream.read(HEADER.size))
        total = path.stat().st_size
        if magic != MAGIC or version != 1 or not 0 < count <= 1000000 or data != HEADER.size+ENTRY.size*count or data > total:
            raise ValueError('Invalid OBIG header')
        rows = [ENTRY.unpack(stream.read(ENTRY.size)) for _ in range(count)]
        previous, end = '', data
        for raw, offset, size, crc in rows:
            if b'\0' not in raw:
                raise ValueError('Unterminated OBIG name')
            name = raw.split(b'\0', 1)[0].decode('ascii')
            if name != key(name) or name <= previous or size > 0x7fffffff or offset < end or offset > total or size > total-offset:
                raise ValueError('Invalid OBIG resource entry')
            if deep:
                stream.seek(offset)
                remaining, actual = size, 0
                while remaining:
                    chunk = stream.read(min(remaining, 8*1024*1024))
                    if not chunk:
                        raise ValueError('Truncated OBIG resource')
                    actual = zlib.crc32(chunk, actual)
                    remaining -= len(chunk)
                if actual != crc:
                    raise ValueError('OBIG checksum mismatch: '+name)
            previous, end = name, offset+size
        return {raw.split(b'\0', 1)[0].decode('ascii'): (offset, size, crc) for raw, offset, size, crc in rows}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    print('Packed', pack(args.root, args.output), 'resources')
