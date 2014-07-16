env = Environment()


env.ParseConfig('pkg-config --libs --cflags poppler')
env.Append(CCFLAGS='-g')
env.Program('pdfform', Glob("*.cc"))






