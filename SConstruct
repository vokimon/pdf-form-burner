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

program_legacy = env.Program('pdfformburner_legacy', Glob("pdfformburner_legacy.cc"))
program = env.Program('pdfformburner', Glob("pdfformburner_qt.cc"))

install = env.Install(os.path.join(env['prefix'],'bin'), program)

env.Alias('install', install)

