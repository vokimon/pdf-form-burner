# pdfform

A tool to extract and fill PDF forms by means of YAML data.

Usage:

Dump the form contained in a pdf into a yaml file:
	:::bash
	$ pdfform <doc.pdf> <output.yaml>
	$ pdfform doc.pdf input.yaml <output.pdf>    # fills doc.pdf with input


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

- Text and non-editable and non-multiple choice fields are the only ones supported


