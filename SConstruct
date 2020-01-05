import os

options = Variables('.scons.options', ARGUMENTS)
options.AddVariables(
	PathVariable('prefix', 'Install prefix', '/usr/local/'),
	)

env = Environment(ENV=os.environ, options=options)
options.Save('.scons.options',env)
Help(options.GenerateHelpText(env))
env.SConsignFile() # Single signature file

env.ParseConfig('pkg-config --libs --cflags poppler yaml-cpp poppler-qt5 Qt5Core')
env.Append(CCFLAGS=[
	'-g',
	'-fPIC',
	'-std=c++14',
	])

program = env.Program('pdfformburner', Glob("pdfformburner.cc"))
program_qt = env.Program('pdfformburner_qt', Glob("pdfformburner_qt.cc"))

install = env.Install(os.path.join(env['prefix'],'bin'), program)

env.Alias('install', install)

