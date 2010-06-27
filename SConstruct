import os.path
from sys import stderr

env = Environment(CC='clang',
                  CCFLAGS='-fcolor-diagnostics -fblocks -O2 -g -Wall',
                  CPPPATH='include')

def CheckPkgConfig(context, version):
    context.Message('Checking for pkg-config... ')
    ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
    context.Result(ret)
    return ret

def CheckPkg(context, name):
    context.Message('Checking for %s... ' % name)
    ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
    context.Result(ret)
    return ret

conf = Configure(env, custom_tests = {'CheckPkgConfig': CheckPkgConfig,
                                      'CheckPkg': CheckPkg })

if not conf.CheckPkgConfig('0.15.0'):
    print >> stderr, 'pkg-config >= 0.15.0 not found.'
    Exit(1)

if not conf.CheckPkg('libevent >= 2.0'):
    print >> stderr, 'libevent2 not found.'
    Exit(1)

if not conf.CheckPkg('taglib_c'):
    print >> stderr, 'taglib_c (libtagc0-dev) required.'
    Exit(1)

if not conf.CheckPkg('openssl'):
    print >> stderr, 'openssl (libssl-dev) required.'
    Exit(1)

if not os.path.exists('/usr/bin/pg_config'):
    print >> stderr, 'pg_config (libpg-dev) required.'
    Exit(1)

env.ParseConfig('pkg-config --cflags --libs libevent')
env.ParseConfig('pkg-config --cflags --libs openssl')

env.MergeFlags(env.ParseFlags(['!pg_config --cflags', '!pg_config --libs',
                               '!pg_config --ldflags', '-lpq']))

env.Library('lib/cfunc', Glob('lib/*.c'))

# from here on are cmusic-specific configs
env.ParseConfig('pkg-config --cflags --libs taglib_c')
env.MergeFlags(env.ParseFlags(['-Llib', '-lcfunc']))

env.Program('src/ctagdump', Glob('src/ctagdump.c'))
#env.Program('src/cmusic', Glob('src/cmusic.c'))
