top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')
    opt.add_option('--enable-update', action='store_true',
      default='', dest='enable_update')

def configure(ctx):
    ctx.load('compiler_cxx')
    ctx.env.ENABLE_UPDATE = ctx.options.enable_update

def build(ctx):
    ctx.recurse('src')