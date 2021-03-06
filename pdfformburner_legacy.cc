#include <poppler-config.h>
#include <poppler/PDFDocFactory.h>
#include <poppler/Page.h>
#include <poppler/Form.h>
#include <poppler/GlobalParams.h>
#include <poppler/UnicodeMap.h>
#include <poppler/UTF.h>
#include <poppler/Dict.h>
#include <yaml-cpp/yaml.h>

#include <iostream>
#include <string>
#include <fstream>

bool showTypes = false;
GlobalParams * globalParams=0;
UnicodeMap * uMap = 0;
static char textEncName[128] = "UTF-8";

const char * typeStrings[] = {
	"Button",
	"Text",
	"Choice",
	"Signature",
	"Undef",
	0};

const char * buttonTypeStrings[] = {
	"Check",
	"Push",
	"Radio",
	0};

template <typename T>
class autodelete
{
public:
	autodelete(T*o) : _o(o) {}
	~autodelete() { delete _o;}
private:
	T * _o;
};

int usage(const char * programName)
{
	std::cerr
		<< "A tool to extract and fill PDF forms by means of YAML data.\n"
		<< "Usage:" << std::endl
		<< "\t" << programName << " <file.pdf> [<output.yaml>]" << std::endl
		<< "\t\tDumps the form field data into a YAML file (or to standard output)" << std::endl
		<< "\t" << programName << " <file.pdf> <input.yaml> <output.pdf>" << std::endl
		<< "\t\tUses the yaml file to fill file.pdf into output.pdf" << std::endl
		;
	return -1;
}

int error(const std::string & message, int code=-1)
{
	std::cerr << "\033[31;1m" << message << "\033[0m" << std::endl;
	return code;
}

std::string pdftext_2_utf8(const GooString *s)
{
	if (!s) return "";
	if (!s->c_str()) return "";

	std::string output;
	Unicode *u;
	char buffer[8];
	unsigned len = TextStringToUCS4(s, &u);
	for (unsigned i = 0; i < len; i++) {
		int n = uMap->mapUnicode(u[i], buffer, sizeof(buffer));
		output.append(buffer,n);
	}
	free(u);
	return output;
}

// Taken from 
GooString * utf8_2_pdftext(const std::string & s)
{
	GooString * result = new GooString;
	const char * in = s.c_str();
	unsigned int codepoint;
	result->append('\xfe');
	result->append('\xff');
	while (*in != 0)
	{
		unsigned char ch = static_cast<unsigned char>(*in);
		if (ch <= 0x7f)
			codepoint = ch;
		else if (ch <= 0xbf)
			codepoint = (codepoint << 6) | (ch & 0x3f);
		else if (ch <= 0xdf)
			codepoint = ch & 0x1f;
		else if (ch <= 0xef)
			codepoint = ch & 0x0f;
		else
			codepoint = ch & 0x07;
		++in;
		if (((*in & 0xc0) != 0x80) && (codepoint <= 0x10ffff))
		{
			if (codepoint > 0xffff)
			{
				result->append(0xd8 + (codepoint>>18)); // MSB
				result->append(0xff & (codepoint>>10)); // LSB
				result->append(0xdc + ((codepoint>>8)&0x03)); // MSB
				result->append(0xff & codepoint); // LSB
			}
			else if (codepoint < 0xd800 || codepoint >= 0xe000)
			{
				result->append(codepoint>>8);
				result->append(codepoint&0xff);
			}
		}
	}
	return result;
}


/**
	FormField has no api to access children fields.
	This is a kludge to access protected fields of
	FormField throught static methods of a derived
	class.
*/
class FormFieldHack : public FormField
{
public:
	static int getNumChildrenFields(FormField * field)
	{
		FormFieldHack * hack = static_cast<FormFieldHack*>(field);
		if (hack->terminal) return 0; // numChildren are widgets
		return hack->numChildren;
	}
	static FormField * getChildField(FormField * field, int i)
	{
		FormFieldHack * hack = static_cast<FormFieldHack*>(field);
		return hack->children[i];
	}
};


int extract(FormFieldText * field, YAML::Emitter & out )
{
	const GooString * content = field->getContent();
	if (field->isMultiline())
		out << YAML::Literal;
	out << YAML::Value << pdftext_2_utf8(content);
	if (showTypes)
		out << YAML::Comment(typeStrings[field->getType()]);
	return 0;
}


// TODO: Test
int extractMultiple(FormFieldChoice * field, YAML::Emitter & out)
{
	const GooString * content = field->getSelectedChoice();
	// TODO: content can be NULL
	out << YAML::BeginSeq;
	for (unsigned i = 0; i<field->getNumChoices(); i++)
	{
		if (not field->isSelected(i)) continue;
		out << YAML::Value << pdftext_2_utf8(field->getChoice(i));
	}
	out << YAML::EndSeq;
	if (showTypes)
	{
		std::ostringstream os;
		os << typeStrings[field->getType()] << ": ";
		for (unsigned i = 0; i<field->getNumChoices(); i++)
			os << (i?", ":"") << pdftext_2_utf8(field->getChoice(i));
		out << YAML::Comment(os.str());
	}
	return 0;
}

int extractSingle(FormFieldChoice * field, YAML::Emitter & out)
{
	const GooString * content = field->getSelectedChoice();
	// TODO: content can be NULL
	out << YAML::Value << pdftext_2_utf8(content);
	if (showTypes)
	{
		std::ostringstream os;
		os << typeStrings[field->getType()] << ": ";
		for (unsigned i = 0; i<field->getNumChoices(); i++)
			os << (i?", ":"") << pdftext_2_utf8(field->getChoice(i));
		out << YAML::Comment(os.str());
	}
	return 0;
}

int extract(FormFieldChoice * field, YAML::Emitter & out)
{
	if (field->isMultiSelect())
		return extractMultiple(field, out);
	return extractSingle(field, out);
}

int extract(FormFieldButton * field, YAML::Emitter & out)
{
	FormButtonType type = field->getButtonType();
	if (type == formButtonPush) {
		out << YAML::Value << YAML::Null;
		return error("Push button ignored");
	}
	out
		<< YAML::Value << not field->getState((char*)"Off");
	if (showTypes)
		out << YAML::Comment(buttonTypeStrings[type]);
	// TODO: Add to the comment available onValues in childs
	return 0;
}

void extractField(FormField * field, YAML::Emitter & out)
{
	FormFieldType type = field->getType();
	std::string fieldName = pdftext_2_utf8(
		field->getPartialName());
	std::string alternateName = pdftext_2_utf8(
		field->getAlternateUiName());
	std::string mappingName = pdftext_2_utf8(
		field->getMappingName());
	out << YAML::Key << fieldName;
	if (!mappingName.empty() && mappingName!=fieldName)
		out << YAML::Comment(mappingName);
	if (!alternateName.empty() && alternateName!=fieldName)
		out << YAML::Comment(alternateName);
	int nChildren = FormFieldHack::getNumChildrenFields(field);
	if (nChildren)
	{
		out << YAML::BeginMap;
		for (unsigned i = 0; i < nChildren; i++)
		{
			FormField *subfield = FormFieldHack::getChildField(field, i);
			extractField(subfield, out);
		}
		out << YAML::EndMap;
		return;
	}
	if (type == formText)
	{
		FormFieldText * textField = dynamic_cast<FormFieldText*>(field);
		extract(textField, out);
		return;
	}
	if (type == formChoice)
	{
		FormFieldChoice * choiceField = dynamic_cast<FormFieldChoice*>(field);
		extract(choiceField, out);
		return;
	}
	if (type == formButton)
	{
		FormFieldButton * buttonField = dynamic_cast<FormFieldButton*>(field);
		extract(buttonField, out);
		return;
	}
	error("Unreconigzed type extracting '" +
		pdftext_2_utf8(field->getFullyQualifiedName()) + "'" );
}


int extractYamlFromPdf(Form * form, std::ostream & outputfile)
{
	YAML::Emitter out(outputfile);
	out << YAML::Comment("Generated by pdf-form-burner");
	out << YAML::BeginMap;
	for (unsigned i = 0; i < form->getNumFields(); i++)
	{
		FormField *field = form->getRootField(i);
		extractField(field, out);
	}
	out << YAML::EndMap;
	return 0;
}

// TODO: Test this
int fillMultiple(FormFieldChoice * field, const YAML::Node & node)
{
	if (not node.IsSequence())
		return error("Sequence required for field "+
			pdftext_2_utf8(field->getFullyQualifiedName()));
	std::set<std::string> values;
	for (YAML::Node::const_iterator e=node.begin(); e != node.end(); e++ )
		values.insert(e->as<std::string>());

	field->deselectAll();

	for (unsigned i = 0; i<field->getNumChoices(); i++)
	{
		const GooString * choiceText = field->getChoice(i);
		std::string value = pdftext_2_utf8(choiceText);
		if (values.find(value)==values.end()) continue;
		field->select(i);
		values.erase(value);
	}
	if (values.empty()) return 0;
	return error("Multiple choice value not supported in field '"+
			pdftext_2_utf8(field->getFullyQualifiedName()));
	return 0;
}

int fillSingle(FormFieldChoice * field, const YAML::Node & node)
{
	if (not node.IsScalar())
		return error("String required for field "+
			pdftext_2_utf8(field->getFullyQualifiedName()));

	std::string value = node.as<std::string>();
	for (unsigned i = 0; i<field->getNumChoices(); i++)
	{
		const GooString * choiceText = field->getChoice(i);
		if (pdftext_2_utf8(choiceText)!=value) continue;
		field->select(i);
		return 0;
	}
	if (not field->hasEdit())
		return error("Invalid value for field "+
			pdftext_2_utf8(field->getFullyQualifiedName())+" '"+value+"'");

	// Editable choice field can have arbitrary values
	const GooString * content = utf8_2_pdftext(value);
	field->setEditChoice(content);
	if (content) delete content;
	return 0;
}

int fill(FormFieldChoice * field, const YAML::Node & node)
{
	if (field->isMultiSelect())
		return fillMultiple(field, node);

	return fillSingle(field, node);
}

int fill(FormFieldText * field, const YAML::Node & node)
{
	if (not node.IsScalar())
		return error("String required for field "+
			pdftext_2_utf8(field->getFullyQualifiedName()));
	std::string value = node.as<std::string>();
	const GooString * content = utf8_2_pdftext(value);
	field->setContentCopy(content);
	if (content) delete content;
	return 0;
}

int fill(FormFieldButton * field, const YAML::Node & node)
{
	if (not node.IsScalar())
		return error("Boolean value required for field "+
			pdftext_2_utf8(field->getFullyQualifiedName()));
	bool value = node.as<bool>();
	// TODO: True value should be taken from the field
	FormWidget * widget = field->getWidget(0);
	FormWidgetButton * button = dynamic_cast<FormWidgetButton*>(widget);
	if (not button)
		return error("Unable to set truth value to "+
			pdftext_2_utf8(field->getFullyQualifiedName()));
	button->setState(value);
	return 0;
}

int fillField(FormField * field, const YAML::Node & node)
{
	FormFieldType type = field->getType();
	std::string fieldName = pdftext_2_utf8(
		field->getPartialName());
	if (not node[fieldName])
		return error("Missing yaml field '" + fieldName + "'");

	const YAML::Node & theNode = node[fieldName];

	unsigned nChildren = FormFieldHack::getNumChildrenFields(field);
	if (nChildren)
	{
		for (unsigned i = 0; i < nChildren; i++)
		{
			FormField *subfield = FormFieldHack::getChildField(field, i);
			int error = fillField(subfield, theNode);
			if (error) return error;
		}
		return 0;
	}
	if (type == formText)
	{
		FormFieldText * textField = dynamic_cast<FormFieldText*>(field);
		return fill(textField, theNode);
	}
	if (type == formChoice)
	{
		FormFieldChoice * choiceField = dynamic_cast<FormFieldChoice*>(field);
		return fill(choiceField, theNode);
	}
	if (type == formButton)
	{
		FormFieldButton * buttonField = dynamic_cast<FormFieldButton*>(field);
		return fill(buttonField, theNode);
	}
	return error("Unreconigzed type filling '" +
		pdftext_2_utf8(field->getFullyQualifiedName()) + "'");
}

int fillPdfWithYaml(Form * form, const YAML::Node & node)
{
	if (not node.IsMap()) return error("YAML root node should be a map");
	for (unsigned i = 0; i < form->getNumFields(); i++)
	{
		FormField *field = form->getRootField(i);
		int error = fillField(field, node);
		if (error) return error;
	}
	return 0;
}



int main(int argc, char** argv)
{
	if (argc<2) return usage(argv[0]);
	const char * yamlFile = argc<3?0:argv[2];
	const char * outputPdf = argc<4?0:argv[3];

	globalParams = new GlobalParams;
	autodelete<GlobalParams> _globalParams(globalParams);

	globalParams->setTextEncoding(textEncName);

	uMap = globalParams->getTextEncoding();
	if (not uMap) return error("Couldn't get text encoding");

	GooString pdfFileName(argv[1]);

	PDFDoc * doc = PDFDocFactory().createPDFDoc(pdfFileName);
	if (not doc) return error("Unable to open document");
	autodelete<PDFDoc> _doc(doc);

	Catalog::FormType formType = doc->getCatalog()->getFormType();

	if (formType == Catalog::NoForm)
		return error("PDF has no form");

	if (formType != Catalog::AcroForm)
		return error("PDF form format not supported");

	Form *form = doc->getCatalog()->getForm();

	if (outputPdf)
	{
		bool usingStdIn = yamlFile==std::string("-");
		YAML::Node node = usingStdIn ? YAML::Load(std::cin) : YAML::LoadFile(yamlFile);
		fillPdfWithYaml(form, node);
//		extractYamlFromPdf(form, std::cout); // Debug
		GooString * outputFilename = new GooString(outputPdf);
		autodelete<GooString> _outputFilename(outputFilename);
		doc->saveAs(outputFilename);
	}
	else if (yamlFile)
	{
		std::ofstream output(yamlFile);
		extractYamlFromPdf(form, output);
	}
	else
	{
		extractYamlFromPdf(form, std::cout);
	}

	return 0;
}


