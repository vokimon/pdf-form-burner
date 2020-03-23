import os

options = Variables('.scons.options', ARGUMENTS)
options.AddVariables(
	PathVariable('prefix', 'Install prefix', '/usr/local/'),
	)

env = Environment(ENV=os.environ, options=options)
options.Save('.scons.options',env)
Help(options.GenerateHelpText(env))
env.SConsignFile() # Single signature file

env.ParseConfig('pkg-config --libs --cflags poppler yaml-cpp poppler-qt5 Qt5Core ')
env.Append(LIBS=['fmt'])
env.Append(CCFLAGS=[
	'-g',
	'-fPIC',
	'-std=c++14',
	])


env['HELP2MAN'] = env.WhereIs('help2man', os.environ['PATH'])
Help2Man = env.Append(BUILDERS={'Help2Man': Builder(
	action='$HELP2MAN ./$SOURCE > $TARGET',
	suffix = '.1',
	src_suffix = '',
)})

program_legacy = env.Program('pdfformburner_legacy', Glob("pdfformburner_legacy.cc"))
program = env.Program('pdfformburner', Glob("pdfformburner_qt.cc"))
manpage = env.Help2Man(source=program)

install = [
	env.Install(os.path.join(env['prefix'],'bin'), program),
	env.Install(os.path.join(env['prefix'],'man/man1'), manpage),
	]

env.Alias('install', install)
env.Alias('manpage', manpage)

