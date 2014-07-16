import os

options = Variables('.scons.options', ARGUMENTS)
options.AddVariables(
	PathVariable('prefix', 'Install prefix', '/usr/local/'),
	)

env = Environment(ENV=os.environ, options=options)
options.Save('.scons.options',env)
Help(options.GenerateHelpText(env))
env.SConsignFile() # Single signature file



env.ParseConfig('pkg-config --libs --cflags poppler yaml-cpp')
env.Append(CCFLAGS='-g')

program = env.Program('pdfform', Glob("*.cc"))

install = env.Install(os.path.join(env['prefix'],'bin'), program)

env.Alias('install', install)

