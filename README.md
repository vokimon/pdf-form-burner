# PDF Form Burner

A tool to extract and fill PDF forms by means of YAML data.

This command line tool can be used two fold:

- to extract form data in a PDF as a YAML file, and
- to fill a PDF form with data in a YAML file.

YAML format is more convenient for that task than other formats
such as FDF and XML, because
YAML can be easily understood and edited by humans
and still, it is machine readable and
a single unicode encoding, utf-8, to care about.


## Usage examples

Dumping the form data contained in a PDF into a YAML file:

	:::bash
	$ pdfformburner <doc.pdf> <output.yaml>

Filling doc.pdf with input.yaml to generate output.pdf

	:::bash
	$ pdfformburner doc.pdf input.yaml <output.pdf>

Automated filling and reading of PDF forms may offer agility
to many unskippable bureaucratic processes.
Other tools, like [pdftk] use [FDF] format as mean to exchange PDF form data.
I built this tool because [YAML] is a nicer format to deal with.
It is easier to parse and generate in most programming languages and
the only encoding you have to deal with is [UTF-8].

[pdftk]:(http://www.pdflabs.com/tools/pdftk-the-pdf-toolkit/)
[YAML]:(http://yaml.org)
[FDF]:(http://en.wikipedia.org/wiki/Forms_Data_Format)
[UTF-8]:(http://www.utf8everywhere.org/)

## Limitations

- Only suports the following field types:
	- Text fields
	- Non-editable single choice fields
	- Check buttons
- Just root fields (it does not decent on the hierarchy)

## Dependencies

- Scons, to build it
- poppler, to access PDF elements
- yaml-cpp, to load and dump YAML files

So in Debian and Ubuntu:

	:::bash
	$ sudo apt-get install libyaml-cpp-dev libpoppler-dev scons

## Install

	:::bash
	$ scons
	$ sudo scons intall

## Debian packaging

Debian packaging is available at the 'debian' branch. You can build the package with the command:

	:::bash
	gbp buildpackage --git-ignore-new --git-upstream-branch=master --git-debian-branch=debian --git-upstream-tag=v1.0



