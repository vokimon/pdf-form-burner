#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDateTime>
#include <iostream>
#include <poppler-qt5.h>
#include <poppler-form.h>
#include <memory>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <fmt/core.h>
#include <fmt/ostream.h>

static std::ostream & operator<< (std::ostream & os, const QString & string)
{
	return os << string.toStdString();
}
YAML::Emitter& operator << (YAML::Emitter& out, const QString & string)
{
	return out << string.toStdString();
}

template<typename ...Args>
static void colorize(std::ostream & os, const std::string color, const std::string prefix, const std::string & message, Args ... args) { 
	os
		<< "\033[" << color << "m" << prefix
		<< fmt::format(message, args...)
		<< "\033[0m"
		<< std::endl;
}

template<typename ...Args>
static void error(const std::string & message, Args ... args) {
	colorize(std::cerr, "31;1", "ERROR: ", message, args...);
}
template<typename ...Args>
static bool fail(const std::string & message, Args ... args) {
	error(message, args...);
	std::exit(-1);
	return false;
}
template<typename ...Args>
static void warn(const std::string & message, Args ... args) {
	colorize(std::cerr, "33", "Warning: ", message, args...);
}
template<typename ...Args>
static void step(const std::string & message, Args ... args) {
	colorize(std::cerr, "34", "== ", message, args...);
}
template<typename ...Args>
static void stage(const std::string & message, Args ... args) {
	colorize(std::cerr, "34;1", "== ", message, args...);
}

#define BEGIN_ENUM(NS, TYPE) \
std::ostream & operator << (std::ostream & os, NS::TYPE value) {\
	typedef NS host; \
	switch(value) {
#define ENUM_VALUE(VAL) case(host::VAL): return os << #VAL;
#define END_ENUM \
	default: return os << "Invalid"; \
	}\
}


BEGIN_ENUM(Poppler::FormField, FormType)
	ENUM_VALUE(FormButton)
	ENUM_VALUE(FormText)
	ENUM_VALUE(FormChoice)
	ENUM_VALUE(FormSignature)
END_ENUM

BEGIN_ENUM(Poppler::FormFieldButton, ButtonType)
	ENUM_VALUE(Push)
	ENUM_VALUE(CheckBox)
	ENUM_VALUE(Radio)
END_ENUM

BEGIN_ENUM(Poppler::FormFieldText, TextType)
	ENUM_VALUE(Normal)
	ENUM_VALUE(Multiline)
	ENUM_VALUE(FileSelect)
END_ENUM
  
BEGIN_ENUM(Poppler::SignatureValidationInfo, SignatureStatus)
	ENUM_VALUE(SignatureValid)
	ENUM_VALUE(SignatureInvalid)
	ENUM_VALUE(SignatureDigestMismatch)
	ENUM_VALUE(SignatureDecodingError)
	ENUM_VALUE(SignatureGenericError)
	ENUM_VALUE(SignatureNotFound)
	ENUM_VALUE(SignatureNotVerified)
END_ENUM

QString translate(const char * text) {
	QCoreApplication & app = *QCoreApplication::instance();
	return app.translate("main", text);
}


void dump(Poppler::FormFieldButton * field, YAML::Emitter & out) {
	switch (field->buttonType()) {
		case Poppler::FormFieldButton::CheckBox:
		case Poppler::FormFieldButton::Radio:
			out << bool(field->state());
			return;
		case Poppler::FormFieldButton::Push:
			out << YAML::Null;
			warn("Push button ignored '{}'",
				field->fullyQualifiedName());
	}
}
void dump(Poppler::FormFieldText * field, YAML::Emitter & out) {
	switch (field->textType()) {
		case Poppler::FormFieldText::FileSelect:
			warn("File select not fully supported, managed as simple text, field {}",
				field->fullyQualifiedName());
			out << field->text();
			return;
		case Poppler::FormFieldText::Multiline:
			out << YAML::Literal << field->text() << YAML::Newline;
			return;
		case Poppler::FormFieldText::Normal:
			out << field->text();
			return;
	}
}

void dump(Poppler::FormFieldChoice * field, YAML::Emitter & out) {
	auto choices = field->choices();
	auto currentChoices = field->currentChoices();
	if (field->multiSelect()) {
		out << YAML::BeginSeq;
		for (auto i: currentChoices) {
			out << choices[i];
		}
		out << YAML::EndSeq;
	}
	else if (field->isEditable() and not field->editChoice().isNull()) {
		out << field->editChoice();
	}
	else {
		out << (currentChoices.empty()?"":choices[currentChoices.first()]);
	}
	out << YAML::Comment(translate("%1 values: %2")
		.arg(field->isEditable()?translate("Suggested"):translate("Allowed"))
		.arg(choices.join(QStringLiteral(", ")))
		.toStdString()
		);
}

void dump(Poppler::FormFieldSignature * field, YAML::Emitter & out) {
	auto info = field->validate(
		Poppler::FormFieldSignature::ValidateVerifyCertificate);
	out << YAML::BeginMap;
	out
		<< YAML::Key << "status"
		<< YAML::Value << info.signatureStatus()
		<< YAML::Key << "signer"
		<< YAML::Value << info.signerName()
		<< YAML::Key << "time"
		<< YAML::Value << QDateTime::fromMSecsSinceEpoch(
				info.signingTime(), Qt::UTC)
			.toString(Qt::ISODate)
		<< YAML::Key << "location"
		<< YAML::Value << info.location()
		<< YAML::Key << "reason"
		<< YAML::Value << info.reason()
		<< YAML::Key << "scope"
		<< YAML::Value << (info.signsTotalDocument()?"Total":"Partial")
	;
	out << YAML::EndMap;
}


class FieldTree {
public:
	FieldTree(Poppler::FormField * field=nullptr) : _field(field) {}
	void add(const QString & fullName, Poppler::FormField * field) {
		int dotPos = fullName.indexOf('.');
		if (dotPos == -1) {
			if (_children.contains(fullName))
				warn("Overwriting existing field '{}', '{}'",
					fullName, field->name());
			_children[fullName] = FieldTree(field);
			return;
		}
		QString levelName = fullName.left(dotPos);
		QString remaining = fullName.mid(dotPos+1);
		if (!_children.contains(levelName)) {
			_children[levelName] = FieldTree();
		}
		FieldTree & level = _children[levelName];
		level.add(remaining, field);
	}
	void extract(YAML::Emitter & out) {
		if (_field) extractField(out);
		else extractChildren(out);
	}

	void extractChildren(YAML::Emitter & out) {
		out << YAML::BeginMap;
		for (auto key : _children.keys()) {
			out << YAML::Key << key << YAML::Value;
			_children[key].extract(out);
		}
		out << YAML::EndMap;
	}

	void extractField(YAML::Emitter & out) {
		switch (_field->type()) {
			case Poppler::FormField::FormButton:
				dump(dynamic_cast<Poppler::FormFieldButton*>(_field), out);
				break;
			case Poppler::FormField::FormText:
				dump(dynamic_cast<Poppler::FormFieldText*>(_field), out);
				break;
			case Poppler::FormField::FormChoice:
				dump(dynamic_cast<Poppler::FormFieldChoice*>(_field), out);
				break;
			case Poppler::FormField::FormSignature:
				dump(dynamic_cast<Poppler::FormFieldSignature*>(_field), out);
				break;
		}
		QString comment = translate("%1%2")
			.arg(_field->name()!=_field->uiName()?_field->uiName():QString())
			.arg(_field->isReadOnly()?" [Read Only]":"")
			;
		if (!comment.isEmpty())
			out << YAML::Comment(comment.toStdString());
	}


	void fill(Poppler::FormField * field, const YAML::Node & node) {
		error("Unsupported field {} of type '{}'",
			field->fullyQualifiedName(),
			field->type());
	}

	void fill(Poppler::FormFieldText * field, const YAML::Node & node) {
		if (not node.IsScalar()) {
			error("String required for field '{}'",
				field->fullyQualifiedName());
		}
		field->setText(node.as<std::string>().c_str());
	}

	void fill(Poppler::FormFieldButton * field, const YAML::Node & node) {
		switch (field->buttonType()) {
			case Poppler::FormFieldButton::CheckBox:
			case Poppler::FormFieldButton::Radio:
			{
				if (not node.IsScalar()) {
					error("Boolean value required for field '{}'",
						field->fullyQualifiedName());
					return;
				}
				bool value = node.as<bool>();
				field->setState(value);
				return;
			}
			case Poppler::FormFieldButton::Push:
				warn("Push button ignored '{}'", field->fullyQualifiedName());
		}
	}

	void fill(Poppler::FormFieldChoice * field, const YAML::Node & node) {
		auto choices = field->choices();
		if (field->multiSelect()) {
			if (not node.IsSequence()) {
				error("Sequence required for field '{}'",
					field->fullyQualifiedName());
			}
			QList<int> selection;
			for (auto subnode: node) {
				if (not subnode.IsScalar()) {
					error("Sequence of scalars values required for field '{}'",
						field->fullyQualifiedName());
					return;
				}
				std::string value = subnode.as<std::string>();
				int selected = choices.indexOf(value.c_str());
				if (selected==-1) {
					error("Illegal value '{}' for field '{}' try with {}",
						value, field->fullyQualifiedName(),
						choices.join(", "));
					return;
				}
				selection.append(selected);
			}
			field->setCurrentChoices(selection);
			return;
		}
		if (not node.IsScalar()) {
			error("Scalar value required for field '{}'",
				field->fullyQualifiedName());
			return;
		}
		std::string value = node.as<std::string>();

		int selected = choices.indexOf(value.c_str());
		if (selected==-1 and field->isEditable()) {
			field->setEditChoice(value.c_str());
			return;
		}
		if (selected==-1) {
			error("Illegal value '{}' for field '{}' try with {}",
				value, field->fullyQualifiedName(),
				choices.join(", "));
			return;
		}
		QList<int> selection;
		selection.append(selected);
		field->setCurrentChoices(selection);
	}

	void fillChildren(const YAML::Node & node) {
		for (auto key : _children.keys()) {
			const YAML::Node & subnode = node[key.toStdString()];
			_children[key].fill(subnode);
		}
	}
	void fillField(const YAML::Node & node) {
		if (!_field) return;
		switch (_field->type()) {
			case Poppler::FormField::FormButton:
				fill(dynamic_cast<Poppler::FormFieldButton*>(_field), node);
				break;
			case Poppler::FormField::FormText:
				fill(dynamic_cast<Poppler::FormFieldText*>(_field), node);
				break;
			case Poppler::FormField::FormChoice:
				fill(dynamic_cast<Poppler::FormFieldChoice*>(_field), node);
				break;
			case Poppler::FormField::FormSignature:
				fill(dynamic_cast<Poppler::FormFieldSignature*>(_field), node);
				break;
		}
	}
	void fill(const YAML::Node & node) {
		if (_field) fillField(node);
		else fillChildren(node);
	}
private:
	QMap<QString, FieldTree> _children;
	Poppler::FormField * _field;
};

int extractYamlFromPdf(FieldTree & fields, std::ostream & outputfile)
{
	YAML::Emitter out(outputfile);
	out << YAML::Comment("Generated by pdf-form-burner");
	fields.extract(out);
	out << YAML::Newline;
	return 0;
}

void fillPdfWithYaml(FieldTree & fields, std::istream & yamlfile)
{
	YAML::Node node = YAML::Load(yamlfile);
	fields.fill(node);
}


int main(int argc, char**argv)
{
	QCoreApplication app(argc, argv);
	app.setApplicationName("pdfformburner");
	app.setOrganizationName("KEEPerians UNLTD");
	app.setOrganizationDomain("kkep.org");
	app.setApplicationVersion("2.0");
	QCommandLineParser parser;
	parser.setApplicationDescription(translate(
		"Extracts and fills PDF form data by means of YAML files"));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("input.pdf",
		translate("PDF file to load"));
	parser.addPositionalArgument("data.yaml",
		translate("YAML file with the form data to write/read. Use a hyphen to use stdin/stdout"));
	parser.addPositionalArgument("output.pdf",
		translate("Filled PDF file. If provided, activates the fill mode and uses the YAML as input"), "[output.pdf]");
	parser.process(app);
	auto arguments = parser.positionalArguments();

	if (arguments.length()<1) {
		parser.showHelp(-1);
	}
	auto inputpdf = arguments[0];

	stage("Loading {}", inputpdf);
	auto document = std::unique_ptr<Poppler::Document>(Poppler::Document::load(inputpdf));

	document or fail("Unable to open the document");
	document->isLocked() and fail("Locked pdf");

	stage("Looking for form fields");
	FieldTree fieldTree;
	for (unsigned page=0; true; page++) {
		Poppler::Page* pdfPage = document->page(page);  // Document starts at page 0
		if (pdfPage == 0) break;
		auto fields = pdfPage->formFields();
		for (auto field : fields) {
			fieldTree.add(field->fullyQualifiedName(), field);
		}
	}
	switch (arguments.length()) {
		case 1: {
			extractYamlFromPdf(fieldTree, std::cout);
		}
		break;
		case 2: {
			std::ofstream outyaml(arguments[1].toStdString().c_str());
			extractYamlFromPdf(fieldTree, outyaml);
		}
		break;
		case 3: {
			if (arguments[1]=='-') {
				fillPdfWithYaml(fieldTree, std::cin);
			}
			else {
				std::ifstream inyaml(arguments[1].toStdString().c_str());
				fillPdfWithYaml(fieldTree, inyaml);
			}
			stage("Saving filled pdf as {}", arguments[2]);
			auto converter = std::unique_ptr<Poppler::PDFConverter>(document->pdfConverter());
			converter->setOutputFileName(arguments[2]);
			converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
			converter->convert()
				or fail("Error saving file {}", arguments[2]);
		}
	}
	return 0;
}

