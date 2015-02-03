import re
import sys
import os

class EntryDef:
    id = 0
    specifiers = ''
    format = ''
    
    def __repr__(self):
        return '{0:>5}, {1:>35}, {2}'.format(self.id, self.specifiers, self.format)

    def count_specifiers_binary(self, specifiers):
        specs = specifiers.split()
        sum = 0
        if specs:
            for s in specs:
                if s[-1] in 'fFxXudDSTp':
                    sum += 4
                elif s[-1] in 'c':
                    sum += 1
                else:
                    print(s)
                    assert False
        return sum
        
    def get_size_in_binary(self):
        return 2 + self.count_specifiers_binary(self.specifiers)

    def to_text(self, id, line):
        fmt = self.format.split('@')
        for f in fmt:
            if f:
                line = line.replace(f, '|', 1)
        if line[-1] != '|':
            line += '|'
        return '{0:04X}'.format(id) + line + '\n'

    def to_printf_format(self):
        specs = self.specifiers.split()
        assert len(specs) == self.format.count('@')
        line = self.format
        for s in specs:
            line = line.replace('@', s, 1)
        print('"', line, '")', sep='')
        


class Entries:
    entry_defs = []
    
    def read_from_file(self, filename): 
        for line in open(filename, encoding='utf-8'):
            if 'LOG_ENTRY' in line:
                columns = line.split('|')
                entry = EntryDef()
                entry.id = int(columns[1], 16)
                entry.specifiers = columns[3]
                entry.format = columns[5]
                self.entry_defs.append(entry) 

    def __repr__(self):
        s = ''
        for e in self.entry_defs:
            s += '{0:>2} <=====> {1}\n'.format(e.get_size_in_binary(), e)
        return s

    def find_entry(self, id):
        for e in self.entry_defs:
            if e.id == id:
                return e
        return None


class LogParser:
    regex = re.compile(r'.*\[\d\d:\d\d\d\]\[(\dx[0-9A-F]{4,4})\](.*)')
    
    def __init__(self, entries):
        self.entries = entries

    def get_log_line_id(self, line):
        l = self.regex.search(line)
        if l:
            return int(l.group(1), 16)
        return -1

    def calculate_log_file_size_in_binary(self, filename):
        sum = 0
        for line in open(filename, encoding='utf-8'):
            id = self.get_log_line_id(line)
            if id >= 0:
                e = self.entries.find_entry(id)
                if e:
                    sum += e.get_size_in_binary()
                else:
                    if id not in [0xffff, 0]:
                        print('id was not found: ', id, file=sys.stderr)
                        #assert False
            else:
                #print('line does not have valid id: ', line, end = '')
                None
    
        return sum

                
    def calculate_log_file_size_in_text(self, filename):
        sum = 0
        for line in open(filename, encoding='utf-8'):
            id = self.get_log_line_id(line)
            if id >= 0:
                e = self.entries.find_entry(id)
                if e:
                    s = e.to_text(id, self.regex.search(line).group(2))
                    print(s, end='')
                    sum += len(s)
                else:
                    if id not in [0xffff, 0]:
                        print('id was not found: ', id, file=sys.stderr)
                        #assert False
            else:
                #print('line does not have valid id: ', line, end = '')
                None
    
        return sum
    
    def write_to_file_as_new_format(self, filename, outputfilename):
        f = open(outputfilename, 'w', encoding='utf-8')
        sum = 0
        for line in open(filename, encoding='utf-8'):
            id = self.get_log_line_id(line)
            if id >= 0:
                e = self.entries.find_entry(id)
                if e:
                    s = e.to_text(id, self.regex.search(line).group(2))
                    print(s, end='', file=f)
                    sum += len(s)
                else:
                    if id not in [0xffff, 0]:
                        print('id was not found: ', id, file=sys.stderr)
                        #assert False
            else:
                #print('line does not have valid id: ', line, end = '')
                None
        
        f.close()
        return sum
    
    def write_to_file_and_remove_time(self, filename, outputfilename):
        f = open(outputfilename, 'w', encoding='utf-8')
        sum = 0
        for line in open(filename, encoding='utf-8'):
            id = self.get_log_line_id(line)
            if id >= 0:
                e = self.entries.find_entry(id)
                if e:
                    l = self.regex.search(line)
                    if l:
                        s = l.group(1) + l.group(2)
                        print(s, file=f)
                        sum += len(s)
                else:
                    if id not in [0xffff, 0]:
                        print('id was not found: ', id, file=sys.stderr)
                        #assert False
            else:
                print('line does not have valid id: ', line, end = '', file=sys.stderr)
                None

        f.close()
        return sum

def main(argv):
    entries = Entries()
    entries.read_from_file(argv[1])

    parser = LogParser(entries)
    print('{0:80}|{1:>7}|{2:>7}|{3:>7}|{4:>7}|{5:>7}|{6:>7}|{7:>7}|'.format('file', 'origin', 'lzss', 'gzip', 'binary', 'text', 't-lzss', 't-gzip'))
    for f in argv[2:]:
        try:
            nn = parser.calculate_log_file_size_in_binary(f)
            if nn > 0:
                newf = f + '.newformat'
                #m = parser.write_to_file_as_new_format(f, newf)
                newfcompressed = newf + '.lzss'
                newfcompressedgzip = newf + '.gz'
                #os.system('./lzss-1024 c {0} {1}'.format(newf, newfcompressed))
                newfnotime = f + '.notime'
                parser.write_to_file_and_remove_time(f, newfnotime)
                newfnotimecompressed = newfnotime + '.lzss'
                newfnotimecompressedgzip = newfnotime + '.gz'
                os.system('./lzss-1024 c {0} {1}'.format(newfnotime, newfnotimecompressed))
                os.system('gzip -9 < {0} > {1}'.format(newfnotime, newfnotimecompressedgzip))
                os.system('gzip -9 < {0} > {1}'.format(newf, newfcompressedgzip))
                p = os.path.getsize(newfcompressed)
                aa = os.path.getsize(newfnotime)
                bb = os.path.getsize(newfnotimecompressed)
                cc = os.path.getsize(newfnotimecompressedgzip)
                dd = os.path.getsize(newf)
                ee = os.path.getsize(newfcompressed)
                ff = os.path.getsize(newfcompressedgzip)
                
                print('{0:80}|{1:>7}|{2:>7}|{3:>7}|{4:>7}|{5:>7}|{6:>7}|{7:>7}|'.format(f, aa, bb, cc, nn, dd, ee, ff))
        finally:
            None
                
if __name__ == "__main__":
    #argv = ['', '/home/mohammad/Work/repositories/hyperion/logger_py_script_friendly.defs', '/home/mohammad/Work/temp/sites/fey/201412/logs.201412010000.log']
    #main(sys.argv)

    entries = Entries()
    entries.read_from_file('/home/mohammad/Work/repositories/hyperion/logger_py_script_friendly.defs')

    for e in entries.entry_defs:
        e.to_printf_format()


