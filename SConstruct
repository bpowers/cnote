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
    print 'pkg-config >= 0.15.0 not found.', stderr
    Exit(1)

if not conf.CheckPkg('nss'):
    print 'nss (libnss3-dev) required.', stderr
    Exit(1)

if not conf.CheckPkg('libevent >= 2.0'):
    print 'libevent2 not found.', stderr
    Exit(1)

if not conf.CheckPkg('id3tag'):
    print 'libid3tag (libid3tag0-dev) required.', stderr
    Exit(1)

env.ParseConfig('pkg-config --cflags --libs libevent')
env.ParseConfig('pkg-config --cflags --libs nss')

env.Library('lib/cfunc', Glob('lib/*.c'))

# from here on are cmusic-specific configs
env.ParseConfig('pkg-config --cflags --libs id3tag')
env.MergeFlags(env.ParseFlags(['-Llib', '-lcfunc']))
env.Program('src/cmusic', Glob('src/*.c'))
