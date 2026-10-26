"""Read-only, bounded extractor for unencrypted Homeworld SGA v2 archives.

Layout reference: hw2view/doc/big_file_format.html. Unknown/encrypted layouts
are rejected. No archive decryption or installation writes are performed.
"""
from pathlib import Path, PurePosixPath
import struct
import zlib


def safe_path(name):
    name = name.replace('\\', '/')
    p = PurePosixPath(name)
    if not name or p.is_absolute() or any(x in ('..', '') or ':' in x for x in p.parts):
        raise ValueError('Unsafe archive path: ' + name)
    if any(x.endswith((' ', '.')) for x in p.parts):
        raise ValueError('Ambiguous Windows archive path: ' + name)
    reserved = {'con','prn','aux','nul'} | {f'{n}{i}' for n in ('com','lpt') for i in range(1,10)}
    if any(x.split('.')[0].lower() in reserved or any(ord(c)<32 or c in '<>"|?*' for c in x) for x in p.parts):
        raise ValueError('Invalid Windows archive path: '+name)
    return Path(*p.parts)


class BigArchive:
    def __init__(self,archive):
        self.path=Path(archive).resolve(strict=True)
        with self.path.open('rb') as stream:
            size = self.path.stat().st_size
            header = stream.read(180)
            if len(header) != 180 or header[:12] != b'_ARCHIVE\x02\0\0\0':
                raise ValueError('Unsupported/encrypted Remastered archive')
            metadata_size, data_base = struct.unpack_from('<II', header, 172)
            if not 24 <= metadata_size <= 32*1024*1024 or data_base < 180+metadata_size or data_base > size:
                raise ValueError('Invalid SGA table bounds')
            metadata = stream.read(metadata_size)
            if len(metadata) != metadata_size:
                raise ValueError('Truncated SGA metadata')
            sections = struct.unpack_from('<IHIHIHIH', metadata)

            def table(offset, count, stride):
                if offset < 24 or offset + count*stride > len(metadata):
                    raise ValueError('SGA table outside metadata')
                return metadata[offset:offset+count*stride]

            folders = table(sections[2], sections[3], 12)
            files = table(sections[4], sections[5], 17)
            strings = sections[6]
            if not 24 <= strings < len(metadata):
                raise ValueError('Invalid SGA name table')

            def name(offset):
                start = strings+offset
                end = metadata.find(b'\0', start, min(start+4096, len(metadata)))
                if start < strings or end < start:
                    raise ValueError('Invalid SGA name')
                return metadata[start:end].decode('ascii')

            owners = {}
            for i in range(sections[3]):
                n, sub_start, sub_end, first, last = struct.unpack_from('<I4H', folders, i*12)
                if not sub_start <= sub_end <= sections[3] or not first <= last <= sections[5]:
                    raise ValueError('Invalid SGA folder range')
                folder = name(n)
                for index in range(first, last):
                    if index in owners:
                        raise ValueError('Multiply owned SGA file')
                    owners[index] = folder
            self.entries = {}
            for i in range(sections[5]):
                n, flags, offset, stored, expanded = struct.unpack_from('<IBIII', files, i*17)
                if i not in owners:raise ValueError('Unowned SGA file')
                key=safe_path('/'.join(x for x in (owners[i],name(n)) if x)).as_posix().lower()
                if key in self.entries:raise ValueError('Duplicate SGA path')
                if flags not in (0,0x10,0x20) or expanded>512*1024*1024:raise ValueError('Unsupported SGA compression/size')
                if offset<264 or data_base+offset+stored>size:raise ValueError('SGA payload outside archive')
                self.entries[key]=(data_base+offset,stored,expanded,flags)

    def read(self,name):
     key=safe_path(name).as_posix().lower();start,stored,expanded,flags=self.entries[key]
     with self.path.open('rb') as f:
      f.seek(start-4);crc=struct.unpack('<I',f.read(4))[0];payload=f.read(stored)
     if len(payload)!=stored:raise ValueError('Truncated SGA payload')
     if flags:
      decoder=zlib.decompressobj();payload=decoder.decompress(payload,expanded+1)
      if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:raise ValueError('Invalid/oversized SGA compressed stream')
     if len(payload)!=expanded or zlib.crc32(payload)&0xffffffff!=crc:raise ValueError('SGA size/CRC mismatch: '+key)
     return payload
    def extract(self,output,prefixes=('',),*,predicate=None):
     output=Path(output).resolve();written=[]
     for key in self.entries:
      if not any(key.startswith(p.lower()) for p in prefixes) or (predicate and not predicate(key)):continue
      destination=output/safe_path(key)
      if not destination.resolve().is_relative_to(output):raise ValueError('SGA destination escapes output')
      destination.parent.mkdir(parents=True,exist_ok=True)
      pending=destination.with_name(destination.name+'.import-pending');pending.write_bytes(self.read(key));pending.replace(destination);written.append(key)
     return written

def extract(archive,output,prefixes=('background/',),*,list_only=False):
 source=BigArchive(archive)
 written=([n for n in source.entries if any(n.startswith(p.lower()) for p in prefixes)] if list_only else source.extract(output,prefixes))
 if not written:raise ValueError('No requested SGA entries found')
 return written
