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

```bash
$ pdfformburner <doc.pdf> <output.yaml>
```

Filling doc.pdf with input.yaml to generate output.pdf

```bash
$ pdfformburner doc.pdf input.yaml <output.pdf>
```

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

Some fields are not fully supported yet:

- RadioButtons are not working yet
- FileSelects just fills and extracts the name, not the file content
- Signature info is extracted and validated but not filled (Signing)
- Actions related to fields (derived fields...) are not executed

## Dependencies

- Scons, to build it
- poppler-qt5, to access PDF elements
- yaml-cpp, to load and dump YAML files

So in Debian and Ubuntu:

```bash
$ sudo apt install libyaml-cpp-dev libpoppler-dev scons libboost-dev
```

## Install

```bash
$ scons
$ sudo scons intall
```

## Debian packaging

Debian packaging is available at the 'debian' branch. You can build the package with the command:

```bash
sudo apt-get install debhelper git-buildpackage
git checkout debian
gbp buildpackage --git-ignore-new --git-upstream-branch=master --git-debian-branch=debian --git-upstream-tag=v1.0
```

## Changelog

### 2.0 (2020-01-06)

- New version based on poppler-qt5 instead of the private poppler api
- Editable ChoiceFields working
- Multiple ChoiceFields working
- FileSelect TextFiels working, but just takes the name, not the contents
- Exports signature field data
- Fix: PushButtons broke yamls
- Legacy version:
	- Old version still available as `pdfformburner_legacy`
	- Fix: PushButtons broke yamls
- Regression tests added
- Man page
- Known bugs:
  	- Some field ui names are exported as 'þÿ'
	  because a bug in poppler-qt5 (a fix is to be released)
	- RadioButtons still not working
	- No filling for signatures
	- Multilines add a new empty line each extract/fill cycle 

### 1.3

- Port to libpoppler v0.84.0
- Alternate names (ui names and mapping names) extracted as comments
- Fix: non-terminal nodes can have type for inheritance
- Fix: numChildren in terminal nodes means num widgets

### 1.2

- Properly fill checkboxes

### 1.1

- Nested fields supported

### 1.0

- First stable version









