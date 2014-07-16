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

int usage(const char * programName)
{
	std::cerr
		<< "Usage:" << std::endl
		<< "\t" << programName << " <file.pdf>" << std::endl
		<< "\t\tDumps the form field information" << std::endl
		<< "\t" << programName << " <file.pdf> <output.yaml>" << std::endl
		<< "\t\tDumps the form field content as yaml" << std::endl
		<< "\t" << programName << " <file.pdf> <input.yaml> <output.pdf>" << std::endl
		<< "\t\tUses the yaml file to fill file.pdf into output.pdf" << std::endl
		;
	return -1;
}

int error(const char * message, int code=-1)
{
	std::cerr << message << std::endl;
	return code;
}

std::string pdftext_2_utf8(GooString *s)
{
	if (!s) return "";
	if (!s->getCString()) return "";

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


int dumpPdfAsYaml(Form * form, std::ostream & outputfile)
{
	YAML::Emitter out(outputfile);
	out << YAML::BeginMap;
	for (unsigned i = 0; i < form->getNumFields(); i++)
	{
		FormField *field = form->getRootField(i);
		FormFieldType type = field->getType();
		std::string fieldName = pdftext_2_utf8(
			field->getFullyQualifiedName());
		out << YAML::Key << fieldName;
		if (type == formText)
		{
			FormFieldText * textField = dynamic_cast<FormFieldText*>(field);
			GooString * content = textField->getContent();
			out << YAML::Value << pdftext_2_utf8(content);
			out << YAML::Comment(typeStrings[type]);
			continue;
		}
		if (type == formChoice)
		{
			FormFieldChoice * choiceField = dynamic_cast<FormFieldChoice*>(field);
			GooString * content = choiceField->getSelectedChoice();
			// TODO: content can be NULL
			out << YAML::Value << pdftext_2_utf8(content);
			std::ostringstream os;
			os << typeStrings[type] << ": ";
			for (unsigned i = 0; i<choiceField->getNumChoices(); i++)
				os << (i?", ":"") << pdftext_2_utf8(choiceField->getChoice(i));
			out << YAML::Comment(os.str());
			continue;
		}
		{
			out << YAML::Value << YAML::Null;
			out << YAML::Comment(
				"Ignored  Field: '" + fieldName + "' Type: " + typeStrings[type])
				;
			std::cerr
				<< "Ignored  Field: '" << fieldName
				<< "' Type: " << typeStrings[type]
				<< std::endl;
		}
	}
	out << YAML::EndMap;
	return 0;
}

int editPdfWithYaml(Form * form, YAML::Node & node)
{
	if (not node.IsMap()) return error("YAML root node should be a map");
	for (unsigned i = 0; i < form->getNumFields(); i++)
	{
		FormField *field = form->getRootField(i);
		FormFieldType type = field->getType();
		std::string fieldName = pdftext_2_utf8(
			field->getFullyQualifiedName());
		if (not node[fieldName])
		{
			std::cerr
				<< "Field '" << fieldName
				<< "' missing at the yaml"
				<< std::endl;
			continue;
		}

		if (type == formText)
		{
			std::string value = node[fieldName].as<std::string>();
			FormFieldText * textField = dynamic_cast<FormFieldText*>(field);
			GooString * content = utf8_2_pdftext(value);
			textField->setContentCopy(content);
			if (content) delete content;
			continue;
		}
		if (type == formChoice)
		{
			std::string value = node[fieldName].as<std::string>();
			FormFieldChoice * choiceField = dynamic_cast<FormFieldChoice*>(field);
			for (unsigned i = 0; i<choiceField->getNumChoices(); i++)
			{
				GooString * choiceText = choiceField->getChoice(i);
				if (pdftext_2_utf8(choiceText)!=value) continue;
				choiceField->select(i);
				break;
			}
			// TODO: Just set the value if editable
			// TODO: Choice not found error
			continue;
		}
		{
			std::cerr
				<< "Ignored  Field: '" << fieldName
				<< "' Type: " << typeStrings[type]
				<< std::endl;
		}
	}
	return 0;
}


template <typename T>
class autodelete
{
public:
	autodelete(T*o) : _o(o) {}
	~autodelete() { delete _o;}
private:
	T * _o;
};


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
		YAML::Node node = YAML::LoadFile(argv[2]);
		editPdfWithYaml(form, node);
		dumpPdfAsYaml(form, std::cout);
		GooString * outputFilename = new GooString(outputPdf);
		doc->saveAs(outputFilename);
		delete outputFilename;
	}
	else if (yamlFile)
	{
		std::ofstream output(argv[2]);
		dumpPdfAsYaml(form, output);
	}
	else
	{
		dumpPdfAsYaml(form, std::cout);
	}

	return 0;
}


