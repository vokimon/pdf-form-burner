env = Environment()


env.ParseConfig('pkg-config --libs --cflags poppler yaml-cpp')
env.Append(CCFLAGS='-g')
env.Program('pdfform', Glob("*.cc"))






