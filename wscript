top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')

def configure(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')

def build(ctx):
    ctx.recurse('src')