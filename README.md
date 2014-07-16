# pdfform

A tool to extract and fill PDF forms by means of YAML data.

Usage:

Dumping the form contained in a pdf into a yaml file:
	:::bash
	$ pdfform <doc.pdf> <output.yaml>

Filling doc.pdf with input.yaml to generate output.pdf
	:::bash
	$ pdfform doc.pdf input.yaml <output.pdf>

Automated filling and reading of PDF forms is a nice way to agilize many process.
Other tools, like [pdftk] use [FDF] format as mean to exchange PDF form data.
I built this tool because [YAML] is a nicer format to deal with.
It is easier to parse and generate in most programming languages and
the only encoding you have to deal with is [UTF-8].

[pdftk]:(http://www.pdflabs.com/tools/pdftk-the-pdf-toolkit/)
[YAML]:(http://yaml.org)
[FDF]:()
[UTF-8]:(http://www.utf8everywhere.org/)

## Dependencies

- Scons, to build it
- poppler, to access pdf elements
- yaml-cpp, to load and dump yaml files

So in Debian and Ubuntu:

	:::bash
	$ sudo apt-get install libyaml-cpp-dev libpoppler-dev scons

## Install

	$ scons
	$ sudo scons intall

## Limitations

- Only suports the following field types: Text and non-editable and non-multiple choice fields.
- Just root fields (it does not decent on the hierarchy)



