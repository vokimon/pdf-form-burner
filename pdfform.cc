#include <poppler-config.h>
#include <poppler/PDFDocFactory.h>
#include <poppler/Page.h>
#include <poppler/Form.h>
#include <poppler/GlobalParams.h>
#include <poppler/UnicodeMap.h>
#include <poppler/UTF.h>
#include <poppler/Dict.h>

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

std::string encode(GooString *s)
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
	delete u;
	return output;
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
	for (unsigned i = 0; i < form->getNumFields(); i++)
	{
		FormField *field = form->getRootField(i);
		GooString *qualifiedName = field->getFullyQualifiedName();
		FormFieldType type = field->getType();
		std::cout
			<< "Field: '" << qualifiedName->getCString() << "'\n"
			<<"\tType: " << typeStrings[type] << std::endl;
		if (type == formText)
		{
			FormFieldText * textField = dynamic_cast<FormFieldText*>(field);
			GooString * content = textField->getContentCopy();
			if (content)
			{
				std::cout << "\tContent: '" << encode(content).c_str() << "'" << std::endl;
				delete content;
			}

			const char value[] = "\xfe\xff\0T\0o\0m\0a\0";
			GooString *newValue = new GooString(value,sizeof(value));
			textField->setContentCopy(newValue);
			delete newValue;


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


