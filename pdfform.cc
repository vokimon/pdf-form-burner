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
//				result->append(static_cast<wchar_t>(0xd800 + (codepoint >> 10),1));
//				result->append(static_cast<wchar_t>(0xdc00 + (codepoint & 0x03ff)),1);

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
			out << YAML::Value << (content?pdftext_2_utf8(content):"");
			out << YAML::Comment(typeStrings[type]);
			continue;
		}
		if (type == formChoice)
		{
			FormFieldChoice * choiceField = dynamic_cast<FormFieldChoice*>(field);
			GooString * content = choiceField->getSelectedChoice();
			out << YAML::Value << (content?pdftext_2_utf8(content):"");
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


int main(int argc, char** argv)
{
	if (argc<2) return usage(argv[0]);

	globalParams = new GlobalParams;
	globalParams->setTextEncoding(textEncName);

	uMap = globalParams->getTextEncoding();
	if (not uMap) return error("Couldn't get text encoding");

	GooString pdfFileName(argv[1]);

	PDFDoc * doc = PDFDocFactory().createPDFDoc(pdfFileName);

	if (not doc) return error("Unable to open document");

	// TODO: check doc
	unsigned nEmbedded = doc->getCatalog()->numEmbeddedFiles();
	std::cout << "Embedded Files: " << nEmbedded << std::endl;
	for (unsigned i = 0; i < nEmbedded; ++i)
		std::cout << "Embeded " << i << std::endl;

	Catalog::FormType formType = doc->getCatalog()->getFormType();

	if (formType == Catalog::NoForm)
		return error("PDF has no form");

	if (formType != Catalog::AcroForm)
		return error("PDF form format not supported");

	Form *form = doc->getCatalog()->getForm();

	dumpPdfAsYaml(form, std::cout);

	for (unsigned i = 0; false && i < form->getNumFields(); i++)
	{
		FormField *field = form->getRootField(i);
		GooString *qualifiedName = field->getFullyQualifiedName();
		FormFieldType type = field->getType();
		if (type == formText)
		{
			// TODO: Escape name
			FormFieldText * textField = dynamic_cast<FormFieldText*>(field);
			GooString * content = textField->getContentCopy();
			std::cout
				<< qualifiedName->getCString() << ": "
				;
			if (content)
			{
				std::cout
					<< "'" << pdftext_2_utf8(content).c_str() << "'"
					;
				delete content;
			}
			std::cout
				<< " # " << typeStrings[type]
				<< std::endl;

			GooString *newValue = utf8_2_pdftext("toooomá ñacata");

			textField->setContentCopy(newValue);
			delete newValue;
		}
		else
		{
			std::cout
				<< "Field: '" << qualifiedName->getCString() << "'\n"
				<<"\tType: " << typeStrings[type] << std::endl;
		}
	}
	GooString * outputFilename = new GooString(argv[2]);
	doc->saveAs(outputFilename);
	delete outputFilename;
	std::cerr << "Saved!" << std::endl;

//	uMap->decRefCnt();
	delete doc;
	delete globalParams;

	return 0;
}


