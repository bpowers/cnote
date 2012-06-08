from sys import stderr, argv
from subprocess import Popen, PIPE
from os import environ, getcwd
from datetime import datetime
from os.path import dirname, samefile, join
from shutil import copyfile

LOCAL_PKGCONFIG = '/usr/local/lib/pkgconfig'
PKG_PATH = 'PKG_CONFIG_PATH'

# workaround because apparently the local/bin pkg-config path isn't in
# the system package-config search path, like I think it is on debian-
# based systems.
if PKG_PATH in environ:
    new_path = '%s:%s' % (LOCAL_PKGCONFIG, environ[PKG_PATH])
else:
    new_path = LOCAL_PKGCONFIG
environ[PKG_PATH] = new_path

def slurp(fname):
    with open(fname, 'r') as f:
        return f.read()

def run_cmd(cmd, effect='stdout'):
    '''
    Runs a shell command, waits for it to complete, and returns stdout.
    '''
    with open('/dev/null', 'w') as dev_null:
        call = Popen(cmd, shell=True, stdout=PIPE, stderr=dev_null)
        ret, _ = call.communicate()
        if effect == 'stdout':
            return ret
        else:
            return call.returncode

def _new_env():
    return {
        'cflags': '',
        'ldflags': '-lrt',
        'libs': '',
    }

help_text = '''Usage: ./configure [ OPTIONS ]
Configures the build system for cfunc.

Options:

  --help      display this help and exit
  --debug     build with debugging symbols
  --profile   build with gcov profiling support
  --optimize  build with heavy optimizations

Report bugs to <bobbypowers@gmail.com>.
'''

class ConfigBuilder:
    def __init__(self):
        self.env = _new_env()
        self.defs = {}
        self.defs['year'] = str(datetime.now().year)
        self.debug_build = '--debug' in argv
        self.profile_build = '--profile' in argv
        self.optimize_build = '--optimize' in argv
        self.pkg_config = 'pkg-config'

        if '--help' in argv:
            print help_text
            exit(0)

    def config(self, key, val):
        self.defs[key] = val

    def require(self, lib='', program=None):
        if not program:
            program = self.pkg_config

        env = self.env
        path = run_cmd('which %s' % (program))
        if len(path) is 0:
            print >> stderr, 'required program "%s" not found.' % program
            exit(1)

        # single-quote stuff for pkg-config
        if len(lib) > 0:
            lib = "'%s'" % lib

        if len(lib) > 0:
            # assumes a pkg-config-like program
            ret = run_cmd('%s --exists %s' % (program, lib), 'returncode')
            if ret != 0:
                print >> stderr, 'required library %s not found' % lib
                exit(1)

        for info in ['cflags', 'libs', 'ldflags']:
            if info in env:
                existing = env[info]
            else:
                existing = ''
            new = run_cmd('%s --%s %s' % (program, info, lib)).strip()
            env[info] = existing + ' ' + new

    def generate(self, fname='config.mk'):
        if self.profile_build:
            self.append('cflags', '-D_PROF')
        with open(fname, 'w') as config:
            for info in self.env:
                values = self.env[info].strip()
                config.write('%s := %s\n' % (info.upper(),
                                             values))

        with open('config.h', 'w') as config:
            config.write('#ifndef _CONFIG_H_\n')
            config.write('#define _CONFIG_H_\n\n')
            for var in self.defs:
                config.write('#define %s\t"%s"\n' %
                             (var.upper().replace('-', '_'), self.defs[var]))
            config.write('\n#endif // _CONFIG_H_\n')

        src_dir = dirname(argv[0])
        if not samefile(src_dir, getcwd()):
            copyfile(join(src_dir, 'Makefile'), 'Makefile')


    def append(self, var, val):
        self.env[var] += ' ' + val

    def prefer_cc(self, cc):
        path = run_cmd('which %s' % (cc))
        if len(path) > 0:
            self.env['cc'] = cc
        else:
            print 'warning: %s not found, using default cc' % cc
